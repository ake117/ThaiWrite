/* =====================================================================
 * shim_dos.c - DOS/BIOS software emulation for the ThaiWrite Linux port
 * ---------------------------------------------------------------------
 * There is no real-mode BIOS in a Linux process, so the shim keeps a
 * small software "PC" and answers the interrupts that CU-Writer uses.
 * It also maps the legacy video windows (0xA0000, and the "seg<<16"
 * bases 0xA0000000 / 0xB0000000 used by SRC/SCUF/WRITE.C) so that the
 * original direct VRAM writes land in memory the renderer can read
 * instead of faulting.
 * ===================================================================== */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#include "dos.h"

/* ---------------------------------------------------------------- */
/*  Software PC state                                               */
/* ---------------------------------------------------------------- */
static unsigned char  shim_ports[0x10000];       /* I/O port space       */
static unsigned char *shim_realmem_ptr;          /* 1 MB real-mode space */
static unsigned char *shim_vram_ptr;             /* 0xA0000 window       */
static unsigned char *shim_indos_page;           /* 0x10000 InDOS page   */
static unsigned char *shim_vram_text_ptr;        /* 0xB8000 text page    */
static unsigned char *shim_vram_herc_ptr;        /* 0xB0000000 window    */
static unsigned char *shim_vram_ega_ptr;         /* 0xA0000000 window    */
static int  shim_video_mapped;                   /* 1 = fixed maps OK    */
static int  shim_mode = 3;                       /* video mode           */
static unsigned char shim_v_ega_bl = 1;          /* 128K, so VGA is seen */
static unsigned char shim_v_ega_bh = 0;
static unsigned char shim_v_ega_cl = 2;          /* VGA-ish configuration */

static shim_isr shim_vectors[256];
static shim_isr shim_idle_hook;

/* INT 21h/35h has to hand the caller a real-mode ES:BX "address" for an
 * interrupt vector.  There is no real-mode address space in this port and
 * a 64-bit C function pointer would not survive the round trip, so the
 * shim hands out a small nonzero *token* per interrupt number instead:
 * 35h splits the token into ES:BX and 25h resolves it back to the ISR
 * that was registered for that interrupt.  The token is deliberately not
 * a callable address; port code must use getvect()/setvect() to reach a
 * handler. */
static void shim_noop_isr(void);

static unsigned short shim_vec_token[256];
static unsigned short shim_next_token = 1;

static unsigned short shim_token_for(int intrno)
{
    if (shim_vec_token[intrno & 0xFF] == 0)
    {
        shim_vec_token[intrno & 0xFF] = shim_next_token++;
        if (shim_next_token == 0)
        {
            shim_next_token = 1;
        }
    }
    return shim_vec_token[intrno & 0xFF];
}

static shim_isr shim_isr_for_token(unsigned long token)
{
    int i;

    for (i = 0; i < 256; i++)
    {
        if (shim_vec_token[i] != 0 && (unsigned long)shim_vec_token[i] == token)
        {
            return shim_vectors[i];
        }
    }
    return shim_noop_isr;
}

static void shim_noop_isr(void) { }

static unsigned char shim_vram_fallback[SHIM_VRAM_SIZE];

