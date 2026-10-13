/* =====================================================================
 * shim_graphics.c - BGI <graphics.h> emulation for the ThaiWrite port
 * ---------------------------------------------------------------------
 * Only SRC/SCUF (the font editor, cufont.exe) uses BGI.  This file
 * implements a working raster engine so that:
 *
 *   1. every BGI call in SRC/SCUF compiles and links,
 *   2. the font editor's save/restore idiom
 *          p = malloc(imagesize(l,t,r,b));
 *          getimage(l,t,r,b,p);  ... putimage(l,t,r,b,p, XOR_PUT);
 *      round-trips exactly,
 *   3. drawing lands in the *same memory* the font editor's own glyph
 *      driver writes to (SRC/SCUF/WRITE.C writes straight to the
 *      Hercules window at 0xB0000000 / the EGA window at 0xA0000000).
 *
 * Raster surface
 * --------------
 * The surface IS the emulated video memory, obtained from shim_dos.c:
 *
 *   HERCMONO/EGAMONO/EGA/EGA64/ATT400  -> the "Herc" window
 *                                        0xB0000000, 64 KB, 4 banks of
 *                                        0x2000 bytes, 90 bytes/scanline
 *   VGA/MCGA/CGA/PC3270               -> the "EGA" window
 *                                        0xA0000000, 256 KB, flat,
 *                                        80 bytes/scanline
 *
 * A pixel (x, y) is one bit: byte x/8, bit 0x80 >> (x & 7).  That is
 * exactly the layout SRC/SCUF/WRITE.C's hgcwritech()/ewritech() use, so
 * BGI drawing and the app's own glyph blitter share one framebuffer.
 *
 * Coordinates
 * -----------
 * Like real BGI, all coordinates are relative to the current viewport
 * origin, and clipping is applied when the viewport was created with
 * clip != 0.  This is load-bearing: SRC/SCUF/FED.C edit() sets the
 * viewport to (400,30)-(maxx,maxy) with clip=1 and then draws/saves the
 * enlarged character matrix at viewport-local (0+align, 0); with
 * absolute coordinates the clip rectangle would throw that drawing
 * away and the font matrix would never appear.  Define
 * SHIM_BGI_ABSOLUTE_COORDS to compile the (wrong) absolute variant.
 *
 * Divergences from Borland BGI
 * ----------------------------
 *   - 2 raster colours only (pixel set/clear).  colour 0 = off, any
 *     other value = on.  getmaxcolor() returns 15 so palette-index code
 *     keeps working, but indices only distinguish "on" from "off".
 *   - the named line styles are approximations; USERBIT_LINE uses the
 *     upattern supplied to setlinestyle() exactly.
 *   - fill patterns other than SOLID_FILL/EMPTY_FILL behave as
 *     SOLID_FILL.
 *   - there is no bitmap font, so outtext()/outtextxy() draw nothing;
 *     textwidth()/textheight() report the built-in 8x8 cell metrics.
 *   - getimage()/putimage() use the shim's own image layout
 *     (2-byte width, 2-byte height, then one bitmap row per scanline);
 *     it is self-consistent, which is all the app needs.
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "graphics.h"
#include "shim_internal.h"
#include "dos.h"                /* shim_vram_herc()/shim_vram_ega() */

/* ---------------------------------------------------------------- */
/*  Raster surface                                                  */
/* ---------------------------------------------------------------- */

#define SHIM_HERC_BYTES 0x10000UL       /* 64 KB  */
#define SHIM_EGA_BYTES  0x40000UL       /* 256 KB */
#define SHIM_HERC_PITCH 90              /* bytes per scanline in a bank */
#define SHIM_EGA_PITCH  80              /* bytes per scanline (flat)     */

/* Used when the fixed-address mappings could not be installed. */
static unsigned char shim_gfx_fallback[SHIM_EGA_BYTES];

