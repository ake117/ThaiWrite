/* =====================================================================
 * sys/stat.h - compatibility wrapper for the ThaiWrite port
 * ---------------------------------------------------------------------
 * The original sources spell this header "sys\stat.h" (DOS separator).
 * shim/sys\stat.h (a literal backslash in the filename) forwards here,
 * and here we pull in the real POSIX header.
 * ===================================================================== */
#ifndef _SHIM_SYS_STAT_H
#define _SHIM_SYS_STAT_H

#include_next <sys/stat.h>

/* Turbo C's stat mode bits used by the sources (none at present) */
#ifndef S_IFMT
#define S_IFMT   0170000
#endif
#ifndef S_IFDIR
#define S_IFDIR  0040000
#endif
#ifndef S_IFREG
#define S_IFREG  0100000
#endif

#endif /* _SHIM_SYS_STAT_H */
