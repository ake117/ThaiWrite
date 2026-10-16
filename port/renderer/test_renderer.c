/*-----------------------------------------------------------------------*/
/*  test_renderer.c - smoke test for the ncurses display backend         */
/*                                                                       */
/*  Exercises every entry point of the ported CWGRPHC.H surface:         */
/*  prchar/prblank (all Thai levels), setcurpos, plot, savepic/retpic,   */
/*  clsall/clsgraph/clrline, prakeaw, rd/wrchardot, getwind/putwind,     */
/*  dispstrhgc/framebox/blockmsg.                                        */
/*                                                                       */
/*  Headless check: run with argument "headless" to run the same         */
/*  assertions without a terminal (shadow-screen only, no rendering).    */
/*-----------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "cwgrphc.h"
#include "cscrn.h"

static int checks = 0;
#define CHECK(cond, msg) do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); } \
        checks++; \
    } while (0)

static void run_assertions(void)
{
    port_cell *cell;

    /* --- prchar middle level + attr --------------------------------- */
    prchar('A', BOLDATTR | ONELINEATTR, 5, 2);
    cell = port_cell_at(5, 2);
    CHECK(cell && cell->base == 'A', "prchar stores base char");
    CHECK(cell->attr == (BOLDATTR | ONELINEATTR), "prchar stores attr");

    /* --- prchar Thai overlays --------------------------------------- */
    /* 0xE8 = MAI THO (topest)                                        */
    prchar(0xE8, 0, 5, 2);
    CHECK(cell->top == 0xE8, "topest vowel lands in cell->top");
    /* writing the same vowel again = XOR clear (DOS blit semantics)  */
    prchar(0xE8, 0, 5, 2);
    CHECK(cell->top == 0, "topest vowel XOR-clears");

    /* 0xD1 = MAI EK (upper)                                          */
    prchar(0xD1, 0, 5, 2);
    CHECK(cell->upper == 0xD1, "upper vowel lands in cell->upper");

    /* 0xD8 = MAI HAN-AKAT variant (below)                            */
    prchar(0xD8, 0, 5, 2);
    CHECK(cell->below == 0xD8, "below vowel lands in cell->below");

    /* middle rewrite clears overlays (DOS mov semantics)             */
    prchar('B', 0, 5, 2);
    CHECK(cell->base == 'B' && cell->upper == 0 && cell->below == 0,
          "middle char rewrites whole cell");

    /* --- prblank ----------------------------------------------------- */
    prblank(5, 2);
    CHECK(port_cell_at(5, 2)->base == 0, "prblank clears cell");

    /* --- whatlevel (HDISP.ASM leveltable) --------------------------- */
    CHECK(whatlevel('A') == MIDDLE, "ASCII is middle level");
    CHECK(whatlevel(0xD0) == MIDDLE, "0xD0 middle");
    CHECK(whatlevel(0xD1) == UPPER, "0xD1 upper");
    CHECK(whatlevel(0xD7) == UPPER, "0xD7 upper");
    CHECK(whatlevel(0xD8) == BELOW, "0xD8 below");
    CHECK(whatlevel(0xDA) == BELOW, "0xDA below");
    CHECK(whatlevel(0xE7) == TOPEST, "0xE7 topest");
    CHECK(whatlevel(0xEC) == TOPEST, "0xEC topest");
    CHECK(whatlevel(0xED) == TOPEST, "0xED topest (leveltable)");
    CHECK(whatlevel(0xEE) == TOPEST, "0xEE topest (leveltable)");

    /* --- clsgraph / clrline ------------------------------------------ */
    prchar('X', 0, 10, 10);
    prchar('X', 0, 11, 11);
    clsgraph(10, 10, 11, 11);
    CHECK(port_cell_at(10, 10)->base == 0 &&
          port_cell_at(11, 11)->base == 0, "clsgraph clears region");

    prchar('Y', 0, 3, 7);
    clrline(3, 7, 10);
    CHECK(port_cell_at(3, 7)->base == 0, "clrline clears row range");

    /* --- rd/wrchardot round-trip ------------------------------------- */
    prchar('Z', ITALICATTR, 20, 4);
    rdchardot(20, 4);
    CHECK(port_chardot[0] == 'Z', "rdchardot captures base");
    CHECK(port_chardot[4] == ITALICATTR, "rdchardot captures attr");
    prblank(20, 4);
    wrchardot(20, 4);
    CHECK(port_cell_at(20, 4)->base == 'Z' &&
          port_cell_at(20, 4)->attr == ITALICATTR,
          "wrchardot restores cell");

    /* --- savepic / retpic -------------------------------------------- */
    prchar('Q', REVERSEATTR, 30, 9);
    savepic();
    CHECK(screen_buffptr != NULL, "savepic buffer allocated");
    CHECK(((unsigned char *)screen_buffptr)
              [((9 * PORT_SCREEN_COLS) + 30) * PORT_CELL_SIZE] == 'Q',
          "savepic serialises cell");
    prblank(30, 9);
    retpic();
    CHECK(port_cell_at(30, 9)->base == 'Q' &&
          port_cell_at(30, 9)->attr == REVERSEATTR,
          "retpic restores cell");

    /* --- getwind / putwind (port byte layout) ------------------------- */
    /* Layout: record r (0..linecount-1) at buf[r*bytecount] holds the  */
    /* cell at (x1, y1 + r/20); min(bytecount,20) bytes of cell state.  */
    {
        char buf[40 * 20];
        memset(buf, 0, sizeof(buf));
        prchar('W', BOLDATTR, 40, 6);
        prchar('S', 0, 40, 7);
        (*getwind_ptr)(40, 6, 40, 20, buf);
        CHECK((unsigned char)buf[0] == 'W', "getwind row0 base");
        CHECK((unsigned char)buf[4] == BOLDATTR, "getwind carries attr");
        CHECK((unsigned char)buf[19 * 20] == 'W',
              "getwind same cell for records 0-19");
        CHECK((unsigned char)buf[20 * 20] == 'S', "getwind row1 base");
        prblank(40, 6);
        prblank(40, 7);
        (*putwind_ptr)(40, 6, 40, 20, buf);
        CHECK(port_cell_at(40, 6)->base == 'W', "putwind restores W");
        CHECK(port_cell_at(40, 6)->attr == BOLDATTR,
              "putwind restores attr");
        CHECK(port_cell_at(40, 7)->base == 'S', "putwind restores S");
    }

    /* --- plot --------------------------------------------------------- */
    plot(70, 25);                 /* cell (8, 1), pixel row 25 % 20     */
    CHECK(port_cell_at(8, 1)->dot[(25 % 20) / 7] != 0,
          "plot sets dot bit");

    /* --- prakeaw ------------------------------------------------------ */
    /*  พระเกี้ยว = bitmap blit 32x20 px ที่ byte column 1 และ 3
     *  (HDISP.ASM _hprakeaw) => row 0 คอลัมน์ 1-4 ย่อเป็นอักขระ Braille
     *  เก็บดอทมาสก์ใน port_cell.over (แสดงผลเป็น U+2800 + mask)        */
    prakeaw();
    CHECK(port_cell_at(1, 0)->over == 0, "prakeaw leaves the title bar alone");
    port_draw_crest(1, 7, 16, 5);
    CHECK(port_cell_at(1, 7)->over == 0x00, "crest cell (0,0) is empty");
    CHECK(port_cell_at(2, 7)->over == 0x00, "crest cell (1,0) is empty");
    CHECK(port_cell_at(1 + 8, 7 + 2)->over == 0xD6, "crest cell (8,2) = 0xD6");
    CHECK(port_cell_at(1 + 15, 7 + 4)->over == 0x05, "crest cell (15,4) = 0x05");
    clsgraph(0, 0, 5, 0);
    prakeaw();

    /* --- dispstrhgc / togglefont --------------------------------------- */
    {
        font_attr a = NORMALATTR;
        char s[] = { 'H', 'i', 0 };
        dispstrhgc(s, 2, 12, NORMALATTR);
        CHECK(port_cell_at(2, 12)->base == 'H', "dispstrhgc H at x=2");
        CHECK(port_cell_at(3, 12)->base == 'i', "dispstrhgc i at x=3");
        togglefont(&a, BOLDCODE);
        CHECK(a == BOLDATTR, "togglefont ^B");
        togglefont(&a, ITALICCODE);
        CHECK(a == (BOLDATTR | ITALICATTR), "togglefont ^W accumulates");
        togglefont(&a, ENLARGECODE);
        CHECK(a == (BOLDATTR | ITALICATTR | ENLARGEATTR),
              "togglefont ^E sets ENLARGEATTR (not ENLARGECODE)");
    }

    /* --- clsall -------------------------------------------------------- */
    prchar('K', 0, 60, 15);
    clsall();
    CHECK(port_cell_at(60, 15)->base == 0, "clsall clears screen");

    /* --- blockmsg / framebox bounds ------------------------------------- */
    blockmsg(10);
    CHECK(port_cell_at(13, 9)->attr & REVERSEATTR,
          "framebox paints reverse border");
}

int main(int argc, char **argv)
{
    int headless = (argc > 1 && strcmp(argv[1], "headless") == 0);

    scrmode = DETECT;
    setgraph();

    if (headless)
    {
        /* render nothing; shadow-screen assertions still hold         */
        run_assertions();
        printf("OK: %d checks passed (headless)\n", checks);
        return 0;
    }

    run_assertions();
    port_flush();

    port_getch();

    (*settext_ptr)();
    printf("OK: %d checks passed\n", checks);
    return 0;
}
