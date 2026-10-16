/*-----------------------------------------------------------------------*/
/*  cwgrphc.h - Display-layer API port of ThaiWrite CWGRPHC.H            */
/*                                                                       */
/*  Original : SRC/COMMON/CWGRPHC.H (function-pointer dispatch table     */
/*             over CGA/Hercules/EGA/VGA bitmap screens)                 */
/*  Port     : cell-based ncurses backend (see README.md)                */
/*                                                                       */
/*  The call-site surface is kept identical to the DOS original:         */
//  same macro names, same argument order, same font_attr bit values,   */
/*  so SCUW/COMMON callers port over unchanged.                         */
/*                                                                       */
/*  Divergences from the original header:                                */
/*   - clsgraph/clsgraph_ptr arguments are (x1, y1, x2, y2).             */
/*     The original typedef names them (x1, x2, y1, y2) but every        */
/*     implementation and caller (SCRN.C cls(), EDISP.ASM) really uses   */
/*     (x1, y1, x2, y2); the port uses the real order.                   */
/*   - no DEFINE_VAR/EXTERN trick: globals are plain `extern` here and   */
/*     defined once in cwgrphc.c.                                        */
/*-----------------------------------------------------------------------*/

#ifndef PORT_CWGRPHC_H
#define PORT_CWGRPHC_H

#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Screen geometry.                                                    */
/* The DOS screens were 80 bytes/scanline (640 px = 80 cells of 8 px)  */
/* and 348..464 scanlines (17..23 cell rows of 20 px).  Callers may    */
/* address up to x = 89 (dispstrhgc limit), VGA wind goes to row 22.   */
/* ------------------------------------------------------------------ */
#define PORT_SCREEN_COLS 90
#define PORT_SCREEN_ROWS 24

/* One screen cell = one 20-byte record, same size as one DOS font     */
/* glyph.  getwind/putwind/savepic/retpic buffers keep the exact byte  */
/* counts the DOS code computed (rows*20 scanlines x cols bytes).      */
#define PORT_CELL_SIZE   20
#define PORT_SCREEN_BYTES (PORT_SCREEN_ROWS * PORT_SCREEN_COLS * PORT_CELL_SIZE)

/* port_cell: state of one 8x20 screen cell.                           */
/*   base  : TIS-620 middle-level char (0 = blank)                     */
/*   top   : topest-level char overlay   (0xE7-0xEC)                   */
/*   upper : above-middle char overlay   (0xD1, 0xD4-0xD7, 0xED-0xEE)  */
/*   below : below-level char overlay    (0xD8-0xDA)                   */
/*   attr  : font_attr bits of the base char                           */
/*   dot[] : coarse 8x20 pixel approximation used by plot():           */
/*           dot[0]=rows 0-6, dot[1]=rows 7-13, dot[2]=rows 14-19,     */
/*           bit N = column N (bit 0 = leftmost).                      */
/*   over  : block-glyph override (index into the quadrant table in    */
/*           cwgrphc.c).  Used where the DOS build blitted a bitmap    */
/*           straight into video memory, e.g. prakeaw's crest, which   */
/*           has no TIS-620 code to carry it.  0 = none.               */
typedef struct port_cell {
    unsigned char base;
    unsigned char top;
    unsigned char upper;
    unsigned char below;
    unsigned char attr;
    unsigned char dot[3];
    unsigned char over;                          /* block glyph index   */
    unsigned char reserved[PORT_CELL_SIZE - 9]; /* zeroed */
} port_cell;

/* ------------------------------------------------------------------ */
/* Globals (were EXTERN/DEFINE_VAR in the original).                   */
/* ------------------------------------------------------------------ */
extern int           scrmode;         /* graphics_hardware value        */
extern unsigned int  graph_seg;       /* legacy, always 0 in the port   */
extern unsigned char *screen_buffptr; /* savepic/retpic full-screen buf */
extern unsigned char port_chardot[PORT_CELL_SIZE]; /* rd/wrchardot    */