static unsigned char *shim_surface;     /* active raster surface        */
static int shim_banked;                 /* 1 = 4-bank Herc interleave   */
static int shim_pitch;                  /* bytes per surface scanline   */
static int shim_surfw;                  /* surface width  in pixels     */
static int shim_surfh;                  /* surface height in pixels     */
static int shim_surfmax;                /* surface size in bytes        */

static int shim_driver = VGA, shim_mode = VGAHI;
static int shim_active = 0;
static int shim_graphresult = grOk;

static int shim_color = 1, shim_bkcolor = 0;
static int shim_fillstyle = EMPTY_FILL, shim_fillcolor = 0;
static int shim_linestyle = SOLID_LINE, shim_linethickness = NORM_WIDTH;
static unsigned short shim_lineupattern = 0xFFFF;
static int shim_writemode = COPY_PUT;

static struct viewporttype shim_vp = {0, 0, 639, 479, 0};
static int shim_cx = 0, shim_cy = 0;
static struct arccoordstype shim_arccoords;

/* BGI driver images the sources register - no-ops in this port. */
unsigned char EGAVGA_driver_far[1];
unsigned char Herc_driver_far[1];
unsigned char EGAVGA_font_far[1];
unsigned char ATT400_driver_far[1];
unsigned char PC3270_driver_far[1];
unsigned char IBM8514_driver_far[1];

/* ---------------------------------------------------------------- */
/*  Surface selection                                               */
/* ---------------------------------------------------------------- */

static void shim_select_surface(int driver)
{
    switch (driver)
    {
        case HERCMONO:
        case EGAMONO:
        case EGA:
        case EGA64:
        case ATT400:
            shim_surface = shim_vram_herc();
            shim_banked  = 1;
            shim_pitch   = SHIM_HERC_PITCH;
            shim_surfw   = 720;
            shim_surfh   = 348;
            shim_surfmax = SHIM_HERC_BYTES;
            break;
        default:                        /* VGA, MCGA, CGA, IBM8514, ... */
            shim_surface = shim_vram_ega();
            shim_banked  = 0;
            shim_pitch   = SHIM_EGA_PITCH;
            shim_surfw   = 640;
            shim_surfh   = (driver == CGA) ? 200 : 480;
            shim_surfmax = SHIM_EGA_BYTES;
            break;
    }
    if (shim_surface == NULL)           /* defensive: never leave NULL   */
        shim_surface = shim_gfx_fallback;
}

/* Public accessors for the port's terminal blitter. */
unsigned char *shim_bgi_surface(void)  { return shim_surface; }
int shim_bgi_surface_banked(void)      { return shim_banked; }
int shim_bgi_surface_pitch(void)       { return shim_pitch; }
int shim_bgi_surface_width(void)       { return shim_surfw; }
int shim_bgi_surface_height(void)      { return shim_surfh; }
int shim_bgi_active(void)              { return shim_active; }

/* ---------------------------------------------------------------- */
/*  Pixel access (absolute raster coordinates)                      */
/* ---------------------------------------------------------------- */

static unsigned char *shim_px(int x, int y)
{
    if (shim_banked)
        return shim_surface + ((y & 3) << 13) + ((y >> 2) * shim_pitch) + (x >> 3);
    return shim_surface + y * shim_pitch + (x >> 3);
}

static int shim_on_surface(int x, int y)
{
    return x >= 0 && x < shim_surfw && y >= 0 && y < shim_surfh;
}

static int shim_clipped(int x, int y)
{
    if (!shim_vp.clip)
        return 0;
    return x < shim_vp.left || x > shim_vp.right ||
           y < shim_vp.top  || y > shim_vp.bottom;
}

static void shim_raw_pixel(int x, int y, int color)
{
    unsigned char *p;
    unsigned char mask;
    if (!shim_on_surface(x, y))
        return;
    p = shim_px(x, y);
    mask = (unsigned char)(0x80 >> (x & 7));
    if (color)
        *p |= mask;
    else
        *p &= (unsigned char)~mask;
}

