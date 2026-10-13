/* =====================================================================
 * conio.h - Turbo C <conio.h> replacement for the ThaiWrite Linux port
 * ---------------------------------------------------------------------
 * Console input is served by the same key queue as bioskey(); getch()
 * follows Turbo C semantics: it returns the ASCII byte, or 0 for an
 * extended key (the scan code is returned by the *next* getch()).
 * The text-mode output calls are stubs, because CU-Writer runs in its
 * own graphics emulation, not in the terminal text plane.
 * ===================================================================== */
#ifndef _SHIM_CONIO_H
#define _SHIM_CONIO_H

#include "shim_turbo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Turbo C text modes */
#define BW40    0
#define C40     1
#define BW80    2
#define C80     3
#define MONO    7
#define LASTMODE -1

int  getch(void);
int  getche(void);
int  kbhit(void);
int  ungetch(int ch);
int  putch(int ch);
char *cgets(char *str);
int  cprintf(const char *format, ...);
int  cputs(const char *str);

void clrscr(void);
void clreol(void);
void delline(void);
void insline(void);
void gotoxy(int x, int y);
int  wherex(void);
int  wherey(void);
void textmode(int newmode);
void window(int left, int top, int right, int bottom);
void textattr(int newattr);
void textcolor(int newcolor);
void textbackground(int newcolor);
void highvideo(void);
void lowvideo(void);
void normvideo(void);
void _setcursortype(int cur_t);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_CONIO_H */
