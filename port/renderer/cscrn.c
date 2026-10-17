/*-----------------------------------------------------------------------*/
/*  cscrn.c - string/box display helpers                                 */
/*                                                                       */
/*  Port of COMMON/CSCRN.C on top of the ncurses cwgrphc backend.        */
/*  whatlevel() comes from the backend (was HDISP.ASM leveltable).       */
/*  thaistrlen logic retained; the DOS B/W logic is unchanged.           */
/*-----------------------------------------------------------------------*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "cscrn.h"

/*  errorsound() ตัวจริงอยู่ใน SRC/COMMON/SOUND.C ของต้นฉบับ       */
/*  (ตอน link cw ตัวจริงจะถูกเลือก ไม่ใช่ weak ตัวนี้)             */
__attribute__((weak)) void errorsound(void)
{
}

/*  thaistrlen ตัวจริงอยู่ใน SCUW/TUTIL1.C ของต้นฉบับ — ที่นี่ประกาศเป็น
 *  weak เพื่อให้ standalone test link ได้ แต่ไม่ชนกับตัวจริงตอน link cw */
int thaistrlen(char *thaist) __attribute__((weak));

int thaistrlen(char *thaist)
{
    int count = 0;

    while (*thaist)
    {
        if (whatlevel((unsigned char)*thaist) == MIDDLE)
        {
            count++;
        }
        thaist++;
    }
    return(count);
}

/*  ---- งาน port: กล่อง splash ของ CW (SCUW/CW.C main) ------------------
 *  ต้นฉบับวาดกล่องกว้าง 44 คอลัมน์ ขอบซ้ายที่ col 17 (0-based) — งาน port
 *  ขยายไปทางซ้าย 16 คอลัมน์ เพื่อวางพระเกี้ยวไว้ในกล่องซ้ายสุด แล้วเลื่อน
 *  กล่องทั้งใบให้อยู่กลางพื้นที่ที่มองเห็นจริง (splash_base())
 *    - แถบบน/ล่าง (0x98/0x99, 0x9A/0x9B): วาดเองให้กว้าง 60 คอลัมน์
 *    - แถวข้อความที่ขึ้นต้นด้วย ' ' + 0x96: ย้ายขอบซ้ายมาที่ base
 *    - แถบพระเกี้ยว (ซ้ายสุด 16 คอลัมน์): ต้องเป็นพื้นขาวครบทุกแถวของ
 *      แถบ รวมแถวที่ไม่มีหมึก (แถวเหนือและล่างตัว logo) ไม่งั้นจะเห็น
 *      เป็นแถบดำคาดอยู่ด้านบนและด้านล่างของ logo                     */
#define SPLASH_W        60                 /* ความกว้างกล่อง (คอลัมน์) */
#define SPLASH_LOGO_W   16                 /* ความกว้างแถบพระเกี้ยว      */
#define SPLASH_TOP       5                 /* แถวขอบบนกล่อง             */
#define SPLASH_BOTTOM   13                 /* แถวขอบล่างกล่อง           */

/*  ขอบซ้ายของกล่อง ให้อยู่กลางพื้นที่ที่วาดได้จริง                      */
/*  UI ของ CW (กรอบ/ไทเทิลบาร์) กว้าง 80 คอลัมน์ตายตัว — จอ terminal ที่
 *  กว้างกว่านั้นเหลือที่ว่างด้านขวา ถ้าจัดกลางด้วยความกว้าง terminal
 *  กล่องจะเยื้องไปทางขวาของกรอบ จึง clamp ที่ 80                      */
#define SPLASH_FRAME_COLS 80

static unsigned int splash_base(void)
{
    int vis  = port_visible_cols();
    int base;

    if (vis > SPLASH_FRAME_COLS)
        vis = SPLASH_FRAME_COLS;
    base = (vis - SPLASH_W) / 2;

    if (base < 1)
        base = 1;
    return (unsigned int)base;
}

