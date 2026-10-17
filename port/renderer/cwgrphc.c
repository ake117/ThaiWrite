/*-----------------------------------------------------------------------*/
/*  cwgrphc.c - ncurses backend for the ThaiWrite display layer          */
/*                                                                       */
/*  Original : SRC/COMMON/CWGRPHC.C + EDISP.ASM / HDISP.ASM / ADISP.ASM  */
/*             (CGA/ATT400, Hercules, EGA/VGA bitmap painters written in */
/*             8086 assembly) + CWFNTM.ASM (shift macros) + the          */
/*             whatlevel leveltable in HDISP.ASM.                        */
/*  Port     : one cell-based backend.  Every DOS adapter entry point    */
/*             (h*, e*, a* prefix) is provided; they all share the       */
/*             ncurses implementation, so setgraph's adapter switch      */
/*             only records which adapter was requested.                 */
/*                                                                       */
/*  Model                                                                */
/*  ------                                                               */
/*  The DOS code painted into a linear framebuffer of 8x20-pixel cells   */
/*  (one TIS-620 glyph per cell, Thai vowels/tone marks XOR-blitted      */
/*  into neighbouring cells).  Here each cell keeps its full overlay     */
/*  state (base + top + upper + below + attr) in a shadow screen;        */
/*  port_flush() renders dirty cells to ncurses.  XOR semantics of the   */
/*  original are approximated: a second XOR of the same overlay removes  */
/*  it (that is all the original ever used XOR for).                     */
/*                                                                       */
/*  Byte buffers (getwind/putwind/savepic/retpic) keep the DOS layout:   */
/*  one 20-byte record per scanline row per column, rows*20 records      */
/*  per column strip, exactly as EDISP.ASM computed them.                */
/*                                                                       */
/*  Fonts: the DOS build read NORMAL.FON / ITALIC.FON (20*256 bytes,     */
/*  one 20-byte glyph per TIS-620 code) into dfont/ditalicfont and        */
/*  the ASM painted glyph bits.  The terminal already has Thai glyphs,   */
/*  so prchar() renders the character itself; the attr bits select       */
/*  A_UNDERLINE / A_BOLD / A_REVERSE / A_ITALIC and super/sub/enlarge    */
/*  adjust placement.  dfont/ditalicfont remain loadable for callers.    */
/*-----------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <locale.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/ioctl.h>

#include <ncurses.h>

#include "cwgrphc.h"

/* ------------------------------------------------------------------ */
/* Globals (DEFINE_VAR section of the original).                       */
/* ------------------------------------------------------------------ */
int           scrmode = DETECT;
unsigned int  graph_seg = 0;          /* was 0xA000/0xB000 in DOS      */
unsigned char *screen_buffptr = NULL;
unsigned char port_chardot[PORT_CELL_SIZE];

/*  dfont / ditalicfont: ที่เก็บจริงอยู่ที่ VAR.H + CW.C ของต้นฉบับ
 *  (VAR.H ใช้ EXTERN ซึ่งกลายเป็นนิยามเมื่อ CW.C define DEFINE_VAR)
 *  ห้ามนิยามซ้ำที่นี่ ไม่งั้น link เจอ multiple definition        */
extern char dfont[20 * 256];
extern char ditalicfont[20 * 256];

/* dispatch table */
Prchar_ptr     prchar_ptr     = NULL;
Prblank_ptr    prblank_ptr    = NULL;
Setcurpos_ptr  setcurpos_ptr  = NULL;
Plot_ptr       plot_ptr       = NULL;
Voidvoid_ptr   setgraph_ptr   = NULL;
Voidvoid_ptr   settext_ptr    = NULL;
Voidvoid_ptr   savepic_ptr    = NULL;
Voidvoid_ptr   retpic_ptr     = NULL;
Voidvoid_ptr   clsall_ptr     = NULL;
Voidvoid_ptr   prakeaw_ptr    = NULL;
Clsgraph_ptr   clsgraph_ptr   = NULL;
Clrline_ptr    clrline_ptr    = NULL;
RWchardot_ptr  rdchardot_ptr  = NULL;
RWchardot_ptr  wrchardot_ptr  = NULL;
PGwind_ptr     putwind_ptr    = NULL;
PGwind_ptr     getwind_ptr    = NULL;

/* ------------------------------------------------------------------ */
/* Backend state.                                                      */
/* ------------------------------------------------------------------ */
static port_cell screen[PORT_SCREEN_ROWS * PORT_SCREEN_COLS];
static unsigned char dirty[PORT_SCREEN_ROWS * PORT_SCREEN_COLS];
static int  port_active = 0;
static int  cur_x = 0, cur_y = 0;
static int  last_maxx = -1, last_maxy = -1;   /* ncurses size ครั้งก่อน  */

/*  ขนาด terminal จริงจาก ioctl  —  shim อ่านคีย์เองด้วย bioskey()
 *  ไม่ได้ผ่าน ncurses getch() ดังนั้น ncurses จึงไม่เคยได้รับสัญญาณ
 *  SIGWINCH แล้วอัปเดต LINES/COLS ให้ ต้องถาม kernel เองทุก flush      */
static int port_query_size(int *rows, int *cols)
{
    struct winsize ws;
    int fd;

    for (fd = 1; fd >= 0; fd--) {
        if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
            *rows = ws.ws_row;
            *cols = ws.ws_col;
            return 1;
        }
    }
    return 0;
}

/* whatlevel lookup - port of HDISP.ASM leveltable (0xD0..0xEF).       */
/* index = c - 0xD0                                                    */
static const unsigned char leveltable[32] = {
    0,2,0,0,2,2,2,2,1,1,1,2,0,0,0,0,
    0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,0
};

int whatlevel(unsigned char c)
{
    if (c < 0xD0)
        return 0;                     /* MIDDLE */
    return leveltable[c - 0xD0];
}

/* ------------------------------------------------------------------ */
/* Helpers.                                                            */
/* ------------------------------------------------------------------ */
static int in_bounds(unsigned int x, unsigned int y)
{
    return port_active && x < PORT_SCREEN_COLS && y < PORT_SCREEN_ROWS;
}

port_cell *port_cell_at(unsigned int x, unsigned int y)
{
    if (!in_bounds(x, y))
        return NULL;
    return &screen[y * PORT_SCREEN_COLS + x];
}

static void mark_dirty(unsigned int x, unsigned int y)
{
    if (in_bounds(x, y))
        dirty[y * PORT_SCREEN_COLS + x] = 1;
}

int port_graph_active(void)
{
    return port_active;
}


/*  ขนาดพื้นที่ที่วาดได้จริง = min(ขนาด shadow screen, ขนาด terminal)
 *  ใช้จัดกล่อง splash ให้อยู่กลางจอ                                     */
int port_visible_cols(void)
{
    int c = getmaxx(stdscr);

    if (c <= 0)
        return PORT_SCREEN_COLS;
    return (c < PORT_SCREEN_COLS) ? c : PORT_SCREEN_COLS;
}

