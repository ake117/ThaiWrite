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

/*  แถบบน/ล่างของกล่อง splash — งาน port ขยายกล่องไปทางซ้ายถึงคอลัมน์ 1
 *  (กว้าง 60 คอลัมน์) เพื่อให้มีที่วางพระเกี้ยวในกล่อง                 */
static void splash_bar(unsigned int y, unsigned int left, unsigned int right,
                       font_attr attr)
{
    unsigned int x;

    prchar(left, attr, 1, y);
    for (x = 2; x <= 59; x++)
    {
        prchar(0x95, attr, x, y);
    }
    prchar(right, attr, 60, y);
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
     *  งาน port ขยายกล่อง splash ไปทางซ้ายถึงคอลัมน์ 1 เพื่อวางพระเกี้ยว
     *  ไว้ในกล่องซ้ายสุด แล้ววาดเป็น "ดำบนขาว" (REVERSEATTR เหมือนกล่อง)
     *    - แถบบน/ล่าง (0x98..0x99 / 0x9A..0x9B): วาดเองให้กว้าง 60 col
     *    - แถวข้อความที่ขึ้นต้นด้วย ' ' + 0x96 (ขอบซ้ายเดิม col 17):
     *      ย้ายขอบซ้ายมาที่ col 1 แล้วไม่วาด 2 ตัวนั้น (เว้นเป็นช่อง)    */
    if ((attr & REVERSEATTR) && x >= 16 && y >= 5 && y <= 13)
    {
        if (((unsigned char)st[0] == ' ') && ((unsigned char)st[1] == 0x98))
        {
            splash_bar(y, 0x98, 0x99, attr);
            return;
        }
        if (((unsigned char)st[0] == ' ') && ((unsigned char)st[1] == 0x9A))
        {
            splash_bar(y, 0x9A, 0x9B, attr);
            port_draw_crest(2, 7, 16, 5, (font_attr)(attr | REVERSEATTR));
            return;
        }
        if (((unsigned char)st[0] == ' ') && ((unsigned char)st[1] == 0x96))
        {
            prchar(0x96, attr, 1, y);   /* ขอบซ้ายใหม่ที่คอลัมน์ 1 */
            st += 2;                    /* ข้าม " " + 0x96 เดิม      */
            x += 2;
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
char *savescrn(int x1, int y1, int x2, int y2)
{
    char *scrnindex;

    scrnindex = (char *) malloc((x2 - x1 + 1) * (y2 - y1 + 1) * 20);
    if (scrnindex == NULL)
    {
        savepic();
        blockmsg(10);
        dispstrhgc("˹¤ѧ ! ѧͷӧҹ...",
                   23, 10, REVERSEATTR);
        return(NULL);
    }

    (*getwind_ptr)(x1, y1, (y2 - y1 + 1) * 20, (x2 - x1 + 1), scrnindex);
    return(scrnindex);
}

/****************************************************************************/
/*  Restore content of screen.                                              */
/****************************************************************************/
void resscrn(char *scrnindex, int x1, int y1, int x2, int y2)
{
    if (scrnindex == NULL)
    {
        retpic();
        return;
    }
    (*putwind_ptr)(x1, y1, (y2 - y1 + 1) * 20, (x2 - x1 + 1), scrnindex);
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
