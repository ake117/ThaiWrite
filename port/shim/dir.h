/* =====================================================================
 * dir.h - Turbo C <dir.h> replacement for the ThaiWrite Linux port
 * ---------------------------------------------------------------------
 * findfirst()/findnext() are implemented in shim_dir.c on top of
 * opendir/readdir, with DOS wildcard matching and DOS attribute
 * semantics (FA_DIREC = directories only, FA_NORMAL = files only).
 * "." and ".." are returned exactly like DOS does, so the original
 * SRC/COMMON/DIR.C "." filter keeps working unchanged.
 *
 * fnsplit()/fnmerge() are exact inverses; the separator present in the
 * input path (either '/' or '\') is preserved so the round-trip check
 * in SRC/SCUW/GETSTR.C:218 behaves as it did on DOS.
 *
 * chdir/mkdir/rmdir/getcwd keep their POSIX prototypes (unistd.h,
 * sys/stat.h) - only the Turbo C-only entry points are declared here.
 * ===================================================================== */
#ifndef _SHIM_DIR_H
#define _SHIM_DIR_H

#include "shim_turbo.h"
#include <unistd.h>
#include <sys/stat.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Same constants as Turbo C 2.01 */
#ifndef MAXPATH
#define MAXPATH   80
#endif
#ifndef MAXDRIVE
#define MAXDRIVE  3
#endif
#ifndef MAXDIR
#define MAXDIR    66
#endif
#ifndef MAXFILE
#define MAXFILE   9
#endif
#ifndef MAXEXT
#define MAXEXT    5
#endif

#ifndef FA_NORMAL
#define FA_NORMAL 0x00
#define FA_RDONLY 0x01
#define FA_HIDDEN 0x02
#define FA_SYSTEM 0x04
#define FA_LABEL  0x08
#define FA_DIREC  0x10
#define FA_ARCH   0x20
#endif

/* Same layout as Turbo C 2.01's <dir.h> (reserved area first) */
struct ffblk
{
    char ff_reserved[21];
    char ff_attrib;
    unsigned short ff_ftime;
    unsigned short ff_fdate;
    unsigned long ff_fsize;
    char ff_name[13];
};

/* fnsplit() return flags (bit masks, as Turbo C uses them) */
#ifndef DRIVE
#define DRIVE      1
#define DIRECTORY  2
#define FILENAME   4
#define EXTENSION  8
#endif

/* Free-space reporting (Turbo C's <dir.h>) */
struct dfree
{
    unsigned df_avail;      /* available clusters          */
    unsigned df_total;      /* total clusters              */
    unsigned df_bsec;       /* bytes per sector            */
    unsigned df_sclus;      /* sectors per cluster         */
};

void getdfree(unsigned char drive, struct dfree *dfreep);

int findfirst(const char *pathname, struct ffblk *ffblk, int attrib);
int findnext(struct ffblk *ffblk);

/* ff_name is 13 bytes, as in Turbo C, so a Unix name longer than 12
 * characters is truncated in ffblk.  Port-owned code that needs the
 * real name (e.g. to open the file) should call this instead; the
 * buffer is valid until the next findfirst()/findnext(). */
const char *shim_find_fullname(void);

/* filename parsing */
int  fnsplit(const char *path, char *drive, char *dir, char *name, char *ext);
void fnmerge(char *path, const char *drive, const char *dir,
             const char *name, const char *ext);

/* "drive" handling: a single pseudo drive, index 0 */
int  getdisk(void);
int  setdisk(int drive);
int  getcurdir(int drive, char *direc);

/* Turbo C's searchpath() (searches the PATH for a file) */
char *searchpath(const char *file);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_DIR_H */