int port_visible_rows(void)
{
    int r = getmaxy(stdscr);

    if (r <= 0)
        return PORT_SCREEN_ROWS;
    return (r < PORT_SCREEN_ROWS) ? r : PORT_SCREEN_ROWS;
}
/* Thai glyph code classes (from EDISP.ASM eprchar):                   */
/*   topest       0xE7..0xEC                                           */
/*   upper        0xD1, 0xD4..0xD7, 0xED..0xEE                         */
/*   below        0xD8..0xDA                                           */
/*   middle       everything else                                      */
static int is_top(unsigned char c)
{
    return (c >= 0xE7 && c <= 0xEC);
}

static int is_upper(unsigned char c)
{
    return (c == 0xD1) ||
           (c >= 0xD4 && c <= 0xD7) ||
           (c == 0xED) || (c == 0xEE);
}

static int is_below(unsigned char c)
{
    return (c >= 0xD8 && c <= 0xDA);
}

/* Reverse-video exclusion list from EDISP.ASM: those upper/top codes   */
/* are NOT inverted when the cell is reversed.                          */
static int reverse_applies(unsigned char c)
{
    if (c <= 0xD0) return 1;
    if (c <= 0xD1) return 0;
    if (c <= 0xD3) return 1;
    if (c <= 0xDA) return 0;
    if (c <  0xE7) return 1;
    if (c <= 0xEE) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* TIS-620 -> UTF-8 สำหรับส่งออก terminal                              */
/* ------------------------------------------------------------------ */
/*  ฟอนต์ของ CU Writer ใช้รหัส TIS-620 (0xA1-0xFB = ไทย) ตรงกับช่วง
 *  U+0E01-U+0E5B ของ Unicode แบบ 1:1 จึงแปลงได้ตรง ๆ
 *  ไบต์ 0x8F-0x9B เป็นอักษรเส้นกล่องของฟอนต์ต้นฉบับ (NORMAL.FON
 *  วาดเป็นเส้นบาง 1 พิกเซล) — ต้องแปลงเป็น Unicode Box Drawing
 *  (U+2500-U+253C) ไม่ใช่ส่งไบต์ตรง ๆ ซึ่งจะกลายเป็น C1 control
 *  ที่ terminal ไม่แสดงผล                                            */
static unsigned int box_unicode(unsigned char c)
{
    switch (c) {
        case 0x8F: return 0x253C;   /* ┼ */
        case 0x90: return 0x252C;   /* ┬ */
        case 0x91: return 0x2534;   /* ┴ */
        case 0x92: return 0x2524;   /* ┤ */
        case 0x93: return 0x251C;   /* ├ */
        case 0x95: return 0x2500;   /* ─ */
        case 0x96: return 0x2502;   /* │ */
        case 0x98: return 0x250C;   /* ┌ */
        case 0x99: return 0x2510;   /* ┐ */
        case 0x9A: return 0x2514;   /* └ */
        case 0x9B: return 0x2518;   /* ┘ */
        default:   return 0;
    }
}

static int put_utf8(char *buf, int n, unsigned char c)
{
    unsigned int u;

    if (c >= 0xA1 && c <= 0xFB) {
        u = 0x0E00u + (unsigned int)(c - 0xA0);
    }
    else if ((u = box_unicode(c)) != 0) {
        /* เส้นกล่อง -> Unicode Box Drawing */
    }
    else if (c >= 0x80) {
        u = c;
    }
    else {
        buf[n++] = (char)c;
        return n;
    }
    buf[n++] = (char)(0xE0 | (u >> 12));
    buf[n++] = (char)(0x80 | ((u >> 6) & 0x3F));
    buf[n++] = (char)(0x80 | (u & 0x3F));
    return n;
}

/* ------------------------------------------------------------------ */
/* Rendering one cell to ncurses.                                      */
/* ------------------------------------------------------------------ */
static chtype cell_attr(const port_cell *cell)
{
    chtype a = A_NORMAL;
    unsigned int attr = cell->attr;

    if (reverse_applies(cell->base) && (attr & REVERSEATTR))
        a |= A_REVERSE;
    if (attr & BOLDATTR)
        a |= A_BOLD;
    if ((attr & ONELINEATTR) || (attr & TWOLINEATTR))
        a |= A_UNDERLINE;
    if (attr & ITALICATTR)
        a |= A_ITALIC;
    return a;
}

/* Move the physical cursor for a logical (x, y, thaimode).            */
static void place_cursor(unsigned int x, unsigned int y, int thaimode)
{
    /* Thai-mode base line sits 4/20 down the cell, english 16/20; on a
       20px cell that is 0.2 vs 0.8 of a terminal row, rendered as the
       same row - keep the terminal cursor on the cell's base line.    */
    (void)thaimode;
    cur_x = (int)x;
    cur_y = (int)y;
    move(cur_y, cur_x);
}

/* ------------------------------------------------------------------ */
/* dot-cell geometry: dot[] เก็บ mask ของคอลัมน์พิกเซลต่อกลุ่มแถว      */
/* ------------------------------------------------------------------ */
/*  เส้นนอน = ทุกคอลัมน์ของกลุ่มเดียวถูกเปิด (0xff)                    */
/*  มีช่วงเส้นนอนในเซลล์ไหม — บิตติดกัน >= 2 บิตในกลุ่มพิกเซลใดกลุ่มหนึ่ง
 *  (เส้นกว้าง 1 พิกเซล แต่ยาวได้หลายพิกเซล และเซลล์ที่อยู่ปลายเส้นอาจมี
 *  แค่ 2 พิกเซลสุดท้าย เช่นมุมขวาล่างของกรอบ)                        */
static int dot_hstroke(const port_cell *c)
{
    int g;

    for (g = 0; g < 3; g++) {
        unsigned char b = c->dot[g];

        if (b == 0xff)
            return 1;
        if (b & (unsigned char)(b >> 1))        /* บิตติดกัน >= 2 */
            return 1;
    }
    return 0;
}

/*  เส้นตั้ง = บิตเดียวกันถูกเปิดในทุกกลุ่มพิกเซล (คอลัมน์พิกเซลใดก็ได้) */
static int dot_vstroke_any(const port_cell *c)
{
    return (c->dot[0] & c->dot[1] & c->dot[2]) != 0;
}

/*  ดอทบิตของอักขระ Braille (U+2800-U+28FF) — ใช้กับ bitmap ที่ DOS
 *  blit ลง video memory ตรง ๆ (prakeaw) เพราะหนึ่งเซลล์แสดงได้ 2x4 ดอท
 *  ละเอียดกว่า half/quadrant block (2x2) สองเท่าในแนวตั้ง
 *  [คอลัมน์][แถว] : ดอท 1/2/3 = ซ้ายแถว 0/1/2, 4/5/6 = ขวาแถว 0/1/2,
 *  ดอท 7 = ซ้ายแถว 3, ดอท 8 = ขวาแถว 3                              */
static const unsigned char braille_bit[2][4] = {
    { 0x01, 0x02, 0x04, 0x40 },     /* คอลัมน์ซ้าย แถว 0..3 */
    { 0x08, 0x10, 0x20, 0x80 }      /* คอลัมน์ขวา  แถว 0..3 */
};

/*  แถบ UI ด้านบน (ชื่อมหาลัย/ไดรฟ์/เมนูบาร์/แถบสถานะ) — ต้นฉบับ DOS วาด
 *  เส้นคั่นไว้ที่ *พิกเซลบนสุด* ของเซลล์ แล้วตัวอักษรอยู่ใต้เส้นในเซลล์
 *  เดียวกัน (เซลล์สูง 20 px) terminal แสดงได้เซลล์ละตัวอักษรเดียว จึง
 *  เห็นข้อความ *ทับ* เส้นกรอบ
 *  แก้: ย้ายเส้นคั่นมาเป็นขีดเส้นใต้ของแถวข้อความ (ตำแหน่งเดียวกับที่ DOS
 *  วางเส้น คือใต้ตัวอักษร) แล้วระงับเส้นนอนของ dot layer ในแถบนี้        */
#define PORT_UI_BAND_ROWS 3

static int ui_band_row(unsigned int y)
{
    return y < PORT_UI_BAND_ROWS;
}

static void render_cell(unsigned int x, unsigned int y)
{
    port_cell *cell = &screen[y * PORT_SCREEN_COLS + x];
    int band = ui_band_row(y);
    char buf[24];
    int n = 0;
    chtype attrs;

    if (x >= (unsigned)getmaxx(stdscr))
        return;

    /*  bitmap blit (crest logo ฯลฯ): port_cell.over = ดอทมาสก์ของ
     *  อักขระ Braille (U+2800 + mask) ที่ย่อจาก bitmap ต้นฉบับ        */
    if (cell->over) {
        if ((int)y < getmaxy(stdscr)) {
            char br[4];

            br[0] = (char)0xE2;
            br[1] = (char)(0xA0 | ((cell->over >> 6) & 0x03));
            br[2] = (char)(0x80 | (cell->over & 0x3F));
            br[3] = '\0';
            attrs = cell_attr(cell) & A_ATTRIBUTES;
            attrset(attrs);
            mvaddstr((int)y, (int)x, br);
            attrset(A_NORMAL);
        }
        return;
    }

    /*  เซลล์ที่มีทั้งข้อความและเส้นกราฟิก: DOS วาดคนละแถวพิกเซลในเซลล์
     *  เดียวกันจึงเห็นทั้งข้อความและเส้น แต่ terminal แสดงได้อย่างเดียว
     *  ถ้าเซลล์มีตัวอักษรจริง (ไม่ใช่ช่องว่าง) ให้ข้อความชนะ — ไม่งั้น
     *  ชื่อเมนูบาร์/รายการเมนูแถวแรกที่ต้นฉบับขีดเส้นทับจะหายทั้งแถบ
     *  เซลล์ว่างยังให้เส้นชนะ กรอบกล่องจึงยังต่อเนื่องเป็นเส้นเดียว     */
    if ((cell->dot[0] | cell->dot[1] | cell->dot[2]) &&
        !((cell->base && cell->base != ' ') ||
          cell->top || cell->upper || cell->below)) {
        /*  graphics dot cell (plot): dot[0..2] คือ 20 แถวพิกเซลของเซลล์
         *  DOS แบ่งเป็น 3 กลุ่ม (0-6 / 7-13 / 14-19) — บน terminal ทั้ง 3
         *  กลุ่มอยู่ใน "แถวเดียวกัน" (เดิม py = y*2+row ทำให้กรอบเลื่อน
         *  ลง 1-2 แถวจนบน/ล่างหาย)
         *
         *  เลือก glyph จากโปรไฟล์บิตแนวตั้ง (dump ได้ด้วย CW_DUMP_DOT):
         *    d0/d1/d2 = ff   → เส้นนอน         "─"
         *    bit0 ทุกกลุ่ม    → เส้นขอบซ้าย    "│"
         *    bit7 ทุกกลุ่ม    → เส้นขอบขวา     "│"
         *    เส้นนอน + ขอบซ้าย/ขวา → มุม/สามแยก "┌ ┐ └ ┘ ├ ┤ ┼"
         *  ต้นฉบับเป็นเส้นกว้าง 1 พิกเซล — เดิมวาดครึ่งบล็อก (▀▄▌▐)
         *  ทำให้กรอบดูหนาเกินจริง                                     */
        unsigned d0 = cell->dot[0], d1 = cell->dot[1], d2 = cell->dot[2];
        /*  ตรวจว่าเส้นในเซลล์นี้ต่อไปยังเซลล์ข้างเคียงหรือไม่ แล้วเลือก
         *  อักษรเส้นบางให้ตรงรูป (มุม/สามแยก/กากบาท) — เส้นต้นฉบับ
         *  กว้าง 1 พิกเซล ครึ่งบล็อกจึงดูหนาเกินจริง                     */
        int has_h = dot_hstroke(cell);      /* มีช่วงเส้นนอน >= 2 px      */
        int vbit  = (int)(d0 & d1 & d2);    /* เส้นตั้ง (คอลัมน์ใดก็ได้)  */
        int up = 0, down = 0, lft = 0, rgt = 0;
        const char *glyph;

        /*  เส้นของต้นฉบับกว้าง 1 พิกเซลและลากผ่าน *ขอบ* เซลล์ เซลล์มุมจึง
         *  มีบิตเส้นไม่ครบ (มุมขวาล่างเหลือแค่ 2 พิกเซลปลายเส้น) — ตัดสิน
         *  รูปจากเซลล์ข้างเคียง ไม่ใช่จากบิตในเซลล์นี้เซลล์เดียว         */
        /*  เซลล์ที่ทับด้วย bitmap blit (over) ล้าง dot ทิ้ง แต่เส้น
         *  ต้นฉบับลากผ่านใต้รูป ต้องนับว่ายังต่อกันอยู่               */
        if (x > 0 && (dot_hstroke(&screen[y * PORT_SCREEN_COLS + x - 1]) ||
                      screen[y * PORT_SCREEN_COLS + x - 1].over))
            lft = 1;
        if (x + 1 < PORT_SCREEN_COLS &&
            (dot_hstroke(&screen[y * PORT_SCREEN_COLS + x + 1]) ||
             screen[y * PORT_SCREEN_COLS + x + 1].over))
            rgt = 1;
        if (y > 0 && dot_vstroke_any(&screen[(y - 1) * PORT_SCREEN_COLS + x]))
            up = 1;
        if (y + 1 < PORT_SCREEN_ROWS &&
            dot_vstroke_any(&screen[(y + 1) * PORT_SCREEN_COLS + x]))
            down = 1;

        /*  แถวบนสุดของแถบ UI: เส้นนอนย้ายไปเป็นขีดเส้นใต้แล้ว มุม ┌ ┐
         *  จึงเหลือแต่เส้นตั้ง — วาดเป็น │ ไม่งั้นจะเห็นมุมลอยไม่มีเส้น    */
        if (band && y == 0 && vbit && !up) {
            if ((int)y < getmaxy(stdscr))
                mvaddstr((int)y, (int)x, "\xe2\x94\x82");
            return;
        }

        if (up && down && lft && rgt)
            glyph = "\xe2\x94\xbc";              /* ┼  cross            */
        else if (down && lft && rgt)
            glyph = "\xe2\x94\xac";              /* ┬  top tee          */
        else if (up && lft && rgt)
            glyph = "\xe2\x94\xb4";              /* ┴  bottom tee       */
        else if (up && down && rgt)
            glyph = "\xe2\x94\x9c";              /* ├  left tee         */
        else if (up && down && lft)
            glyph = "\xe2\x94\xa4";              /* ┤  right tee        */
        else if (down && rgt)
            glyph = "\xe2\x94\x8c";              /* ┌  top-left         */
        else if (down && lft)
            glyph = "\xe2\x94\x90";              /* ┐  top-right        */
        else if (up && rgt)
            glyph = "\xe2\x94\x94";              /* └  bottom-left      */
        else if (up && lft)
            glyph = "\xe2\x94\x98";              /* ┘  bottom-right     */
        else if (lft && rgt)
            glyph = "\xe2\x94\x80";              /* ─  horizontal       */
        else if (up && down)
            glyph = "\xe2\x94\x82";              /* │  vertical         */
        else if (has_h)
            glyph = "\xe2\x94\x80";              /* ─  lone run         */
        else if (vbit)
            glyph = "\xe2\x94\x82";              /* │  lone run         */
        else
            glyph = "\xe2\x96\x92";              /* ▒  unknown          */

        /*  แถบ UI: เส้นนอนล้วน ๆ ถูกแทนด้วยขีดเส้นใต้ของแถวข้อความแล้ว
         *  (ดู ui_band_row) — ไม่วาดซ้ำ ไม่งั้นจะเห็นเส้นคู่            */
        if (band && !vbit && has_h && !up && !down) {
            if ((int)y < getmaxy(stdscr)) {
                attrset(A_UNDERLINE);
                mvaddch((int)y, (int)x, ' ');
                attrset(A_NORMAL);
            }
            return;
        }

        if ((int)y < getmaxy(stdscr))
            mvaddstr((int)y, (int)x, glyph);
        return;
    }

    if (cell->base == 0 && cell->top == 0 && cell->upper == 0 &&
        cell->below == 0) {
        /*  เซลล์ว่างแต่มี attr (เช่นช่องว่างในแถบ reverse ของ splash หรือ
         *  เซลล์ที่ไม่มีหมึกของ bitmap blit) ต้องวาดด้วย attr เดิม ไม่งั้น
         *  จะกลายเป็นช่องดำคั่นกลางแถบขาว                            */
        attrs = cell_attr(cell) & A_ATTRIBUTES;
        if (band)
            attrs |= A_UNDERLINE;
        attrset(attrs);
        mvaddch((int)y, (int)x, ' ');
        attrset(A_NORMAL);
        return;
    }

    /*  วาดเป็น UTF-8 ต่อเนื่องกันทั้งเซลล์ (base + วรรณยุกต์/สระที่ซ้อนอยู่)
     *  ถ้าส่งไบต์ TIS-620 ตรง ๆ ผ่าน mvaddch() terminal ที่เป็น UTF-8
     *  จะทิ้งตัวอักษรนั้นไปทั้งหมด จึงต้องแปลง TIS-620 -> Unicode ไทย
     *  (U+0E00 + code - 0xA0) ที่ชั้น boundary นี้                      */
    if (cell->base)
        n = put_utf8(buf, n, cell->base);
    if (cell->top)
        n = put_utf8(buf, n, cell->top);
    if (cell->upper)
        n = put_utf8(buf, n, cell->upper);
    if (cell->below)
        n = put_utf8(buf, n, cell->below);
    buf[n] = '\0';

    attrs = cell_attr(cell) & A_ATTRIBUTES;
    if (band)
        attrs |= A_UNDERLINE;
    attrset(attrs);
    mvaddstr((int)y, (int)x, buf);
    attrset(A_NORMAL);
}

/*  debug: ถ้าตั้ง CW_DUMP_SCREEN=<ไฟล์> จะ dump เนื้อ shadow screen
 *  (เป็น UTF-8) ลงไฟล์ทุกครั้งที่ flush — ใช้ตรวจว่าแอปวาดอะไรไว้จริง */
static void dump_screen_file(void)
{
    const char *path = getenv("CW_DUMP_SCREEN");
    FILE *f;
    unsigned int x, y;

    /*  CW_DUMP_ATTR ทำงานได้โดยไม่ต้องตั้ง CW_DUMP_SCREEN                */
    {
        const char *apath = getenv("CW_DUMP_ATTR");

        if (apath != NULL && *apath != '\0') {
            FILE *af = fopen(apath, "w");

            if (af != NULL) {
                for (y = 0; y < PORT_SCREEN_ROWS; y++) {
                    fprintf(af, "%2u ", y);
                    for (x = 0; x < PORT_SCREEN_COLS; x++)
                        fprintf(af, "%02x",
                                screen[y * PORT_SCREEN_COLS + x].attr);
                    fputc('\n', af);
                }
                fclose(af);
            }
        }
    }

    if (path == NULL || *path == '\0')
        return;
    f = fopen(path, "w");
    if (f == NULL)
        return;
    {
        /*  debug: CW_DUMP_DOT=<ไฟล์> จะ dump dot bitmap ของทุกเซลล์
         *  กราฟิก (ใช้ดูรูปร่างจริงของกรอบเพื่อเลือก glyph)              */
        const char *dpath = getenv("CW_DUMP_DOT");
        if (dpath != NULL && *dpath != '\0') {
            FILE *df = fopen(dpath, "w");
            if (df != NULL) {
                for (y = 0; y < PORT_SCREEN_ROWS; y++)
                    for (x = 0; x < PORT_SCREEN_COLS; x++) {
                        const port_cell *dc = &screen[y * PORT_SCREEN_COLS + x];
                        if (dc->dot[0] | dc->dot[1] | dc->dot[2])
                            fprintf(df, "y=%2u x=%2u d0=%02x d1=%02x d2=%02x\n",
                                    y, x, dc->dot[0], dc->dot[1], dc->dot[2]);
                    }
                fclose(df);
            }
        }
    }
    fprintf(f, "+");
    for (x = 0; x < PORT_SCREEN_COLS; x++)
        fputc('-', f);
    fprintf(f, "+\n");
    for (y = 0; y < PORT_SCREEN_ROWS; y++) {
        fputc('|', f);
        for (x = 0; x < PORT_SCREEN_COLS; x++) {
            const port_cell *c = &screen[y * PORT_SCREEN_COLS + x];
            char buf[24];
            int n = 0;

            if (c->dot[0] | c->dot[1] | c->dot[2])
                fputc('#', f);
            else {
                if (c->base)  n = put_utf8(buf, n, c->base);
                if (c->top)   n = put_utf8(buf, n, c->top);
                if (c->upper) n = put_utf8(buf, n, c->upper);
                if (c->below) n = put_utf8(buf, n, c->below);
                if (n == 0)
                    fputc(' ', f);
                else {
                    buf[n] = '\0';
                    fputs(buf, f);
                }
            }
        }
        fprintf(f, "|\n");
    }
    fprintf(f, "+");
    for (x = 0; x < PORT_SCREEN_COLS; x++)
        fputc('-', f);
    fprintf(f, "+\n");
    fclose(f);

}

void port_flush(void)
{
    unsigned int x, y;
    int mx, my;

    if (!port_active)
        return;

    /*  ตรวจการเปลี่ยนขนาด terminal ก่อนวาดทุกครั้ง
     *  shim อ่านคีย์เองผ่าน bioskey() ไม่ได้เรียก ncurses getch() จึงไม่มี
     *  ใครอ่าน KEY_RESIZE และ ncurses ก็ไม่อัปเดต LINES/COLS ให้
     *  -> ต้องถามขนาดจริงจาก kernel (ioctl TIOCGWINSZ) เอง แล้วเรียก
     *  resizeterm + mark ทุกเซลล์ dirty เพื่อวาดใหม่ทั้งจอ              */
    if (port_query_size(&my, &mx) && (mx != last_maxx || my != last_maxy)) {
        last_maxx = mx;
        last_maxy = my;
        resizeterm(my, mx);
        clearok(stdscr, TRUE);
        for (y = 0; y < PORT_SCREEN_ROWS; y++)
            for (x = 0; x < PORT_SCREEN_COLS; x++)
                dirty[y * PORT_SCREEN_COLS + x] = 1;
    }

    dump_screen_file();

    if (getenv("CW_DEBUG_FLUSH")) {
        unsigned int dx, dy, ndirty = 0;

        for (dy = 0; dy < PORT_SCREEN_ROWS; dy++)
            for (dx = 0; dx < PORT_SCREEN_COLS; dx++)
                if (dirty[dy * PORT_SCREEN_COLS + dx])
                    ndirty++;
        fprintf(stderr, "[flush] active=%d maxx=%d maxy=%d dirty=%u screen=%dx%d\n",
                port_active, getmaxx(stdscr), getmaxy(stdscr), ndirty,
                PORT_SCREEN_COLS, PORT_SCREEN_ROWS);
    }

    for (y = 0; y < PORT_SCREEN_ROWS; y++) {
        if ((int)y >= getmaxy(stdscr))
            break;
        for (x = 0; x < PORT_SCREEN_COLS; x++) {
            if (dirty[y * PORT_SCREEN_COLS + x]) {
                render_cell(x, y);
                dirty[y * PORT_SCREEN_COLS + x] = 0;
            }
        }
    }
    move(cur_y, cur_x);
    refresh();
}

/* ------------------------------------------------------------------ */
/* prchar / prblank  (EDISP.ASM _eprchar, _eprblank).                  */
/* ------------------------------------------------------------------ */
/* Write `c` into a slot with XOR-ish semantics: writing the same code */
/* twice clears it (the original XOR-blit behaviour).                  */
static void slot_xor(unsigned char *slot, unsigned char c)
{
    *slot = (*slot == c) ? 0 : c;
}

static void backend_prchar(unsigned int c, unsigned int attr,
                           unsigned int x, unsigned int y)
{
    port_cell *cell;

    if (!in_bounds(x, y))
        return;
    cell = &screen[y * PORT_SCREEN_COLS + x];

    c &= 0xFF;

    if (is_top((unsigned char)c)) {
        slot_xor(&cell->top, (unsigned char)c);
        mark_dirty(x, y);
        return;
    }
    if (is_upper((unsigned char)c)) {
        slot_xor(&cell->upper, (unsigned char)c);
        mark_dirty(x, y);
        return;
    }
    if (is_below((unsigned char)c)) {
        slot_xor(&cell->below, (unsigned char)c);
        mark_dirty(x, y);
        return;
    }

    /* middle-level: full cell rewrite (like the DOS mov, not xor)     */
    memset(cell, 0, sizeof(*cell));
    cell->base = (unsigned char)(c ? c : ' ');
    cell->attr = (unsigned char)attr;
    mark_dirty(x, y);
}

static void backend_prblank(unsigned int x, unsigned int y)
{
    port_cell *cell;

    if (!in_bounds(x, y))
        return;
    cell = &screen[y * PORT_SCREEN_COLS + x];
    memset(cell, 0, sizeof(*cell));
    mark_dirty(x, y);
}

/* ------------------------------------------------------------------ */
/* setcurpos  (EDISP.ASM _esetcurpos) - cursor, then flush the frame.  */
/* ------------------------------------------------------------------ */
static void backend_setcurpos(unsigned int x, unsigned int y, int thaimode)
{
    place_cursor(x, y, thaimode);
    port_flush();
}

/* ------------------------------------------------------------------ */
/* plot  (EDISP.ASM _eplot) - coarse dot approximation, see port_cell. */
/* ------------------------------------------------------------------ */
static void backend_plot(unsigned int x, unsigned int y)
{
    port_cell *cell;
    unsigned int cx, cy, row, col;

    cx = x / 8;
    cy = y / 20;
    if (!in_bounds(cx, cy))
        return;
    cell = &screen[cy * PORT_SCREEN_COLS + cx];

    /* two terminal rows per cell - map row 0-6 -> 0, 7-13 -> 1,       */
    /* 14-19 -> 2 (matches the port_cell.dot layout).                  */
    row = (y % 20) / 7;
    if (row > 2)
        row = 2;
    col = x % 8;
    cell->dot[row] |= (unsigned char)(1u << col);
    mark_dirty(cx, cy);
}

/* ------------------------------------------------------------------ */
/* savepic / retpic  (EDISP.ASM _esavepic / _eretpic).                 */
/* The DOS version copied the whole screen to a 32K offscreen buffer;  */
/* the port serialises the shadow screen into the same byte budget.    */
/* ------------------------------------------------------------------ */
static void serialize_cell(const port_cell *cell, unsigned char *out)
{
    memset(out, 0, PORT_CELL_SIZE);
    out[0] = cell->base;
    out[1] = cell->top;
    out[2] = cell->upper;
    out[3] = cell->below;
    out[4] = cell->attr;
    out[5] = cell->dot[0];
    out[6] = cell->dot[1];
    out[7] = cell->dot[2];
    out[8] = cell->over;
}

static void deserialize_cell(const unsigned char *in, port_cell *cell)
{
    memset(cell, 0, sizeof(*cell));
    cell->base   = in[0];
    cell->top    = in[1];
    cell->upper  = in[2];
    cell->below  = in[3];
    cell->attr   = in[4];
    cell->dot[0] = in[5];
    cell->dot[1] = in[6];
    cell->dot[2] = in[7];
    cell->over   = in[8];
}

static void backend_savepic(void)
{
    unsigned int x, y, i;

    if (screen_buffptr == NULL)
        return;
    i = 0;
    for (y = 0; y < PORT_SCREEN_ROWS; y++) {
        for (x = 0; x < PORT_SCREEN_COLS; x++) {
            serialize_cell(&screen[y * PORT_SCREEN_COLS + x],
                           &screen_buffptr[i]);
            i += PORT_CELL_SIZE;
        }
    }
}

static void backend_retpic(void)
{
    unsigned int x, y, i;

    if (screen_buffptr == NULL)
        return;
    i = 0;
    for (y = 0; y < PORT_SCREEN_ROWS; y++) {
        for (x = 0; x < PORT_SCREEN_COLS; x++) {
            deserialize_cell(&screen_buffptr[i],
                             &screen[y * PORT_SCREEN_COLS + x]);
            dirty[y * PORT_SCREEN_COLS + x] = 1;
            i += PORT_CELL_SIZE;
        }
    }
    port_flush();
}

/* ------------------------------------------------------------------ */
/* clsall  (EDISP.ASM _eclsall).                                       */
/* ------------------------------------------------------------------ */
static void backend_clsall(void)
{
    memset(screen, 0, sizeof(screen));
    memset(dirty, 1, sizeof(dirty));
    clear();
    port_flush();
}

/* ------------------------------------------------------------------ */
/* clsgraph  (EDISP.ASM _eclsgraph) - args (x1, y1, x2, y2) inclusive. */
/* ------------------------------------------------------------------ */
static void backend_clsgraph(unsigned int x1, unsigned int y1,
                             unsigned int x2, unsigned int y2)
{
    unsigned int x, y;

    for (y = y1; y <= y2 && y < PORT_SCREEN_ROWS; y++)
        for (x = x1; x <= x2 && x < PORT_SCREEN_COLS; x++)
            backend_prblank(x, y);
}

/* ------------------------------------------------------------------ */
/* clrline  (EDISP.ASM _eclrline) - clear row y1 from x1 to x2.        */
/* ------------------------------------------------------------------ */
static void backend_clrline(unsigned int x1, unsigned int y1,
                            unsigned int x2)
{
    backend_clsgraph(x1, y1, x2, y1);
}

/* ------------------------------------------------------------------ */
/* prakeaw  (EDISP.ASM _eprakeaw / HDISP.ASM _hprakeaw) - พระเกี้ยว    */
/*                                                                    */
/*  ต้นฉบับ (_hprakeaw, HDISP.ASM:1073) เขียน 20 word ต่อบล็อก ลง      */
/*  video memory ตรง ๆ โดย word ที่ n ของแต่ละบล็อกคือ scanline         */
/*  หนึ่งแถว ๆ ละ 16 พิกเซล: chulalogo1 (HDISP.ASM:152) ที่ byte        */
/*  column 1 (px 8-23) และ chulalogo2 (บรรทัด 157) ที่ byte column 3    */
/*  (px 24-39) รวมเป็นรูป 32x20 พิกเซล เริ่มที่ scanline 2              */
/*                                                                    */
/*  ลำดับไบต์: `mov es:[di],ax` เก็บ AL ที่ address ต่ำ (= ซ้าย) และ    */
/*  AH ที่ address สูง (= ขวา) จึงเป็น little-endian; ภายในไบต์บิต 7    */
/*  คือพิกเซลซ้ายสุด                                                  */
/*                                                                    */
/*  งาน port: **ย้าย**พระเกี้ยวไปวาดที่ splash screen แทนแถบชื่อ        */
/*  มหาวิทยาลัย — ไทเทิลบาร์มีที่ให้แค่ 4 เซลล์ x 1 บรรทัด (เพดาน 8x4    */
/*  ดอท) ย่อแล้วเหลือก้อนทึบดูไม่ออกว่าเป็นรูป ส่วน splash มีที่ว่าง     */
/*  ซ้ายกล่อง 16 เซลล์ x 5 บรรทัด = 32x20 ดอท ⇒ map 1:1 กับ bitmap      */
/*  ต้นฉบับทุกพิกเซล (ดู `port_draw_crest`) — ต้นฉบับเรียก `prakeaw()`  */
/*  ตอนเปิดจอ จึงคงฟังก์ชันไว้แต่ไม่วาดในไทเทิลบาร์                      */
/* ------------------------------------------------------------------ */
static const unsigned short chulalogo1[20] = {
    0x0200, 0x3200, 0x0d00, 0xfb03,
    0x1d00, 0xe500, 0x9b03, 0x320c,
    0xe400, 0xdf01, 0xb507, 0x1f0e,
    0x0c00, 0x3f00, 0xe001, 0x0027,
    0xff5f, 0x0027, 0xe051, 0x3f00
};

static const unsigned short chulalogo2[20] = {
    0x0040, 0x004c, 0x00b0, 0xc0df,
    0x00b8, 0x00a7, 0xc0d9, 0x304c,
    0x0027, 0x80fb, 0xe0ad, 0x70f8,
    0x0030, 0x00fc, 0x8007, 0xe400,
    0xfaff, 0xe400, 0x8a07, 0x00fc
};

/*  พิกเซล (col, row) ของพระเกี้ยว 32x20 จาก bitmap ต้นฉบับ
 *  col 0-15 = chulalogo1, col 16-31 = chulalogo2; ภายใน word บิต 7-0
 *  คือ 8 พิกเซลแรก และบิต 15-8 คือ 8 พิกเซลหลัง (little-endian
 *  ตามที่ _hprakeaw เขียนด้วย mov es:[di],ax)                        */
static unsigned int crest_pixel(unsigned int col, unsigned int row)
{
    const unsigned short *blk;
    unsigned int n, bit;

    if (col >= 32 || row >= 20)
        return 0;

    blk = (col < 16) ? chulalogo1 : chulalogo2;
    n   = (col < 16) ? col : col - 16;
    bit = (n < 8) ? (7 - n) : (15 - (n - 8));
    return ((unsigned int)blk[row] >> bit) & 1u;
}

/*  วาดพระเกี้ยวลงตารางเซลล์ (x, y) ขนาด cols x rows เซลล์
 *  หนึ่งเซลล์แสดงได้ 2x4 ดอทของ Braille และหนึ่งดอท = หนึ่งพิกเซล
 *  ของ bitmap ⇒ 32x20 พิกเซล = 16x5 เซลล์ (1:1 ไม่ย่อ)              */
void port_draw_crest(unsigned int x, unsigned int y,
                     unsigned int cols, unsigned int rows, font_attr attr)
{
    unsigned int cx, cy;

    if (!port_active)
        return;

    for (cy = 0; cy < rows; cy++)
        for (cx = 0; cx < cols; cx++) {
            unsigned int mask = 0;
            port_cell *p;
            int k;

            if (!in_bounds(x + cx, y + cy))
                continue;

            for (k = 0; k < 4; k++) {
                unsigned int py = cy * 4 + (unsigned int)k;

                if (py >= 20)
                    break;
                if (crest_pixel(cx * 2, py))
                    mask |= braille_bit[0][k];
                if (crest_pixel(cx * 2 + 1, py))
                    mask |= braille_bit[1][k];
            }

            p = &screen[(y + cy) * PORT_SCREEN_COLS + (x + cx)];
            memset(p, 0, sizeof(*p));
            p->over = (unsigned char)mask;
            /*  ตั้ง attr ให้ทุกเซลล์ (รวมเซลล์ที่ไม่มีหมึก) เพื่อให้พื้น
             *  ของรูปกลืนกับแถบ splash ที่เป็น reverse video           */
            p->attr = (unsigned char)attr;
            mark_dirty(x + cx, y + cy);
        }
}

static void backend_prakeaw(void)
{
    /*  งาน port: พระเกี้ยวถูกย้ายไปวาดที่ splash screen ผ่าน
     *  port_draw_crest() จึงไม่วาดอะไรในไทเทิลบาร์อีก                */
}

/* ------------------------------------------------------------------ */
/* rdchardot / wrchardot  (EDISP.ASM _erdchardot / _ewrchardot).       */
/* ------------------------------------------------------------------ */
static void backend_rdchardot(unsigned int x, unsigned int y)
{
    port_cell *cell;
    int i;

    if (!in_bounds(x, y)) {
        memset(port_chardot, 0, PORT_CELL_SIZE);
        return;
    }
    cell = &screen[y * PORT_SCREEN_COLS + x];
    serialize_cell(cell, port_chardot);
    for (i = 0; i < PORT_CELL_SIZE; i++)
        port_chardot[i] = port_chardot[i]; /* keep buffer semantics */
}

static void backend_wrchardot(unsigned int x, unsigned int y)
{
    port_cell *cell;

    if (!in_bounds(x, y))
        return;
    cell = &screen[y * PORT_SCREEN_COLS + x];
    deserialize_cell(port_chardot, cell);
    mark_dirty(x, y);
}

/* ------------------------------------------------------------------ */
/* getwind / putwind  (EDISP.ASM _egetwind / _eputwind).               */
/* Buffers keep the DOS size: linecount scanline rows x bytecount      */
/* columns, 20 bytes per row-record, starting at (x1, y1).             */
/* ------------------------------------------------------------------ */
void hgetwind(unsigned int x1, unsigned int y1, unsigned int linecount,
              unsigned int bytecount, char *buffer);
void hputwind(unsigned int x1, unsigned int y1, unsigned int linecount,
              unsigned int bytecount, char *buffer);

void hgetwind(unsigned int x1, unsigned int y1, unsigned int linecount,
              unsigned int bytecount, char *buffer)
{
    unsigned int cols = bytecount;                  /* คอลัมน์ของหน้าต่าง */
    unsigned int rows = linecount / PORT_CELL_SIZE; /* แถวเซลล์          */
    unsigned int r, c, s;

    if (buffer == NULL || cols == 0)
        return;

    /*  DOS layout: buffer[(scanline) * cols + column] หนึ่งไบต์ต่อ
     *  หนึ่งพิกเซลแถวของเซลล์ (cell = 8x20 px) ⇒ linecount แถวพิกเซล
     *  x cols คอลัมน์ · port เก็บสถานะเซลล์ได้ 9 ไบต์ จึงเขียนลง
     *  scanline 0..8 ของคอลัมน์นั้น และปล่อย 9..19 เป็นศูนย์
     *  (ขนาดบัฟเฟอร์ยังเท่าต้นฉบับ: rows*20*cols)                    */
    memset(buffer, 0, (size_t)linecount * cols);

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            unsigned int x = x1 + c;
            unsigned int y = y1 + r;
            unsigned char tmp[PORT_CELL_SIZE];

            if (x < PORT_SCREEN_COLS && y < PORT_SCREEN_ROWS)
                serialize_cell(&screen[y * PORT_SCREEN_COLS + x], tmp);
            else
                memset(tmp, 0, sizeof(tmp));

            for (s = 0; s < PORT_CELL_SIZE; s++)
                buffer[(r * PORT_CELL_SIZE + s) * cols + c] = (char)tmp[s];
        }
    }
}