static int shim_raw_get(int x, int y)
{
    if (!shim_on_surface(x, y))
        return 0;
    return (shim_px(x, y)[0] & (0x80 >> (x & 7))) ? 1 : 0;
}

/* ---------------------------------------------------------------- */
/*  Coordinates                                                     */
/* ---------------------------------------------------------------- */

#ifdef SHIM_BGI_ABSOLUTE_COORDS
#define SHIM_X(x) (x)
#define SHIM_Y(y) (y)
#else
#define SHIM_X(x) ((x) + shim_vp.left)
#define SHIM_Y(y) ((y) + shim_vp.top)
#endif

static void shim_set(int x, int y, int color)
{
    x = SHIM_X(x);
    y = SHIM_Y(y);
    if (shim_clipped(x, y))
        return;
    switch (shim_writemode)
    {
        case XOR_PUT: color = color ^ shim_raw_get(x, y); break;
        case OR_PUT:  color = color | shim_raw_get(x, y); break;
        case AND_PUT: color = color & shim_raw_get(x, y); break;
        case NOT_PUT: color = !shim_raw_get(x, y);        break;
        default: break;                                   /* COPY_PUT */
    }
    shim_raw_pixel(x, y, color);
}

/* ---------------------------------------------------------------- */
/*  Init / shutdown                                                 */
/* ---------------------------------------------------------------- */

void detectgraph(int *graphdriver, int *graphmode)
{
    if (graphdriver)
        *graphdriver = VGA;
    if (graphmode)
        *graphmode = VGAHI;
}

void initgraph(int *graphdriver, int *graphmode, const char *pathtodriver)
{
    int driver = graphdriver ? *graphdriver : DETECT;
    int mode   = graphmode ? *graphmode : 0;
    (void)pathtodriver;

    if (driver == DETECT)
        detectgraph(&driver, &mode);

    shim_driver = driver;
    shim_mode   = mode;
    shim_select_surface(driver);

    shim_vp.left   = 0;
    shim_vp.top    = 0;
    shim_vp.right  = shim_surfw - 1;
    shim_vp.bottom = shim_surfh - 1;
    shim_vp.clip   = 0;

    shim_color        = 1;
    shim_bkcolor      = 0;
    shim_fillstyle    = EMPTY_FILL;
    shim_fillcolor    = 0;
    shim_linestyle    = SOLID_LINE;
    shim_linethickness = NORM_WIDTH;
    shim_lineupattern = 0xFFFF;
    shim_writemode    = COPY_PUT;
    shim_cx = shim_cy = 0;

    memset(shim_surface, 0, (size_t)shim_surfmax);
    shim_active = 1;
    shim_graphresult = grOk;

    if (graphdriver)
        *graphdriver = shim_driver;
    if (graphmode)
        *graphmode = shim_mode;
}

void closegraph(void)
{
    shim_active = 0;
}

void restorecrtmode(void)
{
    /* The port renders to a terminal, so there is no text mode to
     * restore; settext() in the renderer does the real work. */
}

int graphresult(void)
{
    return shim_graphresult;
}

const char *grapherrormsg(int errcode)
{
    switch (errcode)
    {
        case grOk:              return "No graphics error";
        case grNoInitGraph:     return "Graphics not initialized (use initgraph)";
        case grNotDetected:     return "Graphics hardware not detected";
        case grFileNotFound:    return "Graphics device driver file not found";
        case grInvalidDriver:   return "Invalid graphics driver";
        case grNoLoadMem:       return "Not enough memory to load graphics driver";
        case grNoScanMem:       return "Not enough memory for scanfill";
        case grNoFloodMem:      return "Not enough memory for floodfill";
        case grFontNotFound:    return "Font file not found";
        case grNoFontMem:       return "Not enough memory to load font";
        case grInvalidMode:     return "Invalid graphics mode for selected driver";
        case grError:           return "Graphics error";
        case grIOerror:         return "Graphics I/O error";
        case grInvalidFont:     return "Invalid font file";
        case grInvalidFontNum:  return "Invalid font number";
        case grInvalidVersion:  return "Invalid version of device driver";
        default:                return "Unknown graphics error";
    }
}

