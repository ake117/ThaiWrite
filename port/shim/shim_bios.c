/* =====================================================================
 * shim_bios.c - BIOS keyboard / printer / clock emulation
 * ---------------------------------------------------------------------
 * The terminal's key escape sequences are translated to the DOS BIOS key
 * codes CU-Writer expects (scan<<8 | ascii), so every "ebioskey(0) ==
 * UPKEY" style comparison keeps working.
 *
 * bioskey(0)  blocking read   (drives the INT 28h idle hook while it waits)
 * bioskey(1)  key pending?
 * bioskey(2)  shift flags     (always 0: no modifier tracking)
 *
 * asm_keypressed() is normally defined in the (dropped) ASM display
 * drivers; the shim provides it so SRC/COMMON/EKBD.C links.
 * ===================================================================== */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <sys/select.h>

#include "shim_turbo.h"
#include "bios.h"
#include "dos.h"

/* ---- DOS key codes the sources compare against (kbdcode.h) -------- */
#define K_ESC        0x011B
#define K_RET        0x1C0D
#define K_BS         0x0E08
#define K_TAB        0x0F09
#define K_SHFTTAB    0x0F00
#define K_UP         0x4800
#define K_DOWN       0x5000
#define K_RIGHT      0x4D00
#define K_LEFT       0x4B00
#define K_HOME       0x4700
#define K_END        0x4F00
#define K_PGUP       0x4900
#define K_PGDN       0x5100
#define K_INS        0x5200
#define K_DEL        0x5300
#define K_F1         0x3B00

/* ---- key queue ---------------------------------------------------- */
#define SHIM_KQ_SIZE 256
static int  shim_kq[SHIM_KQ_SIZE];
static int  shim_kq_head, shim_kq_tail;

static unsigned char shim_pending[16];
static int  shim_pending_len;

static struct termios shim_saved_tio;
static int  shim_tio_saved;
static int  shim_stdin_eof;

/*  debug: CW_KEY_DEBUG=<ไฟล์> บันทึก keycode ทุกตัวที่ถอดได้
 *  (ใช้ไล่ปัญหา "ปุ่มไม่ทำงาน" ว่าคีย์ไปถึงแอปหรือไม่)               */
static void shim_key_debug(int keycode)
{
    static FILE *dbg;
    static int tried;

    if (!tried) {
        const char *path = getenv("CW_KEY_DEBUG");

        tried = 1;
        if (path != NULL && *path != '\0')
            dbg = fopen(path, "a");
    }
    if (dbg != NULL) {
        fprintf(dbg, "%04x\n", keycode & 0xFFFF);
        fflush(dbg);
    }
}

static void shim_kq_push(int keycode)
{
    int next = (shim_kq_tail + 1) % SHIM_KQ_SIZE;

    shim_key_debug(keycode);

    if (next == shim_kq_head)
    {
        return;                     /* full: drop */
    }
    shim_kq[shim_kq_tail] = keycode;
    shim_kq_tail = next;
}

static int shim_kq_pop(void)
{
    int v;

    if (shim_kq_head == shim_kq_tail)
    {
        return -1;
    }
    v = shim_kq[shim_kq_head];
    shim_kq_head = (shim_kq_head + 1) % SHIM_KQ_SIZE;
    return v;
}

static int shim_kq_empty(void)
{
    return shim_kq_head == shim_kq_tail;
}

/* ---- terminal setup ----------------------------------------------- */
static void shim_restore_tio(void)
{
    if (shim_tio_saved)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &shim_saved_tio);
        shim_tio_saved = 0;
    }
}

static void shim_raw_mode(void)
{
    struct termios tio;

    if (shim_tio_saved)
    {
        return;
    }
    if (!isatty(STDIN_FILENO))
    {
        return;
    }
    if (tcgetattr(STDIN_FILENO, &tio) != 0)
    {
        return;
    }
    shim_saved_tio = tio;
    shim_tio_saved = 1;
    atexit(shim_restore_tio);

    cfmakeraw(&tio);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);
}

/* ---- scan code table for printable ASCII -------------------------- */
static unsigned char shim_ascii_scan[0x80];
static int shim_ascii_scan_ready;

