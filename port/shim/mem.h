/* =====================================================================
 * mem.h - Turbo C <mem.h> replacement for the ThaiWrite Linux port
 * ---------------------------------------------------------------------
 * memcpy/memmove/memset/memcmp/memchr come from <string.h>.  The Turbo C
 * only "far" variants and movmem()/setmem() are added here (they just
 * call the near versions).
 * ===================================================================== */
#ifndef _SHIM_MEM_H
#define _SHIM_MEM_H

#include "shim_turbo.h"
#include <string.h>
#include <strings.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Turbo C's movmem() takes (src, dst, len) - note the argument order,
 * the opposite of memmove(). */
void movmem(void *src, void *dst, unsigned len);
void setmem(void *dst, unsigned len, char value);

/* "far" memory functions -> the plain ones */
#define _fmemcpy(d, s, n)  memcpy((d), (s), (n))
#define _fmemmove(d, s, n) memmove((d), (s), (n))
#define _fmemset(d, v, n)  memset((d), (v), (n))
#define _fmemcmp(a, b, n)  memcmp((a), (b), (n))
#define _fmemchr(s, c, n)  memchr((s), (c), (n))
#define _fmemicmp(a, b, n) strncasecmp((const char *)(a), (const char *)(b), (n))
#define _fstrlen(s)        strlen(s)
#define _fstrcpy(d, s)     strcpy((d), (s))
#define _fstrncpy(d, s, n) strncpy((d), (s), (n))
#define _fstrcat(d, s)     strcat((d), (s))
#define _fstrncat(d, s, n) strncat((d), (s), (n))
#define _fstrcmp(a, b)     strcmp((a), (b))
#define _fstrncmp(a, b, n) strncmp((a), (b), (n))
#define _fstricmp(a, b)    strcasecmp((a), (b))
#define _fstrchr(s, c)     strchr((s), (c))
#define _fstrrchr(s, c)    strrchr((s), (c))
#define _fstrstr(a, b)     strstr((a), (b))
#define _fstrdup(s)        strdup(s)

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_MEM_H */