void hputwind(unsigned int x1, unsigned int y1, unsigned int linecount,
              unsigned int bytecount, char *buffer)
{
    unsigned int cols = bytecount;
    unsigned int rows = linecount / PORT_CELL_SIZE;
    unsigned int r, c, s;

    if (buffer == NULL || cols == 0)
        return;

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            unsigned int x = x1 + c;
            unsigned int y = y1 + r;
            unsigned char tmp[PORT_CELL_SIZE];

            memset(tmp, 0, sizeof(tmp));
            for (s = 0; s < PORT_CELL_SIZE; s++)
                tmp[s] = (unsigned char)buffer[(r * PORT_CELL_SIZE + s) * cols + c];

            if (x < PORT_SCREEN_COLS && y < PORT_SCREEN_ROWS) {
                deserialize_cell(tmp, &screen[y * PORT_SCREEN_COLS + x]);
                mark_dirty(x, y);
            }
        }
    }
    port_flush();
}

/* ------------------------------------------------------------------ */
/* Adapter aliases.  All six DOS adapters route to the ncurses         */
/* backend; the aliases keep the original link surface.                */
/* ------------------------------------------------------------------ */
void hprchar(unsigned int c, unsigned int attr, unsigned int x, unsigned int y)
    { backend_prchar(c, attr, x, y); }