/* graphics_hardware (from grdetect.h) - values kept for source compat */
typedef enum port_graphics_hardware {
    PORT_GFX_DETECT = 0,
    PORT_GFX_CGA = 1, PORT_GFX_MCGA = 2, PORT_GFX_EGA = 3,
    PORT_GFX_EGA64 = 4, PORT_GFX_EGAMONO = 5, PORT_GFX_IBM8514 = 6,
    PORT_GFX_HERCMONO = 7, PORT_GFX_ATT400 = 8, PORT_GFX_VGA = 9,
    PORT_GFX_PC3270 = 10,
    PORT_GFX_MDA = 128,
    PORT_GFX_UNKNOWN_HW = -1
} port_graphics_hardware;
#define DETECT     PORT_GFX_DETECT
#define CGA        PORT_GFX_CGA
#define MCGA       PORT_GFX_MCGA
#define EGA        PORT_GFX_EGA
#define EGA64      PORT_GFX_EGA64
#define EGAMONO    PORT_GFX_EGAMONO
#define IBM8514    PORT_GFX_IBM8514
#define HERCMONO   PORT_GFX_HERCMONO
#define ATT400     PORT_GFX_ATT400
#define VGA        PORT_GFX_VGA
#define PC3270     PORT_GFX_PC3270
#define MDA        PORT_GFX_MDA
#define UNKNOWN_HW PORT_GFX_UNKNOWN_HW

/* font_attr (from cwtype.h) - identical bit values */
typedef enum port_font_attr {
    REVERSEATTR = 0x02,
    ONELINEATTR = 0x01,
    BOLDATTR    = 0x04,
    ENLARGEATTR = 0x40,
    TWOLINEATTR = 0x80,
    ITALICATTR  = 0x20,
    SUPERATTR   = 0x08,
    SUBATTR     = 0x10,
    NORMALATTR  = 0x00
} font_attr;

/* font_code (from cwtype.h) */
typedef enum port_font_code {
    ONELINECODE = 19,               /* ^S */
    BOLDCODE    = 2,                /* ^B */
    ENLARGECODE = 5,                /* ^E */
    TWOLINECODE = 18,               /* ^R */
    ITALICCODE  = 23,               /* ^W */
    SUPERCODE   = 20,               /* ^T */
    SUBCODE     = 22,               /* ^V */
    ITALIC      = 23,
    UNDERLINE   = 19,
    SUBSCRIPT   = 22,
    SUPER       = 20,
    ENLARGE     = 5,
    BOLD        = 2,
    DOUBLEUNDERLINE = 18
} font_code;

/* thai_level (from cwtype.h) */
typedef enum port_thai_level {
    TOPEST = 3, UPPER = 2, MIDDLE = 0, BELOW = 1
} thai_level;

/* ================================================================== */
/* Dispatch table (identical shape to the original).                  */
/* ================================================================== */

typedef void (Prchar_func)(unsigned int c, unsigned int attr,
                           unsigned int x, unsigned int y);
typedef Prchar_func *Prchar_ptr;

extern Prchar_ptr  prchar_ptr;
extern Prchar_func hprchar, eprchar, aprchar; /* provided as aliases of  */
                                              /* the ncurses backend     */
typedef void (Prblank_func)(unsigned int x, unsigned int y);
typedef Prblank_func *Prblank_ptr;

extern Prblank_func hprblank, eprblank, aprblank;
extern Prblank_ptr  prblank_ptr;

typedef void (Setcurpos_func)(unsigned int x, unsigned int y, int thaimode);
typedef Setcurpos_func *Setcurpos_ptr;

extern Setcurpos_func hsetcurpos, esetcurpos, asetcurpos;
extern Setcurpos_ptr  setcurpos_ptr;

typedef void (Plot_func)(unsigned int x, unsigned int y);
typedef Plot_func *Plot_ptr;

extern Plot_func hplot, eplot, aplot;
extern Plot_ptr  plot_ptr;

typedef void (Voidvoid_func)(void);
typedef Voidvoid_func *Voidvoid_ptr;

extern Voidvoid_func hsetgraph, esetgraph, asetgraph,
                     hsettext, esettext, asettext;
extern Voidvoid_func hsavepic, esavepic, asavepic,
                     hretpic, eretpic, aretpic;
extern Voidvoid_func hclsall, eclsall, aclsall,
                     hprakeaw, eprakeaw, aprakeaw;
extern Voidvoid_ptr  setgraph_ptr, settext_ptr, savepic_ptr, retpic_ptr;
extern Voidvoid_ptr  clsall_ptr, prakeaw_ptr;

