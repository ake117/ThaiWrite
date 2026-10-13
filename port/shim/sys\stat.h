/* =====================================================================
 * sys\stat.h  (literal backslash in the name, as DOS wrote it)
 * ---------------------------------------------------------------------
 * The original sources contain  #include <sys\stat.h>  and gcc does NOT
 * translate the backslash, so it looks for a file literally named
 * "sys\stat.h".  This stub satisfies that lookup and forwards to the
 * real shim/sys/stat.h.
 * ===================================================================== */
#ifndef _SHIM_SYS_BACKSLASH_STAT_H
#define _SHIM_SYS_BACKSLASH_STAT_H

#include "sys/stat.h"

#endif /* _SHIM_SYS_BACKSLASH_STAT_H */