void eprchar(unsigned int c, unsigned int attr, unsigned int x, unsigned int y)
    { backend_prchar(c, attr, x, y); }
void aprchar(unsigned int c, unsigned int attr, unsigned int x, unsigned int y)
    { backend_prchar(c, attr, x, y); }

void hprblank(unsigned int x, unsigned int y) { backend_prblank(x, y); }
void eprblank(unsigned int x, unsigned int y) { backend_prblank(x, y); }
void aprblank(unsigned int x, unsigned int y) { backend_prblank(x, y); }

void hsetcurpos(unsigned int x, unsigned int y, int thaimode)
    { backend_setcurpos(x, y, thaimode); }
void esetcurpos(unsigned int x, unsigned int y, int thaimode)
    { backend_setcurpos(x, y, thaimode); }
void asetcurpos(unsigned int x, unsigned int y, int thaimode)
    { backend_setcurpos(x, y, thaimode); }

void hplot(unsigned int x, unsigned int y) { backend_plot(x, y); }
void eplot(unsigned int x, unsigned int y) { backend_plot(x, y); }
void aplot(unsigned int x, unsigned int y) { backend_plot(x, y); }

void hsetgraph(void) { /* per-adapter init is a no-op; setgraph() below
                          owns terminal setup */ }
void esetgraph(void) { }
void asetgraph(void) { }