/*  กล่องต้องอยู่กลาง "ที่ว่าง" ไม่ใช่กลางจอทั้งใบ: ด้านบนมี UI ของ
 *  โปรแกรม (ไทเทิล bar, บรรทัดสถานะ, ruler) อยู่แถว 0-3 จึงจัดกลาง
 *  ในช่วงแถว SPLASH_AREA_TOP ถึงเหนือกรอบล่าง                       */
#define SPLASH_H         (SPLASH_BOTTOM - SPLASH_TOP + 1)
#define SPLASH_AREA_TOP   4

static unsigned int splash_dy(void)
{
    int vis    = port_visible_rows();
    int bottom = vis - 2;                   /* เหนือกรอบล่าง 1 แถว   */
    int area_h = bottom - SPLASH_AREA_TOP + 1;
    int top    = SPLASH_AREA_TOP + (area_h - SPLASH_H) / 2;
    int dy     = top - SPLASH_TOP;

    if (dy < 0)
        dy = 0;
    return (unsigned int)dy;
}

/*  เติม blank + attr ครบช่วงคอลัมน์ [x0..x1] ของแถว y                   */
static void splash_fill(unsigned int y, unsigned int x0, unsigned int x1,
                        font_attr attr)
{
    unsigned int x;

    for (x = x0; x <= x1; x++)
        prchar(' ', attr, x, y);
}

/*  แถบบน/ล่างของกล่อง splash (ขอบซ้ายอยู่ที่ base)                      */
static void splash_bar(unsigned int y, unsigned int left, unsigned int right,
                       font_attr attr)
{
    unsigned int base = splash_base();
    unsigned int x;

    prchar(left, attr, base, y);
    for (x = 1; x < SPLASH_W - 1; x++)
        prchar(0x95, attr, base + x, y);
    prchar(right, attr, base + SPLASH_W - 1, y);
}

/*****************************************************************************/
/* display string on screen                                                  */
/*      st   : string to be displayed                                        */
/*      x    : column position ( 0 - 89 )                                    */
/*      y    : row position ( 0 - 17..23 )                                   */
/*      attr : attribute of string                                           */
/*****************************************************************************/
void dispstrhgc(char *st, unsigned int x, unsigned int y, font_attr attr)
{
    /*  ---- splash screen ของ CW (SCUW/CW.C main) ----------------------
     *  ตรวจที่คอลัมน์เริ่มต้นของต้นฉบับเท่านั้น (16 = 21 - CENTER_FACTOR)
     *  เพื่อไม่ให้ไปโดนกล่องข้อความอื่นที่วาดด้วย REVERSEATTR ในย่านนี้
     *  (เช่น message box ของ MENU.C ที่ขึ้นต้นที่ col 45)              */
    if ((attr & REVERSEATTR) && (x == 16) &&
        (y >= SPLASH_TOP) && (y <= SPLASH_BOTTOM))
    {
        unsigned int base = splash_base();
        unsigned int off  = base - 1;   /* ระยะที่เลื่อนจากตำแหน่งเดิม */
        unsigned int dy   = splash_dy(); /* เลื่อนลงให้อยู่กลางที่ว่าง   */

        y += dy;                        /* แถวของกล่องหลังจัดกลาง       */

        if (((unsigned char)st[0] == ' ') && ((unsigned char)st[1] == 0x98))
        {
            splash_bar(y, 0x98, 0x99, attr);
            return;
        }
        if (((unsigned char)st[0] == ' ') && ((unsigned char)st[1] == 0x9A))
        {
            unsigned int fy;

            splash_bar(y, 0x9A, 0x9B, attr);

            /*  แถบพระเกี้ยวด้านซ้าย: เติมพื้นขาวให้ครบทุกแถวของแถบ
             *  ก่อน แล้วค่อยวาดพระเกี้ยวทับแถวกลาง ๆ                     */
            for (fy = SPLASH_TOP + 1 + dy; fy <= SPLASH_BOTTOM - 1 + dy;
                 fy++)
            {
                splash_fill(fy, base + 1, base + SPLASH_LOGO_W, attr);

                /*  ช่องว่างท้ายสตริงของแต่ละแถวถูกวาดด้วย attr ของกล่อง
                 *  ทำให้ขอบขวายื่นออกมาอีก 1 คอลัมน์ — ล้างให้เสมอขอบ   */
                prchar(' ', NORMALATTR, base + SPLASH_W, fy);
            }

            port_draw_crest(base + 1, SPLASH_TOP + 2 + dy,
                            SPLASH_LOGO_W, 5, attr);
            return;
        }
        if (((unsigned char)st[0] == ' ') && ((unsigned char)st[1] == 0x96))
        {
            prchar(0x96, attr, base, y);   /* ขอบซ้ายใหม่ที่ base      */
            st += 2;                       /* ข้าม " " + 0x96 เดิม      */
            x  += 2 + off;                 /* เลื่อนข้อความตามกล่อง     */
        }
    }

    while ((*st != '\0') && (x < 90))
    {
        if ((unsigned char)*st < 32)
        {
            togglefont(&attr, (font_code)*st);
        }
        else
        {
            if (whatlevel((unsigned char)*st) == MIDDLE)
            {
                prchar((unsigned char)*st, attr, x, y);
                if ((attr & ENLARGEATTR) == ENLARGEATTR)
                {
                    /*  ครึ่งหลังของอักษรขยาย: ต้นฉบับยืด glyph ให้กว้าง
                     *  2 คอลัมน์ แต่ terminal ยืด glyph ไม่ได้ จึงเติม
                     *  blank ด้วย attr เดียวกัน — ถ้าไม่เติม เซลล์นั้นจะ
                     *  ค้างเป็นพื้นหลังเดิม เห็นเป็นกล่องดำคั่นกลางแถบ    */
                    prchar(' ', attr, x + 1, y);
                    x++;
                }
                x++;
            }
            else
            {
                if ((attr & ENLARGEATTR) == ENLARGEATTR)
                {
                    if (x >= 2)
                    {
                        prchar((unsigned char)*st, attr, x - 2, y);
                    }
                }
                else
                {
                    if (x >= 1)
                    {
                        prchar((unsigned char)*st, attr, x - 1, y);
                    }
                }
            }
        }
        st++;
    }

}

