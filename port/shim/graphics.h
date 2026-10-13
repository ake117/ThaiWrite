/* =====================================================================
 * graphics.h - Turbo C BGI <graphics.h> replacement for the ThaiWrite
 * Linux port (used only by the font editor, SRC/SCUF).
 * ---------------------------------------------------------------------
 * A small BGI-compatible raster engine is implemented in shim_graphics.c
 * on top of a 640x480 1bpp framebuffer.  getimage()/putimage()/imagesize()
 * use the shim's own (self-consistent) image layout, so the "malloc
 * imagesize(); getimage(); putimage()" idiom in SRC/SCUF works unchanged.
 *
 * Hardware BGI drivers cannot exist here: initgraph() just selects a
 * raster mode, and registerbgidriver()/registerfarbgidriver() are no-ops.
 * ===================================================================== */
#ifndef _SHIM_GRAPHICS_H
#define _SHIM_GRAPHICS_H

#include "shim_turbo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- drivers / modes (same numbering as Turbo C) ------ */
enum graphics_drivers
{
    DETECT, CGA, MCGA, EGA, EGA64, EGAMONO, IBM8514, HERCMONO, ATT400,
    VGA, PC3270
};

enum graphics_modes
{
    CGAC0 = 0, CGAC1, CGAC2, CGAC3, CGAHI,
    MCGAC0 = 0, MCGAC1, MCGAC2, MCGAC3, MCGAMED, MCGAHI,
    EGALOW = 0, EGAHI,
    EGA64LO = 0, EGA64HI,
    EGAMONOHI = 3,
    HERCMONOHI = 0,
    ATT400C0 = 0, ATT400C1, ATT400C2, ATT400C3, ATT400MED, ATT400HI,
    VGAHI = 2,
    PC3270HI = 0,
    IBM8514LO = 0, IBM8514HI
};

/* ---------------- line styles / widths ----------------------------- */
enum line_styles
{
    SOLID_LINE, DOTTED_LINE, CENTER_LINE, DASHED_LINE, USERBIT_LINE
};

enum line_widths { NORM_WIDTH = 1, THICK_WIDTH = 3 };

/* ---------------- fill styles -------------------------------------- */
enum fill_styles
{
    EMPTY_FILL, SOLID_FILL, LINE_FILL, LTSLASH_FILL, SLASH_FILL,
    BKSLASH_FILL, LTBKSLASH_FILL, HATCH_FILL, XHATCH_FILL,
    INTERLEAVE_FILL, WIDE_DOT_FILL, CLOSE_DOT_FILL, USER_FILL
};

/* ---------------- putimage operators ------------------------------- */
enum putimage_ops { COPY_PUT, XOR_PUT, OR_PUT, AND_PUT, NOT_PUT };

/* ---------------- colours (same numbering as Turbo C) -------------- */
enum COLORS
{
    BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY,
    DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED,
    LIGHTMAGENTA, YELLOW, WHITE
};

/* ---------------- viewport ----------------------------------------- */
struct viewporttype
{
    int left, top, right, bottom;
    int clip;
};

struct arccoordstype
{
    int x, y, xstart, ystart, xend, yend;
};

/* BGI's attribute "settings" structures - getlinesettings() and
 * getfillsettings() take one of these, exactly as in Borland BGI. */
struct linesettingstype
{
    int linestyle;
    unsigned upattern;
    int thickness;
};

struct fillsettingstype
{
    int pattern;
    int color;
};

struct textsettingstype
{
    int font;
    int direction;
    int charsize;
};

struct palettetype
{
    unsigned char size;
    signed char colors[16];
};

/* ---------------- graphresult() error codes (as in Turbo C) -------- */
#define grOk               0
#define grNoInitGraph      (-1)
#define grNotDetected      (-2)
#define grFileNotFound     (-3)
#define grInvalidDriver    (-4)
#define grNoLoadMem        (-5)
#define grNoScanMem        (-6)
#define grNoFloodMem       (-7)
#define grFontNotFound     (-8)
#define grNoFontMem        (-9)
#define grInvalidMode      (-10)
#define grError            (-11)
#define grIOerror          (-12)
#define grInvalidFont      (-13)
#define grInvalidFontNum   (-14)
#define grInvalidVersion   (-18)

/* ---------------- BGI driver images (no-ops here) ------------------ */
extern unsigned char EGAVGA_driver_far[];
extern unsigned char Herc_driver_far[];
extern unsigned char EGAVGA_font_far[];