void hsettext(void) { }
void esettext(void) { }
void asettext(void) { }

void hsavepic(void) { backend_savepic(); }
void esavepic(void) { backend_savepic(); }
void asavepic(void) { backend_savepic(); }

void hretpic(void) { backend_retpic(); }
void eretpic(void) { backend_retpic(); }
void aretpic(void) { backend_retpic(); }

void hclsall(void) { backend_clsall(); }
void eclsall(void) { backend_clsall(); }
void aclsall(void) { backend_clsall(); }

void hprakeaw(void) { backend_prakeaw(); }
void eprakeaw(void) { backend_prakeaw(); }
void aprakeaw(void) { backend_prakeaw(); }

void hclsgraph(unsigned int x1, unsigned int y1,
               unsigned int x2, unsigned int y2)
    { backend_clsgraph(x1, y1, x2, y2); }
void eclsgraph(unsigned int x1, unsigned int y1,
               unsigned int x2, unsigned int y2)
    { backend_clsgraph(x1, y1, x2, y2); }
void aclsgraph(unsigned int x1, unsigned int y1,
               unsigned int x2, unsigned int y2)
    { backend_clsgraph(x1, y1, x2, y2); }

void hclrline(unsigned int x1, unsigned int y1, unsigned int x2)
    { backend_clrline(x1, y1, x2); }