typedef void (Clsgraph_func)(unsigned int x1, unsigned int y1,
                             unsigned int x2, unsigned int y2);
typedef Clsgraph_func *Clsgraph_ptr;

extern Clsgraph_func hclsgraph, eclsgraph, aclsgraph;
extern Clsgraph_ptr  clsgraph_ptr;

typedef void (Clrline_func)(unsigned int x1, unsigned int y1,
                            unsigned int x2);
typedef Clrline_func *Clrline_ptr;

extern Clrline_func hclrline, eclrline, aclrline;
extern Clrline_ptr  clrline_ptr;

typedef void (RWchardot_func)(unsigned int x, unsigned int y);
typedef RWchardot_func *RWchardot_ptr;

extern RWchardot_func hrdchardot, erdchardot, ardchardot,
                      hwrchardot, ewrchardot, awrchardot;
extern RWchardot_ptr  rdchardot_ptr, wrchardot_ptr;

typedef void (PGwind_func)(unsigned int x1, unsigned int y1,
                           unsigned int linecount, unsigned int bytecount,
                           char *buffer);
typedef PGwind_func *PGwind_ptr;

extern PGwind_func hputwind, eputwind, aputwind, hgetwind, egetwind, agetwind;
extern PGwind_ptr  putwind_ptr, getwind_ptr;

/* ================================================================== */
/* Call-site macros - identical names to the original header.         */
/* ================================================================== */

#define prchar(c, attr, x, y)       (*prchar_ptr)(c, attr, x, y)
#define prblank(x, y)               (*prblank_ptr)(x, y)
#define setcurpos(x, y, thaimode)   (*setcurpos_ptr)(x, y, thaimode)
#define plot(x, y)                  (*plot_ptr)(x, y)
#define settext()                   (*settext_ptr)()
#define savepic()                   (*savepic_ptr)()
#define retpic()                    (*retpic_ptr)()
#define clsall()                    (*clsall_ptr)()
#define clsgraph(x1, y1, x2, y2)    (*clsgraph_ptr)(x1, y1, x2, y2)
#define clrline(x1, y1, x2)         (*clrline_ptr)(x1, y1, x2)
#define prakeaw()                   (*prakeaw_ptr)()
#define rdchardot(x, y)             (*rdchardot_ptr)(x, y)
#define wrchardot(x, y)             (*wrchardot_ptr)(x, y)
/* putwind/getwind are called through the pointers directly, as in the
   original (CSCRN.C savescrn/resscrn style). */

/* ================================================================== */
/* Port additions.                                                    */
/* ================================================================== */

/* Initialize ncurses and bind the dispatch table (port of setgraph's
   backend-selection switch: every DOS adapter maps to the ncurses
   backend).  If scrmode == DETECT it is set to VGA.                  */
void setgraph(void);

/* True while the ncurses backend is active (after setgraph, before
   settext).                                                          */
int  port_graph_active(void);

/* Shadow-screen access.  Returns NULL when out of bounds.            */
port_cell *port_cell_at(unsigned int x, unsigned int y);

/* Draw the Chulalongkorn crest (prakeaw's 32x20 bitmap) into the
   shadow screen at cell (x, y), cols x rows cells.  One cell shows
   2x4 Braille dots and one dot is one bitmap pixel, so the full
   32x20 crest is 16x5 cells at 1:1 (no downsampling).  Used by the
   splash screen; the title bar no longer carries the crest.          */
void port_draw_crest(unsigned int x, unsigned int y,
                     unsigned int cols, unsigned int rows, font_attr attr);

/* Push changed cells of the shadow screen to the terminal and place
   the cursor.  Called automatically by setcurpos (mirrors the DOS
   flow where the screen is up to date while waiting for a key); a
   main loop may call it at any other time.                           */
void port_flush(void);

/* Blocking key read (bridges ebioskey(0) until the input ticket).    */
int  port_getch(void);

/* settext implementation - bound to settext_ptr by setgraph().       */
void port_settext(void);

/* whatlevel - port of the HDISP.ASM leveltable lookup.               */
int  whatlevel(unsigned char c);

#endif /* PORT_CWGRPHC_H */
