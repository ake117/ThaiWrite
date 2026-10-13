/* =====================================================================
 * dos.h - Turbo C <dos.h> replacement for the ThaiWrite Linux port
 * ---------------------------------------------------------------------
 * The real-mode register interface (int86/intdos/intr/...) is emulated
 * in user space by shim_dos.c: a small software "PC" is kept in memory
 * (video mode register, VGA register ports, a 1 MB real-mode address
 * space for peek/poke, and an interrupt dispatch table).
 *
 * Implemented interrupts:
 *   INT 10h  - video BIOS      (mode set/get, EGA/VGA configuration,
 *                               MCGA/video-info probe)
 *   INT 16h  - keyboard        (see bios.h / bioskey)
 *   INT 1Ah  - BIOS clock      (see bios.h / biostime)
 *   INT 21h  - DOS             (AH=30h version, 33h Ctrl-Break,
 *                               34h InDOS flag, 35h/25h vector access)
 *   INT 28h/23h/24h - DOS idle / Ctrl-C / critical error vectors
 *                               (setvect/getvect keep a dispatch table)
 * ===================================================================== */
#ifndef _SHIM_DOS_H
#define _SHIM_DOS_H

#include "shim_turbo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- */
/*  Register unions (identical layout to Turbo C on x86)            */
/* ---------------------------------------------------------------- */
struct WORDREGS
{
    unsigned short ax, bx, cx, dx, si, di, cflag, flags;
};

struct BYTEREGS
{
    unsigned char al, ah, bl, bh, cl, ch, dl, dh;
};

union REGS
{
    struct WORDREGS x;
    struct BYTEREGS h;
};

struct SREGS
{
    unsigned short es, cs, ss, ds;
};

struct REGPACK
{
    unsigned short r_ax, r_bx, r_cx, r_dx, r_bp, r_si, r_di, r_ds, r_es, r_flags;
};

/* ---------------------------------------------------------------- */
/*  Software interrupt interface                                    */
/* ---------------------------------------------------------------- */
int  int86(int intrno, union REGS *inregs, union REGS *outregs);
int  int86x(int intrno, union REGS *inregs, union REGS *outregs, struct SREGS *segregs);
int  intdos(union REGS *inregs, union REGS *outregs);
int  intdosx(union REGS *inregs, union REGS *outregs, struct SREGS *segregs);
void intr(int intrno, union REGS *inregs, union REGS *outregs);
void geninterrupt(int intrno);
void segread(struct SREGS *segregs);

/* Interrupt vector table.  The "void interrupt" qualifier is stripped
 * by shim_turbo.h, so the ISR type here is a plain void(void) function. */
typedef void (*shim_isr)(void);
shim_isr getvect(int intrno);
void     setvect(int intrno, shim_isr isr);

/* DOS "idle" hook.  Called with a small frequency while the shim is
 * blocked waiting for a key (bioskey(0)); used by the port to drive
 * the INT 28h based blinking in SRC/SCUF/INT.C without real timer
 * interrupts. */
void shim_set_idle_hook(shim_isr hook);
void shim_run_idle(void);

/* ---------------------------------------------------------------- */
/*  DOS TSR / critical-error interface                              */
/* ---------------------------------------------------------------- */
void keep(unsigned char status, unsigned int memsize);
void enable(void);
void disable(void);

typedef int (*shim_harderr_handler)(int errval, int ax, int bp, int si);
int  _harderr(shim_harderr_handler handler);
#define harderr(handler) _harderr(handler)
void hardresume(int rescode);
void hardretn(int retn);

/* ---------------------------------------------------------------- */
/*  Port I/O emulation                                              */
/* ---------------------------------------------------------------- */
unsigned char inportb(unsigned short portid);
unsigned short inport(unsigned short portid);
void outportb(unsigned short portid, unsigned char value);
void outport(unsigned short portid, unsigned short value);

/* ---------------------------------------------------------------- */
/*  Real-mode memory access (1 MB shadow space in shim_dos.c)       */
/* ---------------------------------------------------------------- */
unsigned char peekb(unsigned short seg, unsigned short off);
unsigned short peek(unsigned short seg, unsigned short off);
void pokeb(unsigned short seg, unsigned short off, unsigned char value);
void poke(unsigned short seg, unsigned short off, unsigned short value);
unsigned char *shim_realmem(void);

/* ---------------------------------------------------------------- */
/*  Timing / speaker                                                */
/* ---------------------------------------------------------------- */
void delay(unsigned int milliseconds);
void sound(unsigned int frequency);
void nosound(void);

/* ---------------------------------------------------------------- */
/*  Emulated VGA state (used by graphics.h and the port renderer)   */
/* ---------------------------------------------------------------- */
#define SHIM_VRAM_SIZE (256 * 1024)     /* 0xA0000 .. 0xE0000 */
unsigned char *shim_vram(void);         /* the 0xA0000..0xE0000 window   */
unsigned char *shim_vram_text(void);    /* the 0xB8000 text page window  */
unsigned char *shim_vram_herc(void);    /* "seg 0xB000" Herc framebuffer */
unsigned char *shim_vram_ega(void);     /* "seg 0xA000" EGA/VGA surface  */
int shim_video_is_mapped(void);         /* 1 = the fixed maps installed  */
int shim_video_mode(void);
void shim_set_video_mode(int mode);
void shim_set_ega_config(unsigned char bl, unsigned char bh, unsigned char cl);
unsigned char shim_ega_bl(void);
unsigned char shim_ega_bh(void);
unsigned char shim_ega_cl(void);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_DOS_H */
