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

/*****************************************************************************/
/* display string on screen                                                  */
/*      st   : string to be displayed                                        */
/*      x    : column position ( 0 - 89 )                                    */
/*      y    : row position ( 0 - 17..23 )                                   */
/*      attr : attribute of string                                           */
/*****************************************************************************/
void dispstrhgc(char *st, unsigned int x, unsigned int y, font_attr attr)
{
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
        *curfont = *curfont ^ ENLARGECODE;
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