void eclrline(unsigned int x1, unsigned int y1, unsigned int x2)
    { backend_clrline(x1, y1, x2); }
void aclrline(unsigned int x1, unsigned int y1, unsigned int x2)
    { backend_clrline(x1, y1, x2); }

void hrdchardot(unsigned int x, unsigned int y) { backend_rdchardot(x, y); }
void erdchardot(unsigned int x, unsigned int y) { backend_rdchardot(x, y); }
void ardchardot(unsigned int x, unsigned int y) { backend_rdchardot(x, y); }

void hwrchardot(unsigned int x, unsigned int y) { backend_wrchardot(x, y); }
void ewrchardot(unsigned int x, unsigned int y) { backend_wrchardot(x, y); }
void awrchardot(unsigned int x, unsigned int y) { backend_wrchardot(x, y); }

/* put/getwind are dispatched straight through the pointers, exactly   */
/* like CSCRN.C savescrn/resscrn did on DOS.                           */

/* ------------------------------------------------------------------ */
/* setgraph - port of CWGRPHC.C setgraph: allocate the screen buffer,  */
/* select the backend (always ncurses) and bind the dispatch table.    */
/* ------------------------------------------------------------------ */
/*  ---- terminal restore ------------------------------------------------
 *  ปิดโปรแกรมแล้วต้องคืน terminal ให้ shell ใช้ต่อได้:
 *    - endwin() ออกจาก ncurses (โชว์ cursor, ปิด keypad/alternate screen)
 *    - คืน termios ดิบที่เก็บไว้ *ก่อน* initscr() — ไม่ใช่ค่าที่ ncurses
 *      ตั้งไว้ (cbreak/noecho) เพราะ shim บันทึก termios หลัง ncurses
 *      ทำงานแล้ว ทำให้ atexit ของ shim คืนค่า noecho กลับมาที่ shell
 *      (อาการ: alt+x ออกแล้วพิมพ์อะไรก็ไม่ขึ้น ใช้ terminal ต่อไม่ได้)
 *  เรียกทั้งตอน exit ปกติและเมื่อโดนสัญญาณ                        */