/****************************************************************************/
/* display string with printf capability                                    */
/****************************************************************************/
void dispprintf(unsigned int x, unsigned int y, font_attr attr,
                char *format, ...)
{
    va_list argptr;
    char tstring[240];

    va_start(argptr, format);
    vsprintf(tstring, format, argptr);
    va_end(argptr);
    dispstrhgc(tstring, x, y, attr);
}

/*****************************************************************************/
/* display blank                                                             */
/*****************************************************************************/
void dispblank(unsigned int x, unsigned int y, unsigned int count,
               font_attr attr)
{
    count++;
    while (count--)
    {
        prchar(' ', attr, x++, y);
    }
}

/****************************************************************************/
/*  Set bit in attribute byte on/off from ascii control code given.         */
/****************************************************************************/
void togglefont(font_attr *curfont, font_code code)
{
    if (code == ONELINECODE)
        *curfont = *curfont ^ ONELINEATTR;
    if (code == SUPERCODE)
        *curfont = *curfont ^ SUPERATTR;
    if (code == SUBCODE)
        *curfont = *curfont ^ SUBATTR;
    if (code == ITALICCODE)
        *curfont = *curfont ^ ITALICATTR;
    if (code == BOLDCODE)
        *curfont = *curfont ^ BOLDATTR;
    if (code == TWOLINECODE)
        *curfont = *curfont ^ TWOLINEATTR;
    if (code == ENLARGECODE)
        *curfont = *curfont ^ ENLARGEATTR;
}

/*
Draw frame box on screen
Box chars are TIS-620 (CP874-style line art):
  0x95 horizontal, 0x96 vertical, 0x98 top-left,
  0x99 top-right, 0x9A bottom-left, 0x9B bottom-right
*/
#define BOX_H   0x95
#define BOX_V   0x96
#define BOX_TL  0x98
#define BOX_TR  0x99
#define BOX_BL  0x9A
#define BOX_BR  0x9B