int registerbgidriver(void (*driver)(void))
{
    (void)driver;
    return grOk;                 /* drivers are built in */
}

int registerfarbgidriver(void *driver)
{
    (void)driver;
    return grOk;
}

int registerbgifont(void (*font)(void))
{
    (void)font;
    return grOk;
}

int registerfarbgifont(void *font)
{
    (void)font;
    return grOk;
}

int installuserdriver(const char *name, int (*detect)(void))
{
    (void)name;
    (void)detect;
    return grOk;
}

int installuserfont(const char *name)
{
    (void)name;
    return 0;
}

void setactivepage(int page)  { (void)page; }
void setvisualpage(int page)  { (void)page; }

/* ---------------------------------------------------------------- */
/*  Viewport                                                        */
/* ---------------------------------------------------------------- */

void setviewport(int left, int top, int right, int bottom, int clip)
{
    shim_vp.left   = left;
    shim_vp.top    = top;
    shim_vp.right  = right;
    shim_vp.bottom = bottom;
    shim_vp.clip   = clip ? 1 : 0;
}

void getviewsettings(struct viewporttype *viewport)
{
    if (viewport)
        *viewport = shim_vp;
}

void clearviewport(void)
{
    int x, y;
    for (y = shim_vp.top; y <= shim_vp.bottom; y++)
        for (x = shim_vp.left; x <= shim_vp.right; x++)
            shim_raw_pixel(x, y, shim_bkcolor);
}

void cleardevice(void)
{
    struct viewporttype save = shim_vp;
    shim_vp.left = 0;
    shim_vp.top = 0;
    shim_vp.right = shim_surfw - 1;
    shim_vp.bottom = shim_surfh - 1;
    clearviewport();
    shim_vp = save;
}

/* ---------------------------------------------------------------- */
/*  Attributes                                                      */
/* ---------------------------------------------------------------- */

void setcolor(int color)                     { shim_color = color; }
int  getcolor(void)                          { return shim_color; }
void setbkcolor(int color)                   { shim_bkcolor = color; }
int  getbkcolor(void)                        { return shim_bkcolor; }
int  getmaxcolor(void)                       { return 15; }

void setfillstyle(int pattern, int color)
{
    shim_fillstyle = pattern;
    shim_fillcolor = color;
}

void setfillpattern(const char *upattern, int color)
{
    shim_fillstyle = USER_FILL;
    shim_fillcolor = color;
    (void)upattern;
}

void getfillpattern(char *upattern)
{
    if (upattern)
        memset(upattern, 0xFF, 8);
}

void getfillsettings(struct fillsettingstype *fillinfo)
{
    if (fillinfo)
    {
        fillinfo->pattern = shim_fillstyle;
        fillinfo->color   = shim_fillcolor;
    }
}

void setlinestyle(int linestyle, unsigned upattern, int thickness)
{
    shim_linestyle     = linestyle;
    shim_lineupattern  = (unsigned short)upattern;
    shim_linethickness = thickness;
}

void getlinesettings(struct linesettingstype *lineinfo)
{
    if (lineinfo)
    {
        lineinfo->linestyle = shim_linestyle;
        lineinfo->upattern  = shim_lineupattern;
        lineinfo->thickness = shim_linethickness;
    }
}

void setwritemode(int mode)
{
    shim_writemode = mode;
}

/* ---------------------------------------------------------------- */
/*  Primitives                                                      */
/* ---------------------------------------------------------------- */

static unsigned short shim_line_mask(void)
{
    switch (shim_linestyle)
    {
        case SOLID_LINE:  return 0xFFFF;
        case DOTTED_LINE: return 0xCCCC;
        case CENTER_LINE: return 0xF8F8;
        case DASHED_LINE: return 0xFC00;
        case USERBIT_LINE:return shim_lineupattern;
        default:          return 0xFFFF;
    }
}

