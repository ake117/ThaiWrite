/* =====================================================================
 * string.h - Turbo C <string.h> compatibility for the ThaiWrite port
 * ---------------------------------------------------------------------
 * Adds the Turbo C-only case-conversion helpers (strupr/strlwr/...) that
 * glibc does not provide.  The POSIX/ISO C functions come from the real
 * <string.h> via include_next.
 * ===================================================================== */
#ifndef _SHIM_STRING_H
#define _SHIM_STRING_H

#include_next <string.h>
#include <strings.h>

#ifdef __cplusplus
extern "C" {
#endif

char *strupr(char *s);
char *strlwr(char *s);
char *strrev(char *s);

int stricmp(const char *s1, const char *s2);
int strcmpi(const char *s1, const char *s2);
int strnicmp(const char *s1, const char *s2, size_t n);
int strncmpi(const char *s1, const char *s2, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_STRING_H */