void framebox(unsigned int x1, unsigned int y1,
              unsigned int x2, unsigned int y2,
              unsigned int attr)
{
    unsigned int i, j;

    i = x1;
    prchar(' ', attr, i++, y1);
    prchar(BOX_TL, attr, i,   y1);
    for (i++, j = x2 - 1; i < j; i++)
    {
        prchar(BOX_H, attr, i, y1);
    }
    prchar(BOX_TR, attr, i++, y1);
    prchar(' ', attr, i,   y1);

    for (j = y1 + 1; j < y2; j++)
    {
        for (i = x1; i <= x2; i++)
        {
            prchar(' ', attr, i, j);
        }
        prchar(BOX_V, attr, x2 - 1, j);
        prchar(BOX_V, attr, x1 + 1, j);
    }
    i = x1;
    prchar(' ', attr, i++, y2);
    prchar(BOX_BL, attr, i,   y2);
    for (i++, j = x2 - 1; i < j; i++)
    {
        prchar(BOX_H, attr, i, y2);
    }
    prchar(BOX_BR, attr, i++, y2);
    prchar(' ', attr, i,   y2);
}

void blockmsg(int y)
{
    framebox(13, y - 1, 13 + 53, y + 1, REVERSEATTR);
}

/****************************************************************************/
/*  Save content of screen. Return pointer to content of screen.            */
/****************************************************************************/
/*  ระหว่าง popup (dropdown menu / กล่องข้อความ) ต้นฉบับวาดกรอบบนที่พิกเซล
 *  y*20+2 คือ *แถวเดียวกับ ruler* (ruler อยู่พิกเซล y*20) ทำให้กรอบบนของ
 *  dropdown หลอมเป็นเส้นเดียวกับ ruler — terminal แยกไม่ได้
 *  => เลื่อนการวาดของ popup ลง 1 แถว (ดู port_popup_shift) ให้กรอบบนมี
 *     แถวของตัวเอง แล้ว save/restore เพิ่มอีก 1 แถวไม่ให้เหลือเศษ        */
int port_popup_shift = 0;
#define PORT_POPUP_FIRST_ROW_PX 60      /* แถว 3 ขึ้นไปเท่านั้นที่เลื่อน */

char *savescrn(int x1, int y1, int x2, int y2)
{
    char *scrnindex;
    int extra = 1;   /* popup จะถูกวาดเลื่อนลง 1 แถวหลัง save — ต้องเผื่อไว้ */

    scrnindex = (char *) malloc((x2 - x1 + 1) * (y2 - y1 + 1 + extra) * 20);
    if (scrnindex == NULL)
    {
        savepic();
        blockmsg(10);
        dispstrhgc("˹¤ѧ ! ѧͷӧҹ...",
                   23, 10, REVERSEATTR);
        return(NULL);
    }

    (*getwind_ptr)(x1, y1, (y2 - y1 + 1 + extra) * 20, (x2 - x1 + 1), scrnindex);
    port_popup_shift = 1;                   /* ต่อจากนี้วาด popup เลื่อนลง */
    return(scrnindex);
}

/****************************************************************************/
/*  Restore content of screen.                                              */
/****************************************************************************/
void resscrn(char *scrnindex, int x1, int y1, int x2, int y2)
{
    int extra = port_popup_shift ? 1 : 0;

    if (scrnindex == NULL)
    {
        port_popup_shift = 0;
        retpic();
        return;
    }
    (*putwind_ptr)(x1, y1, (y2 - y1 + 1 + extra) * 20, (x2 - x1 + 1), scrnindex);
    port_popup_shift = 0;
    free(scrnindex);
}

