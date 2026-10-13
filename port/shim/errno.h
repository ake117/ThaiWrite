/* =====================================================================
 * errno.h - Turbo C <errno.h> compatibility for the ThaiWrite port
 * ---------------------------------------------------------------------
 * SRC/COMMON/CSCRN.C prints sys_errlist[errno].  Modern glibc no longer
 * declares sys_errlist, so the shim provides its own table and renames
 * the identifier to avoid clashing with the legacy libc symbol.
 * ===================================================================== */
#ifndef _SHIM_ERRNO_H
#define _SHIM_ERRNO_H

#include_next <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* shim_sys_errlist[] is built from strerror() in shim_compat.c and is
 * indexed 0..255, so an out-of-range errno is still safe to read. */
extern char *shim_sys_errlist[];
extern int   shim_sys_nerr;

#ifndef sys_errlist
#define sys_errlist shim_sys_errlist
#endif
#ifndef sys_nerr
#define sys_nerr shim_sys_nerr
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_ERRNO_H */