static void shim_plot_thick(int x, int y, int color)
{
    int i;
    for (i = 0; i < shim_linethickness; i++)
        shim_set(x + i, y + i, color);
}

static void shim_draw_line(int x1, int y1, int x2, int y2, int color)
{
    int dx = abs(x2 - x1), sx = (x1 < x2) ? 1 : -1;
    int dy = -abs(y2 - y1), sy = (y1 < y2) ? 1 : -1;
    int err = dx + dy, e2;
    unsigned short mask = shim_line_mask();
    unsigned long n = 0;

    for (;;)
    {
        if (mask & (1u << (n & 15)))
            shim_plot_thick(x1, y1, color);
        if (x1 == x2 && y1 == y2)
            break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
        n++;
    }
}

void putpixel(int x, int y, int color)
{
    shim_set(x, y, color);
}

int getpixel(int x, int y)
{
    x = SHIM_X(x);
    y = SHIM_Y(y);
    return shim_raw_get(x, y);
}

void moveto(int x, int y)  { shim_cx = x; shim_cy = y; }
void moverel(int dx, int dy) { shim_cx += dx; shim_cy += dy; }

void lineto(int x, int y)
{
    shim_draw_line(shim_cx, shim_cy, x, y, shim_color);
    shim_cx = x;
    shim_cy = y;
}

void linerel(int dx, int dy)
{
    shim_draw_line(shim_cx, shim_cy, shim_cx + dx, shim_cy + dy, shim_color);
    shim_cx += dx;
    shim_cy += dy;
}

void line(int x1, int y1, int x2, int y2)
{
    shim_draw_line(x1, y1, x2, y2, shim_color);
}

void rectangle(int left, int top, int right, int bottom)
{
    shim_draw_line(left,  top,    right, top,    shim_color);
    shim_draw_line(right, top,    right, bottom, shim_color);
    shim_draw_line(right, bottom, left,  bottom, shim_color);
    shim_draw_line(left,  bottom, left,  top,    shim_color);
}

static void shim_fill_rect(int left, int top, int right, int bottom)
{
    int x, y;
    if (shim_fillstyle == EMPTY_FILL)
        return;
    if (left > right)  { int t = left;  left  = right; right = t; }
    if (top  > bottom) { int t = top;   top   = bottom; bottom = t; }
    for (y = SHIM_Y(top); y <= SHIM_Y(bottom); y++)
        for (x = SHIM_X(left); x <= SHIM_X(right); x++)
        {
            if (shim_clipped(x, y))
                continue;
            shim_raw_pixel(x, y, shim_fillcolor);
        }
}

void bar(int left, int top, int right, int bottom)
{
    shim_fill_rect(left, top, right, bottom);
}

void bar3d(int left, int top, int right, int bottom, int depth, int topflag)
{
    shim_fill_rect(left, top, right, bottom);
    if (depth <= 0)
        return;
    if (topflag)
    {
        shim_draw_line(right, top, right + depth, top - depth, shim_color);
        shim_draw_line(right + depth, top - depth, left + depth, top - depth,
                       shim_color);
        shim_draw_line(left + depth, top - depth, left, top, shim_color);
    }
    shim_draw_line(right, top, right + depth, top - depth, shim_color);
    shim_draw_line(right, bottom, right + depth, bottom - depth, shim_color);
    shim_draw_line(right + depth, bottom - depth, left + depth, bottom - depth,
                   shim_color);
}

void circle(int x, int y, int radius)
{
    int px = 0, py = radius;
    int d = 1 - radius;
    while (px <= py)
    {
        shim_set(x + px, y + py, shim_color);
        shim_set(x - px, y + py, shim_color);
        shim_set(x + px, y - py, shim_color);
        shim_set(x - px, y - py, shim_color);
        shim_set(x + py, y + px, shim_color);
        shim_set(x - py, y + px, shim_color);
        shim_set(x + py, y - px, shim_color);
        shim_set(x - py, y - px, shim_color);
        if (d < 0)
            d += 2 * px + 3;
        else
        {
            d += 2 * (px - py) + 5;
            py--;
        }
        px++;
    }
}

