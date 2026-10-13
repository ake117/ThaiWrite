/* =====================================================================
 * shim_conio.c - Turbo C <conio.h> replacement for the ThaiWrite port
 * ---------------------------------------------------------------------
 * getch()/getche()/kbhit()/ungetch()/putch() are served from the same key
 * queue as bioskey().  getch() follows Turbo C semantics: the ASCII byte
 * is returned, or 0 for an extended key, in which case the next getch()
 * returns the scan code.  The text-mode output calls are stubs, because
 * CU-Writer paints its own screen instead of using the terminal plane.
 * ===================================================================== */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "shim_turbo.h"
#include "bios.h"
#include "conio.h"

/* The scan code stashed by getch() when it returns 0, and a one-slot
 * push-back slot for ungetch(). */
static int shim_getch_pending_scan = -1;
static int shim_unget_ascii = -1;

int getch(void)
{
    int keycode;

    if (shim_getch_pending_scan >= 0)
    {
        keycode = shim_getch_pending_scan;
        shim_getch_pending_scan = -1;
        return keycode;
    }

    if (shim_unget_ascii >= 0)
    {
        keycode = shim_unget_ascii;
        shim_unget_ascii = -1;
        return keycode;
    }

    keycode = bioskey(0);

    if ((keycode & 0xFF) != 0)
    {
        return keycode & 0xFF;
    }
    /* extended key: Turbo C returns 0 first, then the scan code */
    shim_getch_pending_scan = (keycode >> 8) & 0xFF;
    return 0;
}

int getche(void)
{
    int c = getch();

    if (c != 0)
    {
        putch(c);
    }
    return c;
}

int kbhit(void)
{
    return bioskey(1);
}

int ungetch(int ch)
{
    shim_unget_ascii = ch & 0xFF;
    return ch;
}

int putch(int ch)
{
    fputc(ch & 0xFF, stdout);
    fflush(stdout);
    return ch;
}

char *cgets(char *str)
{
    /* Turbo C: str[0] = max length in, str[1] = actual length out. */
    int maxlen = (unsigned char)str[0];
    int n = 0;
    int c;

    while (n < maxlen - 1)
    {
        c = getch();
        if (c == 0)
        {
            continue;               /* skip extended keys */
        }
        if (c == '\r' || c == '\n')
        {
            break;
        }
        if (c == 8 || c == 0x7F)
        {
            if (n > 0)
            {
                n--;
            }
            continue;
        }
        str[2 + n] = (char)c;
        n++;
    }
    str[1] = (char)n;
    str[2 + n] = '\0';
    return str + 2;
}

int cprintf(const char *format, ...)
{
    va_list ap;
    int n;

    va_start(ap, format);
    n = vfprintf(stdout, format, ap);
    va_end(ap);
    fflush(stdout);
    return n;
}

int cputs(const char *str)
{
    fputs(str, stdout);
    fflush(stdout);
    return 0;
}

/* ---- text-mode stubs ---------------------------------------------- */
void clrscr(void)     { fputs("\033[2J\033[H", stdout); fflush(stdout); }
void clreol(void)     { }
void delline(void)    { }
void insline(void)    { }
void gotoxy(int x, int y) { (void)x; (void)y; }
int  wherex(void)     { return 0; }
int  wherey(void)     { return 0; }
void textmode(int newmode) { (void)newmode; }
void window(int left, int top, int right, int bottom)
{
    (void)left; (void)top; (void)right; (void)bottom;
}
void textattr(int newattr)      { (void)newattr; }
void textcolor(int newcolor)    { (void)newcolor; }
void textbackground(int c)      { (void)c; }
void highvideo(void)  { }
void lowvideo(void)   { }
void normvideo(void)  { }
void _setcursortype(int cur_t)  { (void)cur_t; }