/*-----------------------------------------------------------------------*/
/*  ที่เหลือของ COMMON/CSCRN.C ที่ port ยังไม่ได้ทำ                       */
/*  (ย้ายมาไว้ท้ายไฟล์เพื่อไม่ให้ปนกับของเดิม)                            */
/*                                                                       */
/*  _line/_rectangle วาดเส้นแบบ pixel ตามอัลกอริทึม Bresenham ตัวเดียวกับ  */
/*  ต้นฉบับ โดยเรียก plot() ของ backend (coarse 8x20 dot approximation)   */
/*-----------------------------------------------------------------------*/

int sign(int x)
{
    if (abs(x) != x)
    {
        return(-1);
    }
    else
    {
        if (x == 0)
        {
            return(0);
        }
        else
        {
            return(1);
        }
    }
}

void _line(int x1, int y1, int x2, int y2)
{
    int x, y;
    int delx, dely;

    /*  popup เลื่อนลง 1 แถว (ดู port_popup_shift)                    */
    if (port_popup_shift && y1 >= PORT_POPUP_FIRST_ROW_PX)
    {
        y1 += 20;
        y2 += 20;
    }
    int s1, s2, change, weight, temp, i;

    x = x1;
    y = y1;
    delx = abs(x2 - x1);
    dely = abs(y2 - y1);
    s1 = sign(x2 - x1);
    s2 = sign(y2 - y1);
    if (dely > delx)
    {
        temp = delx;
        delx = dely;
        dely = temp;
        change = 1;
    }
    else
    {
        change = 0;
    }
    weight = dely + dely - delx;
    for (i = 1; i <= delx; i++)
    {
        plot(x, y);
        while (weight >= 0)
        {
            if (change == 1)
            {
                x += s1;
            }
            else
            {
                y += s2;
            }
            weight -= delx + delx;
        }
        if (change == 1)
        {
            y += s2;
        }
        else
        {
            x += s1;
        }
        weight += dely + dely;
    }
}

void _rectangle(int x1, int y1, int x2, int y2)
{
    _line(x1, y1, x1, y2);
    _line(x1, y1, x2, y1);
    _line(x2, y1, x2, y2);
    _line(x1, y2, x2, y2);
}

/*  ข้อความไทยเป็น TIS-620 ตามต้นฉบับ (เขียนเป็น \xNN ให้ไฟล์เป็น ASCII)  */
void showerrno(void)
{
    errorsound();
    blockmsg(10);
    switch (errno)
    {
        case ENOENT:
            dispstrhgc("\xcb\xd2\xe1\xbf\xe9\xc1\xa2\xe9\xcd\xc1\xd9\xc5\xe4\xc1\xe8\xbe\xba ! \xa1\xb4\xbb\xd8\xe8\xc1\xe3\xb4\xe6\xe0\xbe\xd7\xe8\xcd\xb7\xd3\xa7\xd2\xb9\xb5\xe8\xcd...",
                    21, 10, REVERSEATTR);
            break;
        case ENOMEM:
            dispstrhgc("\xcb\xb9\xe8\xc7\xc2\xa4\xc7\xd2\xc1\xa8\xd3\xe4\xc1\xe8\xbe\xcd ! \xa1\xb4\xbb\xd8\xe8\xc1\xe3\xb4\xe6\xe0\xbe\xd7\xe8\xcd\xb7\xd3\xa7\xd2\xb9\xb5\xe8\xcd...",
                    23, 10, REVERSEATTR);
            break;
        default:
            framebox(13, 9, 66, 12, REVERSEATTR);
            dispstrhgc("\xe0\xa1\xd4\xb4\xa4\xc7\xd2\xc1\xbc\xd4\xb4\xbe\xc5\xd2\xb4\xc0\xd2\xc2\xe3\xb9\xc3\xd0\xba\xba \xa1\xc3\xd8\xb3\xd2\xe1\xa8\xe9\xa7\xe4\xbb\xb7\xd5\xe8 \xa8\xd8\xcc\xd2\xcf",
                    23, 10, REVERSEATTR);
            dispstrhgc(strerror(errno), 23, 11, REVERSEATTR);
    }
    port_getch();   /* แทน ebioskey(0) ของ EKBD.C - รอคีย์จาก terminal */
}
