/* =====================================================================
 * stdio.h - Turbo C <stdio.h> compatibility for the ThaiWrite port
 * ---------------------------------------------------------------------
 * The ISO C prototypes come from the real <stdio.h> via include_next.
 * fopen()/remove()/rename() are additionally interposed at link time by
 * shim_io.c so DOS backslash separators in paths are normalised.
 * ===================================================================== */
#ifndef _SHIM_STDIO_H
#define _SHIM_STDIO_H

#include_next <stdio.h>

#endif /* _SHIM_STDIO_H */