static struct termios port_orig_tio;
static int            port_orig_tio_saved;

static void port_cleanup_term(void)
{
    if (port_active) {
        endwin();
        port_active = 0;
    }
    if (port_orig_tio_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &port_orig_tio);
        port_orig_tio_saved = 0;
    }
}

static void port_signal_exit(int sig)
{
    port_cleanup_term();
    _exit(128 + sig);
}

void setgraph(void)
{
    static int first = 1;

    if (!first) {
        (*setgraph_ptr)();
        return;
    }
    first = 0;

    screen_buffptr = malloc(PORT_SCREEN_BYTES);
    if (screen_buffptr == NULL) {
        fputs("Not enough memory\n", stderr);
        exit(1);
    }
    memset(screen_buffptr, 0, PORT_SCREEN_BYTES);

    if (scrmode == DETECT)
        scrmode = VGA;                /* terminal = one adapter        */

    prchar_ptr     = backend_prchar;
    prblank_ptr    = backend_prblank;
    setcurpos_ptr  = backend_setcurpos;
    plot_ptr       = backend_plot;
    setgraph_ptr   = hsetgraph;
    settext_ptr    = port_settext;
    savepic_ptr    = backend_savepic;
    retpic_ptr     = backend_retpic;
    clsall_ptr     = backend_clsall;
    clsgraph_ptr   = backend_clsgraph;
    clrline_ptr    = backend_clrline;
    prakeaw_ptr    = backend_prakeaw;
    rdchardot_ptr  = backend_rdchardot;
    wrchardot_ptr  = backend_wrchardot;
    putwind_ptr    = hputwind;
    getwind_ptr    = hgetwind;

    /* ncurses init
     *  setlocale ต้องมาก่อน initscr(): ncursesw ตัดสินใจว่าจะแปลงสตริง
     *  เป็น wide char ด้วย locale ใด ถ้าไม่เรียก ระบบจะอยู่ใน "C" locale
     *  แล้ว mvaddstr() จะทิ้งไบต์ >= 0x80 ทั้งหมด -> ตัวอักษรไทยหายเงียบ ๆ
     *  (ASCII ยังขึ้นปกติ จึงดูเหมือนจอวาดได้แต่ไทยว่าง)                     */
    setlocale(LC_ALL, "");

    /*  เก็บ termios ก่อน ncurses แตะ แล้วคืนตอนออกจากโปรแกรม          */
    if (isatty(STDIN_FILENO) &&
        tcgetattr(STDIN_FILENO, &port_orig_tio) == 0) {
        port_orig_tio_saved = 1;
    }
    atexit(port_cleanup_term);
    signal(SIGTERM, port_signal_exit);
    signal(SIGHUP,  port_signal_exit);
    signal(SIGQUIT, port_signal_exit);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);   /* reversed          */
    use_default_colors();
    erase();
    port_active = 1;
}