static void shim_build_scan_table(void)
{
    static const unsigned char scan[96] = {
        /* 0x20 ' ' */ 0x39,
        /* ! " # $ % & ' ( ) * + , - . / */
        0x02, 0x28, 0x04, 0x05, 0x06, 0x08, 0x28, 0x0A, 0x0B, 0x09,
        0x0D, 0x33, 0x0C, 0x34, 0x35,
        /* 0 1 2 3 4 5 6 7 8 9 */
        0x0B, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
        /* : ; < = > ? @ */
        0x27, 0x27, 0x33, 0x0D, 0x34, 0x35, 0x03,
        /* A .. Z */
        0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24,
        0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14,
        0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,
        /* [ \ ] ^ _ ` */
        0x1A, 0x2B, 0x1B, 0x07, 0x0C, 0x29,
        /* a .. z */
        0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24,
        0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14,
        0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,
        /* { | } ~ DEL */
        0x1A, 0x2B, 0x1B, 0x29, 0x0E
    };
    int i;

    if (shim_ascii_scan_ready)
    {
        return;
    }
    memset(shim_ascii_scan, 0, sizeof(shim_ascii_scan));
    for (i = 0; i < 96; i++)
    {
        shim_ascii_scan[0x20 + i] = scan[i];
    }
    shim_ascii_scan_ready = 1;
}

/* Scan code of a control letter (Ctrl-A .. Ctrl-Z) */
static unsigned char shim_ctrl_scan(int c)
{
    static const unsigned char t[27] = {
        0, 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24,
        0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14,
        0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
    };
    if (c >= 1 && c <= 26)
    {
        return t[c];
    }
    return 0;
}