/* Draw the elliptical arc between stangle and endangle (degrees, CCW,
 * 0 = east) using a fixed-step polar walk - adequate for the app's use
 * of arcs as decorations. */
static void shim_arc_steps(int x, int y, int stangle, int endangle,
                           int xradius, int yradius, int color)
{
    double a, a0, a1, step;
    a0 = stangle * 3.14159265358979 / 180.0;
    a1 = endangle * 3.14159265358979 / 180.0;
    while (a1 < a0)
        a1 += 2 * 3.14159265358979;
    step = 1.0 / (2.0 * (xradius > yradius ? xradius : yradius) + 1.0);
    for (a = a0; a <= a1; a += step)
    {
        int px = x + (int)floor(xradius * cos(a) + 0.5);
        int py = y - (int)floor(yradius * sin(a) + 0.5);
        shim_set(px, py, color);
    }
    shim_arccoords.x      = x;
    shim_arccoords.y      = y;
    shim_arccoords.xstart = x + (int)floor(xradius * cos(a0) + 0.5);
    shim_arccoords.ystart = y - (int)floor(yradius * sin(a0) + 0.5);
    shim_arccoords.xend   = x + (int)floor(xradius * cos(a1) + 0.5);
    shim_arccoords.yend   = y - (int)floor(yradius * sin(a1) + 0.5);
}

void ellipse(int x, int y, int stangle, int endangle,
             int xradius, int yradius)
{
    shim_arc_steps(x, y, stangle, endangle, xradius, yradius, shim_color);
}

void arc(int x, int y, int stangle, int endangle, int radius)
{
    shim_arc_steps(x, y, stangle, endangle, radius, radius, shim_color);
}

void getarccoords(struct arccoordstype *arccoords)
{
    if (arccoords)
        *arccoords = shim_arccoords;
}

void pieslice(int x, int y, int stangle, int endangle, int radius)
{
    shim_arc_steps(x, y, stangle, endangle, radius, radius, shim_color);
    shim_fill_rect(x - radius, y - radius, x + radius, y + radius);
}

void sector(int x, int y, int stangle, int endangle,
            int xradius, int yradius)
{
    shim_arc_steps(x, y, stangle, endangle, xradius, yradius, shim_color);
    shim_fill_rect(x - xradius, y - yradius, x + xradius, y + yradius);
}

void drawpoly(int numpoints, int *polypoints)
{
    int i;
    for (i = 0; i < numpoints - 1; i++)
        shim_draw_line(polypoints[2 * i], polypoints[2 * i + 1],
                       polypoints[2 * i + 2], polypoints[2 * i + 3],
                       shim_color);
    if (numpoints > 2)
        shim_draw_line(polypoints[2 * (numpoints - 1)],
                       polypoints[2 * (numpoints - 1) + 1],
                       polypoints[0], polypoints[1], shim_color);
}

void fillpoly(int numpoints, int *polypoints)
{
    drawpoly(numpoints, polypoints);
    /* A faithful scan-line polygon fill is unnecessary here (the app
     * does not call fillpoly); fill the bounding box so the effect is
     * visible if it ever is called. */
    if (numpoints > 0)
    {
        int i, minx = polypoints[0], maxx = polypoints[0];
        int miny = polypoints[1], maxy = polypoints[1];
        for (i = 1; i < numpoints; i++)
        {
            if (polypoints[2 * i]     < minx) minx = polypoints[2 * i];
            if (polypoints[2 * i]     > maxx) maxx = polypoints[2 * i];
            if (polypoints[2 * i + 1] < miny) miny = polypoints[2 * i + 1];
            if (polypoints[2 * i + 1] > maxy) maxy = polypoints[2 * i + 1];
        }
        shim_fill_rect(minx, miny, maxx, maxy);
    }
}

