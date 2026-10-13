/* =====================================================================
 * stdlib.h - Turbo C <stdlib.h> compatibility for the ThaiWrite port
 * ---------------------------------------------------------------------
 * Adds the Turbo C-only integer-to-string helpers (itoa/ltoa/ultoa) and
 * the far-heap spellings.  The ISO C functions come from the real
 * <stdlib.h> via include_next.
 * ===================================================================== */
#ifndef _SHIM_STDLIB_H
#define _SHIM_STDLIB_H

#include_next <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

char *itoa(int value, char *string, int radix);
char *ltoa(long value, char *string, int radix);
char *ultoa(unsigned long value, char *string, int radix);

/* Turbo C's non-const allocator spellings */
#define _fmalloc(n)     malloc(n)
#define _ffree(p)       free(p)
#define _fcalloc(n, s)  calloc((n), (s))
#define _frealloc(p, n) realloc((p), (n))

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_STDLIB_H */