unsigned char *shim_realmem(void)
{
    if (shim_realmem_ptr == NULL)
    {
        shim_realmem_ptr = (unsigned char *)calloc(1, 0x100000);
        if (shim_realmem_ptr == NULL)
        {
            shim_realmem_ptr = (unsigned char *)mmap(NULL, 0x100000,
                PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
    }
    return shim_realmem_ptr;
}

/* Map "addr" (page aligned) read/write, fixed.  Returns the pointer or
 * NULL on failure. */
static unsigned char *shim_map_fixed(unsigned long addr, size_t len)
{
    void *p = mmap((void *)addr, len, PROT_READ | PROT_WRITE,
                   MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED)
    {
        return NULL;
    }
    return (unsigned char *)p;
}

__attribute__((constructor))
static void shim_dos_init(void)
{
    int i;

    for (i = 0; i < 256; i++)
    {
        shim_vectors[i] = shim_noop_isr;
    }

    /* One page at real-mode address 0x10000 (segment 0x1000) that stands in
     * for the low-memory area DOS keeps the "InDOS" flag in.  INT 21h/34h
     * hands out its address as ES:BX, so this gives callers a non-null,
     * safely dereferenceable pointer that always reads 0, i.e. "DOS is not
     * busy", which is the truth about the shim. */
    shim_indos_page = shim_map_fixed(0x10000UL, 0x1000UL);

    /* 0xA0000 .. 0x100000: VGA graphics planes, VGA text page (0xB8000)
     * and the 0xC0000 area GRDETECT.C probes. */
    shim_vram_ptr = shim_map_fixed(0xA0000UL, 0x60000UL);
    if (shim_vram_ptr != NULL)
    {
        shim_vram_text_ptr = shim_vram_ptr + 0x18000;   /* 0xB8000 */
    }
    else
    {
        shim_vram_ptr = shim_vram_fallback;
        shim_vram_text_ptr = shim_vram_fallback + 0x18000;
    }

    /* "seg<<16" bases used by SRC/SCUF/WRITE.C's hgcwritech()/ewritech().
     * These must be mapped at their *real* addresses: the driver code
     * builds raw far pointers such as (char far *)0xb0000000L, so the
     * only way those writes can land in the shim's framebuffer is for
     * the mapping to be installed at that exact address. */
    shim_video_mapped = 0;
    shim_vram_herc_ptr = shim_map_fixed(0xB0000000UL, 0x10000UL);
    shim_vram_ega_ptr  = shim_map_fixed(0xA0000000UL, 0x40000UL);
    if (shim_vram_herc_ptr != NULL && shim_vram_ega_ptr != NULL)
    {
        shim_video_mapped = 1;
    }
    else
    {
        /* Best effort: give the renderer *something* to read.  Raw
         * far-pointer writes from SRC/SCUF/WRITE.C will fault in this
         * case, which shim_video_is_mapped() lets the port detect. */
        shim_vram_herc_ptr = shim_vram_fallback;
        shim_vram_ega_ptr  = shim_vram_fallback;
    }
}

unsigned char *shim_vram(void)          { return shim_vram_ptr; }
unsigned char *shim_vram_text(void)     { return shim_vram_text_ptr; }
unsigned char *shim_vram_herc(void)     { return shim_vram_herc_ptr; }
unsigned char *shim_vram_ega(void)      { return shim_vram_ega_ptr; }
int shim_video_is_mapped(void)          { return shim_video_mapped; }
int shim_video_mode(void)               { return shim_mode; }
void shim_set_video_mode(int mode)      { shim_mode = mode; }
void shim_set_ega_config(unsigned char bl, unsigned char bh, unsigned char cl)
{
    shim_v_ega_bl = bl;
    shim_v_ega_bh = bh;
    shim_v_ega_cl = cl;
}
unsigned char shim_ega_bl(void) { return shim_v_ega_bl; }
unsigned char shim_ega_bh(void) { return shim_v_ega_bh; }
unsigned char shim_ega_cl(void) { return shim_v_ega_cl; }

/* ---------------------------------------------------------------- */
/*  Interrupt dispatch                                              */
/* ---------------------------------------------------------------- */
static void shim_int10(union REGS *in, union REGS *out)
{
    unsigned char ah = in->h.ah;
    unsigned char al = in->h.al;

    switch (ah)
    {
        case 0x00:                                  /* set video mode   */
            shim_mode = al;
            break;
        case 0x0F:                                  /* get video mode   */
            out->h.al = (unsigned char)shim_mode;
            out->h.ah = 0;                          /* 80 columns       */
            break;
        case 0x1A:                                  /* video information */
            out->h.al = 0x1A;                       /* call supported   */
            out->h.bl = 7;                          /* VGA active display */
            out->h.bh = 0;                          /* color            */
            break;
        case 0x12:                                  /* EGA/VGA config   */
            if (in->h.bl == 0x10)
            {
                out->h.bh = shim_v_ega_bh;            /* controller mode  */
                out->h.bl = shim_v_ega_bl;            /* memory size      */
                out->h.cl = shim_v_ega_cl;            /* configuration    */
            }
            break;
        default:
            /* set mode variants used by CWEGRPH/CWAGRPH (0x11,0x10,0x0F,
             * 0x40,0x03) arrive as AX, not as AH!=0 */
            break;
    }

    /* "AX = 0x00xx" set-mode forms */
    switch (in->x.ax)
    {
        case 0x0011: shim_mode = 0x11; break;       /* 640x480 VGA      */
        case 0x0010: shim_mode = 0x10; break;       /* 640x350 EGA      */
        case 0x000F: shim_mode = 0x0F; break;       /* 640x350 mono     */
        case 0x0040: shim_mode = 0x40; break;       /* AT&T 400 line    */
        case 0x0003: shim_mode = 0x03; break;       /* text             */
        default: break;
    }
    _shim_AX = in->x.ax;
    _shim_BX = in->x.bx;
}

static void shim_int21(union REGS *in, union REGS *out)
{
    switch (in->h.ah)
    {
        case 0x30:                                  /* DOS version      */
            out->h.al = 3;
            out->h.ah = 30;                         /* "DOS 3.30"       */
            break;
        case 0x33:                                  /* Ctrl-Break flag  */
            if (in->h.al == 0)
            {
                out->h.dl = 1;                      /* break checking on */
            }
            else if (in->h.al == 1)
            {
                out->h.dl = 0;                      /* set break off    */
            }
            else if (in->h.al == 5)
            {
                out->h.dl = 3;                      /* boot drive       */
            }
            break;
        case 0x34:                                  /* InDOS flag       */
            /* ES:BX -> the emulated low-memory flag page (always 0). */
            out->x.bx = 0;                          /* offset           */
            _shim_ES = (shim_indos_page != NULL) ? 0x1000 : 0;
            break;
        case 0x35:                                  /* get vector       */
        {
            /* Split the interrupt's vector token into ES:BX.  See the
             * shim_vec_token comment: this is a token, not a callable
             * address. */
            unsigned short token = shim_token_for(in->h.al);

            out->x.bx = token;
            _shim_ES = 0;
            break;
        }
        case 0x25:                                  /* set vector       */
        {
            /* Undo a 35h: resolve the ES:BX token back to the ISR that
             * was registered for that interrupt and install it here. */
            unsigned long token = ((unsigned long)_shim_ES << 16) | out->x.bx;

            setvect(in->h.al, shim_isr_for_token(token));
            break;
        }
        default:
            break;
    }
}

/* INT 1Ah BIOS clock.  Also reachable through biostime(). */
static void shim_int1a(union REGS *in, union REGS *out)
{
    struct timespec ts;
    unsigned long ticks;

    clock_gettime(CLOCK_REALTIME, &ts);
    {
        time_t t = ts.tv_sec;
        struct tm tmv;
        localtime_r(&t, &tmv);
        ticks = (unsigned long)(((tmv.tm_hour * 3600L + tmv.tm_min * 60L + tmv.tm_sec) * 182L) / 10L)
                + (unsigned long)((ts.tv_nsec / 10000000L) * 182L / 1000L);
    }

    switch (in->h.ah)
    {
        case 0x00:
            out->x.cx = (unsigned short)(ticks >> 16);
            out->x.dx = (unsigned short)(ticks & 0xFFFF);
            out->h.al = 0;                          /* midnight flag    */
            break;
        case 0x01:                                  /* set time         */
            break;
        case 0x02:
            {
                time_t t = time(NULL);
                struct tm tmv;
                localtime_r(&t, &tmv);
                out->h.ch = (unsigned char)tmv.tm_hour;
                out->h.cl = (unsigned char)tmv.tm_min;
                out->h.dh = (unsigned char)tmv.tm_sec;
            }
            break;
        case 0x04:
            {
                time_t t = time(NULL);
                struct tm tmv;
                localtime_r(&t, &tmv);
                out->h.ch = (unsigned char)(tmv.tm_year % 100);
                out->h.cl = (unsigned char)(tmv.tm_mon + 1);
                out->h.dh = (unsigned char)tmv.tm_mday;
            }
            break;
        default:
            break;
    }
}

static void shim_dispatch(int intrno, union REGS *in, union REGS *out)
{
    if (out != in)
    {
        *out = *in;
    }

    switch (intrno)
    {
        case 0x10: shim_int10(in, out); break;
        case 0x21: shim_int21(in, out); break;
        case 0x1A: shim_int1a(in, out); break;
        default:
            if (shim_vectors[intrno & 0xFF] != NULL &&
                shim_vectors[intrno & 0xFF] != shim_noop_isr)
            {
                shim_vectors[intrno & 0xFF]();
                *out = *in;
            }
            break;
    }
    _shim_AX = out->x.ax;
    _shim_BX = out->x.bx;
    _shim_CX = out->x.cx;
    _shim_DX = out->x.dx;
}

int int86(int intrno, union REGS *inregs, union REGS *outregs)
{
    shim_dispatch(intrno, inregs, outregs);
    return outregs->x.ax;
}

int int86x(int intrno, union REGS *inregs, union REGS *outregs, struct SREGS *segregs)
{
    (void)segregs;
    shim_dispatch(intrno, inregs, outregs);
    return outregs->x.ax;
}

int intdos(union REGS *inregs, union REGS *outregs)
{
    shim_dispatch(0x21, inregs, outregs);
    return outregs->x.ax;
}

int intdosx(union REGS *inregs, union REGS *outregs, struct SREGS *segregs)
{
    shim_dispatch(0x21, inregs, outregs);
    if (segregs != NULL)
    {
        segread(segregs);
    }
    return outregs->x.ax;
}

void intr(int intrno, union REGS *inregs, union REGS *outregs)
{
    shim_dispatch(intrno, inregs, outregs);
}

void geninterrupt(int intrno)
{
    union REGS r;

    memset(&r, 0, sizeof(r));
    r.x.ax = _shim_AX;
    r.x.bx = _shim_BX;
    r.x.cx = _shim_CX;
    r.x.dx = _shim_DX;
    shim_dispatch(intrno, &r, &r);
}

void segread(struct SREGS *segregs)
{
    if (segregs == NULL)
    {
        return;
    }
    /* flat model: CS/DS/ES/SS have no meaning; report the InDOS slot as
     * segment 0 so the INT 21h/34h dance in SRC/SCUF/INT.C keeps working. */
    segregs->es = 0;
    segregs->cs = 0;
    segregs->ss = 0;
    segregs->ds = 0;
}

shim_isr getvect(int intrno)
{
    if (intrno < 0 || intrno > 255)
    {
        return shim_noop_isr;
    }
    return shim_vectors[intrno];
}

void setvect(int intrno, shim_isr isr)
{
    if (intrno < 0 || intrno > 255)
    {
        return;
    }
    shim_vectors[intrno] = (isr != NULL) ? isr : shim_noop_isr;
}

void shim_set_idle_hook(shim_isr hook)
{
    shim_idle_hook = hook;
}

/*  renderer ลงทะเบียน port_flush() ไว้ให้เราเรียกได้ แต่ shim ต้องไม่บังคับ
 *  ให้ต้องมี renderer (standalone test ของ shim ยัง link ได้) จึงใช้ weak
 *  ถ้า link มาด้วยกันจริง (binary cw) ตัวจริงจะถูกเรียกทุกครั้งที่ idle      */
extern void port_flush(void) __attribute__((weak));

void shim_run_idle(void)
{
    shim_isr h;

    /* The INT 28h handler installed by SRC/SCUF/INT.C */
    h = shim_vectors[0x28];
    if (h != NULL && h != shim_noop_isr)
    {
        h();
    }
    if (shim_idle_hook != NULL && shim_idle_hook != shim_noop_isr)
    {
        shim_idle_hook();
    }
    if (port_flush != NULL)
    {
        port_flush();
    }
}

/* ---------------------------------------------------------------- */
/*  TSR / critical error                                            */
/* ---------------------------------------------------------------- */
static shim_harderr_handler shim_harderr_handler_fn;

int _harderr(shim_harderr_handler handler)
{
    shim_harderr_handler_fn = handler;
    return 0;
}

void hardresume(int rescode)
{
    (void)rescode;
}

void hardretn(int retn)
{
    (void)retn;
}

void keep(unsigned char status, unsigned int memsize)
{
    (void)status;
    (void)memsize;
    /* No TSR support: just leave the program running. */
}

void enable(void)  { }
void disable(void) { }

/* ---------------------------------------------------------------- */
/*  Port I/O                                                        */
/* ---------------------------------------------------------------- */
unsigned char inportb(unsigned short portid)
{
    return shim_ports[portid];
}

unsigned short inport(unsigned short portid)
{
    return (unsigned short)(shim_ports[portid] | (shim_ports[(portid + 1) & 0xFFFF] << 8));
}

void outportb(unsigned short portid, unsigned char value)
{
    shim_ports[portid] = value;
}

void outport(unsigned short portid, unsigned short value)
{
    shim_ports[portid] = (unsigned char)(value & 0xFF);
    shim_ports[(portid + 1) & 0xFFFF] = (unsigned char)(value >> 8);
}

/* ---------------------------------------------------------------- */
/*  Real-mode memory                                                */
/* ---------------------------------------------------------------- */
unsigned char peekb(unsigned short seg, unsigned short off)
{
    unsigned long a = ((unsigned long)seg << 4) + off;
    return shim_realmem()[a & 0xFFFFF];
}

unsigned short peek(unsigned short seg, unsigned short off)
{
    unsigned long a = ((unsigned long)seg << 4) + off;
    unsigned char *m = shim_realmem();
    return (unsigned short)(m[a & 0xFFFFF] | (m[(a + 1) & 0xFFFFF] << 8));
}

void pokeb(unsigned short seg, unsigned short off, unsigned char value)
{
    unsigned long a = ((unsigned long)seg << 4) + off;
    shim_realmem()[a & 0xFFFFF] = value;
}

void poke(unsigned short seg, unsigned short off, unsigned short value)
{
    unsigned long a = ((unsigned long)seg << 4) + off;
    unsigned char *m = shim_realmem();
    m[a & 0xFFFFF] = (unsigned char)(value & 0xFF);
    m[(a + 1) & 0xFFFFF] = (unsigned char)(value >> 8);
}

/* ---------------------------------------------------------------- */
/*  Timing / speaker                                                */
/* ---------------------------------------------------------------- */
void delay(unsigned int milliseconds)
{
    struct timespec ts;

    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
    {
        ;
    }
}

void sound(unsigned int frequency)
{
    const char *on = getenv("SHIM_SOUND");

    (void)frequency;
    if (on != NULL && *on != '\0' && *on != '0')
    {
        fputc('\a', stderr);
        fflush(stderr);
    }
}

void nosound(void)
{
}
