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
 *  ไบต์ 0x80-0xA0 และ 0xFC-0xFF (สัญลักษณ์/กล่อง) ส่งเป็น Latin-1 2 ไบต์  */
static int put_utf8(char *buf, int n, unsigned char c)
{
    if (c >= 0xA1 && c <= 0xFB) {
        unsigned int u = 0x0E00u + (unsigned int)(c - 0xA0);
        buf[n++] = (char)(0xE0 | (u >> 12));
        buf[n++] = (char)(0x80 | ((u >> 6) & 0x3F));
        buf[n++] = (char)(0x80 | (u & 0x3F));
    }
    else if (c >= 0x80) {
        buf[n++] = (char)(0xC0 | (c >> 6));
        buf[n++] = (char)(0x80 | (c & 0x3F));
    }
    else {
        buf[n++] = (char)c;
    }
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

static void render_cell(unsigned int x, unsigned int y)
{
    port_cell *cell = &screen[y * PORT_SCREEN_COLS + x];
    char buf[24];
    int n = 0;
    chtype attrs;

    if (x >= (unsigned)getmaxx(stdscr))
        return;

    if (cell->dot[0] | cell->dot[1] | cell->dot[2]) {
        /*  graphics dot cell (plot): dot[0..2] คือ 20 แถวพิกเซลของเซลล์
         *  DOS แบ่งเป็น 3 กลุ่ม (0-6 / 7-13 / 14-19) — บน terminal ทั้ง 3
         *  กลุ่มอยู่ใน "แถวเดียวกัน" (เดิม py = y*2+row ทำให้กรอบเลื่อน
         *  ลง 1-2 แถวจนบน/ล่างหาย)
         *
         *  เลือก glyph จากโปรไฟล์บิตแนวตั้ง (dump ได้ด้วย CW_DUMP_DOT):
         *    d0=ff           → แถบบน      "▀"  (แถบชื่อเรื่อง/เส้นบน)
         *    d2=ff           → แถบล่าง     "▄"  (แถบปิดท้ายจอ)
         *    bit0 ทุกกลุ่ม    → เส้นขอบซ้าย  "▌"
         *    bit7 ทุกกลุ่ม    → เส้นขอบขวา   "▐"
         *    แน่นทั้งเซลล์     → "█"  ·  กรณีอื่น fallback → "▒"
         *  เดิมวาด ACS_CKBOARD ทุกเซลล์ ทำให้กรอบเป็นบล็อกเทาทึบ        */
        unsigned d0 = cell->dot[0], d1 = cell->dot[1], d2 = cell->dot[2];
        int band_top = (d0 == 0xff) || (d1 == 0xff);
        int band_bottom = (d1 == 0xff) || (d2 == 0xff);
        int edge_left = (d0 & 0x01) || (d1 & 0x01) || (d2 & 0x01);
        int edge_right = (d0 & 0x80) || (d1 & 0x80) || (d2 & 0x80);
        const char *glyph;

        if (band_top && band_bottom)
            glyph = "\xe2\x96\x88";              /* █ full block        */
        else if (band_top)
            glyph = "\xe2\x96\x80";              /* ▀ upper half block  */
        else if (band_bottom)
            glyph = "\xe2\x96\x84";              /* ▄ lower half block  */
        else if (edge_left && !edge_right)
            glyph = "\xe2\x96\x8c";              /* ▌ left half block   */
        else if (edge_right && !edge_left)
            glyph = "\xe2\x96\x90";              /* ▐ right half block  */
        else
            glyph = "\xe2\x96\x92";              /* ▒ dither (fallback) */

        if ((int)y < getmaxy(stdscr))
            mvaddstr((int)y, (int)x, glyph);
        return;
    }

    if (cell->base == 0 && cell->top == 0 && cell->upper == 0 &&
        cell->below == 0) {
        mvaddch((int)y, (int)x, ' ');
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
/* prakeaw  (EDISP.ASM _eprakeaw) - Chulalongkorn crest logo at the    */
/* upper-left corner.  The DOS bitmaps (chulalogo1/2) are 16x20 pixel  */
/* patterns; the port draws a two-cell mark covering the same area.    */
/* ------------------------------------------------------------------ */
static void backend_prakeaw(void)
{
    port_cell *c1, *c2;

    if (!port_active)
        return;
    c1 = &screen[2 * PORT_SCREEN_COLS + 1];
    c2 = &screen[2 * PORT_SCREEN_COLS + 3];
    memset(c1, 0, sizeof(*c1));
    memset(c2, 0, sizeof(*c2));
    c1->base = (unsigned char)0xA4;   /*   phi  */
    c2->base = (unsigned char)0xAE;   /*  tor   */
    mark_dirty(1, 2);
    mark_dirty(3, 2);
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
    unsigned int row, x, y, i, n;

    if (buffer == NULL)
        return;
    n = bytecount < PORT_CELL_SIZE ? bytecount : PORT_CELL_SIZE;
    /* DOS layout: record r (r = 0..linecount-1) is scanline           */
    /* (y1*20 + r) of column strip x1, stored at buffer[r*bytecount].  */
    /* One 20-byte record per screen-cell row: cell row y uses records */
    /* y*20 .. y*20+19, all holding the same cell state (the cell is   */
    /* the unit; splitting it across records would lose overlays).     */
    /* Only the first `n` bytes of each record are written when the    */
    /* caller's records are narrower than a full cell.                 */
    for (row = 0; row < linecount; row++) {
        y = y1 + row / PORT_CELL_SIZE;
        x = x1;
        i = row * bytecount;
        if (y < PORT_SCREEN_ROWS && x < PORT_SCREEN_COLS) {
            unsigned char tmp[PORT_CELL_SIZE];
            serialize_cell(&screen[y * PORT_SCREEN_COLS + x], tmp);
            memcpy(&buffer[i], tmp, n);
        }
        else {
            memset(&buffer[i], 0, n);
        }
    }
}

void hputwind(unsigned int x1, unsigned int y1, unsigned int linecount,
              unsigned int bytecount, char *buffer)
{
    unsigned int row, x, y, i, n;

    if (buffer == NULL)
        return;
    n = bytecount < PORT_CELL_SIZE ? bytecount : PORT_CELL_SIZE;
    for (row = 0; row < linecount; row++) {
        unsigned char tmp[PORT_CELL_SIZE];
        i = row * bytecount;
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, &buffer[i], n);
        y = y1 + row / PORT_CELL_SIZE;
        x = x1;
        if (y < PORT_SCREEN_ROWS && x < PORT_SCREEN_COLS) {
            deserialize_cell(tmp, &screen[y * PORT_SCREEN_COLS + x]);
            mark_dirty(x, y);
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
