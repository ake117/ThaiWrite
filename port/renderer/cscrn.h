#ifndef PORT_RENDERER_CSCRN_H
#define PORT_RENDERER_CSCRN_H

/* dispstrhgc / dispprintf / dispblank / togglefont / framebox /
   blockmsg / savescrn / resscrn - port of COMMON/CSCRN.C.
   Kept in the renderer so a main() can adopt them piecemeal. */

#include "cwgrphc.h"

void dispstrhgc(char *st, unsigned int x, unsigned int y, font_attr attr);
void dispprintf(unsigned int x, unsigned int y, font_attr attr,
                char *format, ...);
void dispblank(unsigned int x, unsigned int y, unsigned int count,
               font_attr attr);
void togglefont(font_attr *curfont, font_code code);
void framebox(unsigned int x1, unsigned int y1,
              unsigned int x2, unsigned int y2,
              unsigned int attr);
void blockmsg(int y);
char *savescrn(int x1, int y1, int x2, int y2);
void resscrn(char *scrnindex, int x1, int y1, int x2, int y2);

#endif