/* ---- sequence decoding -------------------------------------------- */
/* Returns the number of bytes consumed, or 0 when more input is needed. */
static int shim_decode_seq(int *out, const unsigned char *b, int n)
{
    unsigned char c0, c1, c2;

    shim_build_scan_table();        /* idempotent; fills shim_ascii_scan */

    if (n <= 0)
    {
        return 0;
    }
    c0 = b[0];

    if (c0 != 0x1B)
    {
        if (c0 == 0x0D) { *out = K_RET; return 1; }
        if (c0 == 0x0A) { *out = 0x240A; return 1; }
        if (c0 == 0x09) { *out = K_TAB; return 1; }
        if (c0 == 0x08 || c0 == 0x7F) { *out = K_BS; return 1; }
        if (c0 < 0x20)
        {
            unsigned char s = shim_ctrl_scan(c0);
            if (s != 0)
            {
                *out = (s << 8) | c0;
                return 1;
            }
            *out = 0x0100 | c0;
            return 1;
        }
        if (c0 < 0x80)
        {
            *out = (shim_ascii_scan[c0] << 8) | c0;
            return 1;
        }
        *out = 0x0100 | c0;
        return 1;
    }

    /* Escape ... */
    if (n == 1)
    {
        return 0;                   /* need more bytes, or it is bare ESC */
    }
    c1 = b[1];

    if (c1 == '[' || c1 == 'O')
    {
        if (n < 3)
        {
            return 0;
        }
        c2 = b[2];
        if (c1 == 'O')
        {
            switch (c2)
            {
                case 'A': *out = K_UP;    return 3;
                case 'B': *out = K_DOWN;  return 3;
                case 'C': *out = K_RIGHT; return 3;
                case 'D': *out = K_LEFT;  return 3;
                case 'H': *out = K_HOME;  return 3;
                case 'F': *out = K_END;   return 3;
                case 'P': *out = K_F1 + 0x0000; return 3;
                case 'Q': *out = K_F1 + 0x0100; return 3;
                case 'R': *out = K_F1 + 0x0200; return 3;
                case 'S': *out = K_F1 + 0x0300; return 3;
                default:  *out = K_ESC;   return 2;
            }
        }
        /* CSI ... */
        if (c2 >= '0' && c2 <= '9')
        {
            int i = 2, val = 0;
            while (i < n && b[i] >= '0' && b[i] <= '9')
            {
                val = val * 10 + (b[i] - '0');
                i++;
            }
            if (i >= n)
            {
                return 0;           /* need the final byte */
            }
            if (b[i] == '~')
            {
                switch (val)
                {
                    case 1:  *out = K_HOME; return i + 1;
                    case 2:  *out = K_INS;  return i + 1;
                    case 3:  *out = K_DEL;  return i + 1;
                    case 4:  *out = K_END;  return i + 1;
                    case 5:  *out = K_PGUP; return i + 1;
                    case 6:  *out = K_PGDN; return i + 1;
                    case 7:  *out = K_HOME; return i + 1;
                    case 8:  *out = K_END;  return i + 1;
                    case 11: *out = K_F1;        return i + 1;
                    case 12: *out = K_F1 + 0x0100; return i + 1;
                    case 13: *out = K_F1 + 0x0200; return i + 1;
                    case 14: *out = K_F1 + 0x0300; return i + 1;
                    case 15: *out = K_F1 + 0x0400; return i + 1;
                    case 17: *out = K_F1 + 0x0500; return i + 1;
                    case 18: *out = K_F1 + 0x0600; return i + 1;
                    case 19: *out = K_F1 + 0x0700; return i + 1;
                    case 20: *out = K_F1 + 0x0800; return i + 1;
                    case 21: *out = K_F1 + 0x0900; return i + 1;
                    case 23: *out = 0x8500;        return i + 1;   /* F11 */
                    case 24: *out = 0x8600;        return i + 1;   /* F12 */
                    default: *out = K_ESC;         return i + 1;
                }
            }
            /* ESC [ <digits> <letter> (e.g. ESC [ 1 ; 5 A) - ignore modifiers */
            *out = K_ESC;
            return i + 1;
        }
        switch (c2)
        {
            case 'A': *out = K_UP;    return 3;
            case 'B': *out = K_DOWN;  return 3;
            case 'C': *out = K_RIGHT; return 3;
            case 'D': *out = K_LEFT;  return 3;
            case 'H': *out = K_HOME;  return 3;
            case 'F': *out = K_END;   return 3;
            case 'Z': *out = K_SHFTTAB; return 3;
            default:  *out = K_ESC;   return 2;
        }
    }

    /* ESC <printable> = Alt+key */
    if (c1 >= 0x20 && c1 < 0x80)
    {
        unsigned char s = shim_ascii_scan[c1];
        *out = (s << 8);            /* ASCII 0, scan in high byte */
        return 2;
    }
    *out = K_ESC;
    return 2;
}

/* ---- raw read + decode -------------------------------------------- */
static int shim_read_available(int wait_ms)
{
    fd_set rfds;
    struct timeval tv;
    struct timeval *ptv;
    int r;
    unsigned char buf[64];
    ssize_t got;
    static const long ESC_TIMEOUT_MS = 40;

    shim_raw_mode();

    if (wait_ms <= 0)
    {
        tv.tv_sec = 0;
        tv.tv_usec = 0;             /* poll */
    }
    else if (shim_pending_len > 0)
    {
        /* give an ambiguous ESC a little time to complete */
        tv.tv_sec = ESC_TIMEOUT_MS / 1000;
        tv.tv_usec = (ESC_TIMEOUT_MS % 1000) * 1000;
    }
    else
    {
        tv.tv_sec = wait_ms / 1000;
        tv.tv_usec = (wait_ms % 1000) * 1000;
    }
    ptv = &tv;

    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    r = select(STDIN_FILENO + 1, &rfds, NULL, NULL, ptv);
    if (r < 0)
    {
        return 0;
    }
    if (r == 0)
    {
        /* timeout: a lone ESC becomes the ESC key */
        if (shim_pending_len == 1 && shim_pending[0] == 0x1B)
        {
            shim_pending_len = 0;
            shim_kq_push(K_ESC);
            return 1;
        }
        return 0;
    }

    got = read(STDIN_FILENO, buf, sizeof(buf));
    if (got == 0)
    {
        shim_stdin_eof = 1;
        return 0;
    }
    if (got < 0)
    {
        return 0;
    }

    {
        ssize_t i = 0;
        while (i < got)
        {
            int keycode = 0;
            int used;

            if (shim_pending_len >= (int)sizeof(shim_pending))
            {
                shim_pending_len = 0;
            }
            shim_pending[shim_pending_len++] = buf[i];
            i++;

            used = shim_decode_seq(&keycode, shim_pending, shim_pending_len);
            if (used > 0)
            {
                shim_kq_push(keycode);
                memmove(shim_pending, shim_pending + used, shim_pending_len - used);
                shim_pending_len -= used;
            }
        }
    }
    return 1;
}