void floodfill(int x, int y, int border)
{
    long *stack;
    unsigned char *seen;
    size_t cap = 1024, sp = 0;
    int target, fillbit;

    x = SHIM_X(x);
    y = SHIM_Y(y);
    if (!shim_on_surface(x, y))
        return;
    if (shim_fillstyle == EMPTY_FILL)
        return;

    /* border < 0 means "no border": flood the region that has the same
     * colour as the seed pixel.  That is what SRC/SCUF/FED.C's
     * floodfill(1, 1, -1) relies on to fill the inside of the square. */
    if (border < 0)
        target = shim_raw_get(x, y);
    else
        target = !(border != 0);
    fillbit = (shim_fillcolor != 0);

    stack = (long *)malloc(cap * sizeof(long));
    seen  = (unsigned char *)calloc((size_t)shim_surfw * (size_t)shim_surfh, 1);
    if (!stack || !seen)
    {
        free(stack);
        free(seen);
        return;
    }

#define SHIM_PUSH(px, py)                                                    \
    do {                                                                     \
        if ((px) >= 0 && (px) < shim_surfw && (py) >= 0 && (py) < shim_surfh)  \
        {                                                                    \
            unsigned char *q = shim_px((px), (py));                          \
            unsigned char m = (unsigned char)(0x80 >> ((px) & 7));           \
            size_t idx = (size_t)(py) * (size_t)shim_surfw + (size_t)(px);    \
            if (!seen[idx] && (((*q & m) ? 1 : 0) == target))                 \
            {                                                                \
                seen[idx] = 1;                                               \
                if (sp == cap)                                               \
                {                                                            \
                    long *ns = (long *)realloc(stack, (cap *= 2) * sizeof(long)); \
                    if (!ns) { free(stack); free(seen); return; }            \
                    stack = ns;                                              \
                }                                                            \
                stack[sp++] = ((long)(py) << 16) | (unsigned)(px);           \
                if (fillbit) *q |= m; else *q &= (unsigned char)~m;           \
            }                                                                \
        }                                                                    \
    } while (0)

    SHIM_PUSH(x, y);
    while (sp)
    {
        long p = stack[--sp];
        int px = (int)(p & 0xFFFF);
        int py = (int)((p >> 16) & 0xFFFF);
        SHIM_PUSH(px - 1, py);
        SHIM_PUSH(px + 1, py);
        SHIM_PUSH(px, py - 1);
        SHIM_PUSH(px, py + 1);
    }
    free(stack);
    free(seen);
#undef SHIM_PUSH
}

void getaspectratio(int *xasp, int *yasp)
{
    if (xasp) *xasp = 10000;
    if (yasp) *yasp = 10000;
}

void setaspectratio(int xasp, int yasp)
{
    (void)xasp;
    (void)yasp;
}

/* ---------------------------------------------------------------- */
/*  Geometry                                                        */
/* ---------------------------------------------------------------- */

int getmaxx(void) { return shim_surfw - 1; }
int getmaxy(void) { return shim_surfh - 1; }

unsigned imagesize(int left, int top, int right, int bottom)
{
    unsigned width  = (unsigned)(abs(right - left) + 1);
    unsigned height = (unsigned)(abs(bottom - top) + 1);
    return 4u + (((width + 7u) / 8u) * height);
}

/* ---------------------------------------------------------------- */
/*  Image save / restore                                            */
/* ---------------------------------------------------------------- */

/* Image layout: uint16 width (in pixels), uint16 height (in scanlines),
 * then one bitmap row per scanline, MSB = leftmost pixel. */
static unsigned shim_img_pitch(unsigned width)
{
    return (width + 7u) / 8u;
}