/* ------------------------------------------------------------------ */
/* settext implementation - bound to settext_ptr (port of the int10h   */
/* mode-3 call).  The `settext()` macro in cwgrphc.h dispatches here.  */
/* ------------------------------------------------------------------ */
void port_settext(void)
{
    if (!port_active)
        return;
    port_active = 0;
    endwin();
}

int port_getch(void)
{
    int c;

    if (!port_active)
        return -1;
    port_flush();
    for (;;) {
        c = getch();
        /*  ncurses ส่ง KEY_RESIZE มาเมื่อ terminal เปลี่ยนขนาด — ไม่ใช่คีย์
         *  ที่โปรแกรมรู้จัก ต้องกลืนทิ้งแล้ววาดจอใหม่ (mark ทุกเซลล์ dirty)
         *  ถ้าส่งต่อให้ core จะถูกตีความเป็นอักขระมั่ว ๆ                  */
        if (c == KEY_RESIZE) {
            resizeterm(getmaxy(stdscr), getmaxx(stdscr));
            clearok(stdscr, TRUE);
            last_maxx = getmaxx(stdscr);
            last_maxy = getmaxy(stdscr);
            {
                unsigned int x, y;
                for (y = 0; y < PORT_SCREEN_ROWS; y++)
                    for (x = 0; x < PORT_SCREEN_COLS; x++)
                        dirty[y * PORT_SCREEN_COLS + x] = 1;
            }
            port_flush();
            continue;
        }
        break;
    }
    return c;
}

/* Control-C / crash safety: endwin on abnormal exit keeps the         */
/* terminal usable.                                                    */
__attribute__((destructor)) static void port_atexit(void)
{
    if (port_active)
        endwin();
}