void shim_key_pump(void)
{
    int guard = 16;

    while (guard-- > 0)
    {
        if (shim_read_available(0) == 0)
        {
            break;
        }
        /* keep draining while bytes keep coming */
        {
            fd_set rfds;
            struct timeval tv;
            FD_ZERO(&rfds);
            FD_SET(STDIN_FILENO, &rfds);
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) <= 0)
            {
                break;
            }
        }
    }
}

/* ---- public entry points ------------------------------------------ */
int bioskey(int cmd)
{
    if (cmd == 0)
    {
        for (;;)
        {
            int v;

            shim_key_pump();
            v = shim_kq_pop();
            if (v >= 0)
            {
                return v;
            }
            if (shim_stdin_eof)
            {
                return K_ESC;               /* EOF: behave like "ESC" */
            }
            /* wait a little, then drive the INT 28h idle hook */
            shim_read_available(10);
            shim_run_idle();
        }
    }
    if (cmd == 1)
    {
        shim_key_pump();
        return shim_kq_empty() ? 0 : 1;
    }
    /* cmd == 2: shift status */
    return 0;
}

int asm_keypressed(void)
{
    static int dbg_left = 8;
    shim_key_pump();
    if (dbg_left > 0 && getenv("CW_TONE_DEBUG") != NULL)
    {
        /* debug: ดูว่าตอน startup มีคีย์ค้างให้ cusong() โดนข้ามไหม      */
        dbg_left--;
        fprintf(stderr, "[kp q=%d p=%d]", shim_kq_empty() ? 0 : 1,
                (int)shim_pending_len);
        fflush(stderr);
    }
    if (!shim_kq_empty())
    {
        return 1;
    }
    if (shim_pending_len > 0)
    {
        /* An ESC alone is ambiguous; report "not pressed" until it times
         * out so arrow sequences are never split. */
        if (!(shim_pending_len == 1 && shim_pending[0] == 0x1B))
        {
            return 1;
        }
    }
    return 0;
}

/* ---- printer ------------------------------------------------------ */
static FILE *shim_printer;
static int   shim_printer_tried;

static FILE *shim_printer_get(void)
{
    if (shim_printer_tried)
    {
        return shim_printer;
    }
    shim_printer_tried = 1;
    {
        const char *path = getenv("SHIM_PRINTER");

        if (path != NULL && *path != '\0')
        {
            shim_printer = fopen(path, "ab");
        }
    }
    return shim_printer;
}

int biosprint(int cmd, int byte, int port)
{
    (void)port;

    if (cmd == PRINTER_WRITE)
    {
        FILE *fp = shim_printer_get();

        if (fp != NULL)
        {
            fputc(byte & 0xFF, fp);
            fflush(fp);
        }
        return 0x90;                    /* selected + not busy */
    }
    /* PRINTER_STATUS */
    return 0x90;
}

/* ---- BIOS tick clock ---------------------------------------------- */
long biostime(int cmd, long newtime)
{
    struct timespec ts;
    time_t t;
    struct tm tmv;
    unsigned long ticks;

    (void)newtime;

    clock_gettime(CLOCK_REALTIME, &ts);
    t = ts.tv_sec;
    localtime_r(&t, &tmv);
    ticks = (unsigned long)(((tmv.tm_hour * 3600L + tmv.tm_min * 60L + tmv.tm_sec) * 182L) / 10L)
            + (unsigned long)((ts.tv_nsec / 10000000L) * 182L / 1000L);

    if (cmd == 0)
    {
        return (long)(ticks & 0xFFFFFFFFUL);
    }
    return 0;
}