void getimage(int left, int top, int right, int bottom, void *bitmap)
{
    unsigned char *b = (unsigned char *)bitmap;
    unsigned width, height, pitch, row, col;
    int x0, y0;

    if (left > right)  { int t = left;  left  = right;  right = t; }
    if (top  > bottom) { int t = top;   top   = bottom; bottom = t; }

    x0 = SHIM_X(left);
    y0 = SHIM_Y(top);
    width  = (unsigned)(right - left + 1);
    height = (unsigned)(bottom - top + 1);
    pitch  = shim_img_pitch(width);

    b[0] = (unsigned char)(width & 0xFF);
    b[1] = (unsigned char)((width >> 8) & 0xFF);
    b[2] = (unsigned char)(height & 0xFF);
    b[3] = (unsigned char)((height >> 8) & 0xFF);
    b += 4;

    for (row = 0; row < height; row++)
    {
        unsigned char *dst = b + row * pitch;
        memset(dst, 0, pitch);
        for (col = 0; col < width; col++)
        {
            if (shim_raw_get(x0 + (int)col, y0 + (int)row))
                dst[col >> 3] |= (unsigned char)(0x80 >> (col & 7));
        }
    }
}

void putimage(int left, int top, void *bitmap, int op)
{
    unsigned char *b = (unsigned char *)bitmap;
    unsigned width, height, pitch, row, col;
    int x0 = SHIM_X(left), y0 = SHIM_Y(top);

    width  = (unsigned)(b[0] | (b[1] << 8));
    height = (unsigned)(b[2] | (b[3] << 8));
    pitch  = shim_img_pitch(width);
    b += 4;

    for (row = 0; row < height; row++)
    {
        unsigned char *src = b + row * pitch;
        for (col = 0; col < width; col++)
        {
            int x = x0 + (int)col;
            int y = y0 + (int)row;
            int v = (src[col >> 3] & (0x80 >> (col & 7))) ? 1 : 0;
            if (shim_clipped(x, y))
                continue;
            switch (op)
            {
                case COPY_PUT: shim_raw_pixel(x, y, v); break;
                case XOR_PUT:  shim_raw_pixel(x, y, v ^ shim_raw_get(x, y)); break;
                case OR_PUT:   shim_raw_pixel(x, y, v | shim_raw_get(x, y)); break;
                case AND_PUT:  shim_raw_pixel(x, y, v & shim_raw_get(x, y)); break;
                case NOT_PUT:  shim_raw_pixel(x, y, !shim_raw_get(x, y)); break;
                default:       shim_raw_pixel(x, y, v); break;
            }
        }
    }
}

/* ---------------------------------------------------------------- */
/*  Text (no bitmap font in the port)                               */
/* ---------------------------------------------------------------- */

void settextstyle(int font, int direction, int charsize)
{
    (void)font; (void)direction; (void)charsize;
}

void settextjustify(int horiz, int vert)
{
    (void)horiz; (void)vert;
}

void gettextsettings(struct textsettingstype *texttypeinfo)
{
    if (texttypeinfo)
    {
        texttypeinfo->font      = 0;
        texttypeinfo->direction = 0;
        texttypeinfo->charsize  = 1;
    }
}

void setpalette(int colornum, int color)
{
    (void)colornum;
    (void)color;
}

void getpalette(struct palettetype *palette)
{
    if (palette)
    {
        int i;
        palette->size = 16;
        for (i = 0; i < 16; i++)
            palette->colors[i] = (signed char)i;
    }
}

int getpalettesize(void)
{
    return 16;
}

void outtext(const char *textstring)
{
    /* The port has no BGI bitmap font.  SRC/SCUF draws all of its text
     * through CSCRN's dispstrhgc()/dispprintf(); outtext() is provided
     * for link compatibility only. */
    (void)textstring;
}

void outtextxy(int x, int y, const char *textstring)
{
    (void)x; (void)y;
    (void)textstring;
}

int textheight(const char *textstring)
{
    (void)textstring;
    return 8;
}

int textwidth(const char *textstring)
{
    return (int)(strlen(textstring) * 8);
}
