/* =====================================================================
 * fcntl.h - Turbo C <fcntl.h> compatibility for the ThaiWrite port
 * ---------------------------------------------------------------------
 * Adds the DOS-only open flags that Turbo C had.  Defining O_BINARY as 0
 * also happens to repair the original source's "O_RDONLY || O_BINARY"
 * expressions (logical OR): with O_BINARY == 0 they collapse to the
 * intended O_RDONLY instead of the accidental O_WRONLY (1).
 * ===================================================================== */
#ifndef _SHIM_FCNTL_H
#define _SHIM_FCNTL_H

#include_next <fcntl.h>

#ifndef O_BINARY
#define O_BINARY 0      /* DOS text/binary distinction does not exist here */
#endif
#ifndef O_TEXT
#define O_TEXT 0
#endif
#ifndef _O_BINARY
#define _O_BINARY 0
#endif
#ifndef _O_TEXT
#define _O_TEXT 0
#endif

#endif /* _SHIM_FCNTL_H */
