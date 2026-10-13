/* =====================================================================
 * io.h - Turbo C <io.h> replacement for the ThaiWrite Linux port
 * ---------------------------------------------------------------------
 * The POSIX prototypes (open/close/read/write/dup/unlink/...) come from
 * <unistd.h> and <fcntl.h>.  This header only adds the Turbo C spellings
 * (_open, _close, _read, _write, _creat, _dup) plus filelength()/eof().
 *
 * open()/creat()/fopen()/unlink()/remove()/rename()/chdir()/mkdir()/
 * rmdir() are interposed at link time by shim_io.c so that DOS-style
 * backslash separators in paths ("%s\\TW.CFG") are normalised to '/'.
 * ===================================================================== */
#ifndef _SHIM_IO_H
#define _SHIM_IO_H

#include "shim_turbo.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Turbo C spellings.  These route through the interposed functions so
 * path normalisation happens automatically. */
#ifndef _open
#define _open   open
#endif
#ifndef _close
#define _close  close
#endif
#ifndef _read
#define _read   read
#endif
#ifndef _write
#define _write  write
#endif
#ifndef _dup
#define _dup    dup
#endif
#ifndef _creat
#define _creat  creat
#endif
#ifndef _dup2
#define _dup2   dup2
#endif

/* Not present in POSIX */
long filelength(int handle);
int  eof(int handle);
long tell(int handle);

/* DOS attribute values accepted by Turbo C's creat()/open().
 * Also declared in dir.h; guarded so whichever is included first wins. */
#ifndef FA_NORMAL
#define FA_NORMAL   0x00
#define FA_RDONLY   0x01
#define FA_HIDDEN   0x02
#define FA_SYSTEM   0x04
#define FA_LABEL    0x08
#define FA_DIREC    0x10
#define FA_ARCH     0x20
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_IO_H */