/* ---------------- control ------------------------------------------ */
void initgraph(int *graphdriver, int *graphmode, const char *pathtodriver);
void detectgraph(int *graphdriver, int *graphmode);
void closegraph(void);
void restorecrtmode(void);
int  graphresult(void);
const char *grapherrormsg(int errcode);
int  registerbgidriver(void (*driver)(void));
int  registerfarbgidriver(void *driver);
int  registerbgifont(void (*font)(void));
int  registerfarbgifont(void *font);
int  installuserdriver(const char *name, int (*detect)(void));
int  installuserfont(const char *name);

void setactivepage(int page);
void setvisualpage(int page);
void cleardevice(void);
void clearviewport(void);
void setviewport(int left, int top, int right, int bottom, int clip);
void getviewsettings(struct viewporttype *viewport);

/* ---------------- port additions ---------------------------------- */
/* The raster surface IS the emulated video memory (see shim_dos.h):
 * 0xB0000000 for the Herc/EGA-mono modes, 0xA0000000 for VGA/MCGA/CGA.
 * These accessors let a terminal backend blit it.  A pixel (x,y) is one
 * bit: byte x/8, bit 0x80>>(x&7), exactly as SRC/SCUF/WRITE.C's
 * hgcwritech()/ewritech() lay glyphs out, so the font editor's own
 * glyph driver and the BGI drawing land in the same framebuffer. */
unsigned char *shim_bgi_surface(void);
int  shim_bgi_surface_banked(void);   /* 1 = 4-bank Herc interleave */
int  shim_bgi_surface_pitch(void);    /* bytes per surface scanline */
int  shim_bgi_surface_width(void);
int  shim_bgi_surface_height(void);
int  shim_bgi_active(void);

/* All BGI coordinates are relative to the current viewport origin, and
 * clipping is applied when the viewport was created with clip != 0 -
 * same as Borland BGI.  Define SHIM_BGI_ABSOLUTE_COORDS to build the
 * absolute variant instead. */

/* ---------------- drawing ------------------------------------------ */
void putpixel(int x, int y, int color);
int  getpixel(int x, int y);
void moveto(int x, int y);
void moverel(int dx, int dy);
void lineto(int x, int y);
void linerel(int dx, int dy);
void line(int x1, int y1, int x2, int y2);
void rectangle(int left, int top, int right, int bottom);
void bar(int left, int top, int right, int bottom);
void bar3d(int left, int top, int right, int bottom, int depth, int topflag);
void circle(int x, int y, int radius);
void arc(int x, int y, int stangle, int endangle, int radius);
void ellipse(int x, int y, int stangle, int endangle, int xradius, int yradius);
void drawpoly(int numpoints, int *polypoints);
void fillpoly(int numpoints, int *polypoints);
void floodfill(int x, int y, int border);
void pieslice(int x, int y, int stangle, int endangle, int radius);
void sector(int x, int y, int stangle, int endangle, int xradius, int yradius);
void getarccoords(struct arccoordstype *arccoords);

/* ---------------- attributes --------------------------------------- */
void setcolor(int color);
void setbkcolor(int color);
int  getcolor(void);
int  getbkcolor(void);
int  getmaxcolor(void);
void setfillstyle(int pattern, int color);
void setfillpattern(const char *upattern, int color);
void getfillpattern(char *upattern);
void getfillsettings(struct fillsettingstype *fillinfo);
void setlinestyle(int linestyle, unsigned upattern, int thickness);
void getlinesettings(struct linesettingstype *lineinfo);
void setwritemode(int mode);

/* ---------------- text (no bitmap font in the shim) ---------------- */
void settextstyle(int font, int direction, int charsize);
void settextjustify(int horiz, int vert);
void gettextsettings(struct textsettingstype *texttypeinfo);
void setpalette(int colornum, int color);
void getpalette(struct palettetype *palette);
int  getpalettesize(void);
void outtext(const char *textstring);
void outtextxy(int x, int y, const char *textstring);
int  textheight(const char *textstring);
int  textwidth(const char *textstring);

/* ---------------- geometry / raster ops ---------------------------- */
int  getmaxx(void);
int  getmaxy(void);
void getaspectratio(int *xasp, int *yasp);
void setaspectratio(int xasp, int yasp);
unsigned imagesize(int left, int top, int right, int bottom);
void getimage(int left, int top, int right, int bottom, void *bitmap);
void putimage(int left, int top, void *bitmap, int op);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_GRAPHICS_H */
