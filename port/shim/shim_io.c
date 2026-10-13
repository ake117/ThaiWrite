/* =====================================================================
 * shim_io.c - file/path interposition for the ThaiWrite Linux port
 * ---------------------------------------------------------------------
 * The original sources build paths with DOS separators, e.g.
 *      sprintf(fname, "%s\\TW.CFG", cw_dir);
 * and gcc does not translate those.  Instead of patching the sources, the
 * shim interposes the relevant POSIX entry points (they are resolved from
 * this object before libc) and normalises the separator on the way in.
 *
 * A path containing a backslash is never valid on Linux, so rewriting
 * '\' to '/' is always the intended meaning.  The real libc functions are
 * reached through dlsym(RTLD_NEXT, ...).
 * ===================================================================== */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

#include "shim_internal.h"
#include "io.h"

static void *shim_next(const char *name)
{
    return dlsym(RTLD_NEXT, name);
}

/* ---------------------------------------------------------------- */
/*  open / open64 / creat / creat64                                 */
/* ---------------------------------------------------------------- */
int open(const char *path, int flags, ...)
{
    static int (*real)(const char *, int, ...);
    char buf[PATH_MAX];
    mode_t mode = 0;

    if (flags & (O_CREAT | O_TMPFILE))
    {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    if (real == NULL)
    {
        real = (int (*)(const char *, int, ...))shim_next("open");
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)), flags, mode);
}

int open64(const char *path, int flags, ...)
{
    static int (*real)(const char *, int, ...);
    char buf[PATH_MAX];
    mode_t mode = 0;

    if (flags & (O_CREAT | O_TMPFILE))
    {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    if (real == NULL)
    {
        real = (int (*)(const char *, int, ...))shim_next("open64");
        if (real == NULL)
        {
            real = (int (*)(const char *, int, ...))shim_next("open");
        }
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)), flags, mode);
}

int creat(const char *path, mode_t mode)
{
    static int (*real)(const char *, mode_t);
    char buf[PATH_MAX];

    if (real == NULL)
    {
        real = (int (*)(const char *, mode_t))shim_next("creat");
    }
    /* Turbo C callers pass a DOS attribute (e.g. FA_ARCH) as "mode", which
     * would create an unreadable file; force the usual 0666 instead. */
    (void)mode;
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)), 0666);
}

int creat64(const char *path, mode_t mode)
{
    static int (*real)(const char *, mode_t);
    char buf[PATH_MAX];

    if (real == NULL)
    {
        real = (int (*)(const char *, mode_t))shim_next("creat64");
        if (real == NULL)
        {
            real = (int (*)(const char *, mode_t))shim_next("creat");
        }
    }
    (void)mode;
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)), 0666);
}

/* ---------------------------------------------------------------- */
/*  fopen / fopen64                                                 */
/* ---------------------------------------------------------------- */
FILE *fopen(const char *path, const char *mode)
{
    static FILE *(*real)(const char *, const char *);
    char buf[PATH_MAX];

    if (real == NULL)
    {
        real = (FILE *(*)(const char *, const char *))shim_next("fopen");
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)), mode);
}

FILE *fopen64(const char *path, const char *mode)
{
    static FILE *(*real)(const char *, const char *);
    char buf[PATH_MAX];

    if (real == NULL)
    {
        real = (FILE *(*)(const char *, const char *))shim_next("fopen64");
        if (real == NULL)
        {
            real = (FILE *(*)(const char *, const char *))shim_next("fopen");
        }
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)), mode);
}

/* ---------------------------------------------------------------- */
/*  remove / rename / unlink / access / chdir / mkdir / rmdir        */
/* ---------------------------------------------------------------- */
int remove(const char *path)
{
    static int (*real)(const char *);
    char buf[PATH_MAX];

    if (real == NULL)
    {
        real = (int (*)(const char *))shim_next("remove");
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)));
}

int rename(const char *oldpath, const char *newpath)
{
    static int (*real)(const char *, const char *);
    char b1[PATH_MAX], b2[PATH_MAX];

    if (real == NULL)
    {
        real = (int (*)(const char *, const char *))shim_next("rename");
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(oldpath, b1, sizeof(b1)),
                shim_norm_path(newpath, b2, sizeof(b2)));
}

int unlink(const char *path)
{
    static int (*real)(const char *);
    char buf[PATH_MAX];

    if (real == NULL)
    {
        real = (int (*)(const char *))shim_next("unlink");
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)));
}

int access(const char *path, int amode)
{
    static int (*real)(const char *, int);
    char buf[PATH_MAX];

    if (real == NULL)
    {
        real = (int (*)(const char *, int))shim_next("access");
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)), amode);
}

int chdir(const char *path)
{
    static int (*real)(const char *);
    char buf[PATH_MAX];

    if (real == NULL)
    {
        real = (int (*)(const char *))shim_next("chdir");
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)));
}

int mkdir(const char *path, mode_t mode)
{
    static int (*real)(const char *, mode_t);
    char buf[PATH_MAX];

    if (real == NULL)
    {
        real = (int (*)(const char *, mode_t))shim_next("mkdir");
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)), mode);
}

int rmdir(const char *path)
{
    static int (*real)(const char *);
    char buf[PATH_MAX];

    if (real == NULL)
    {
        real = (int (*)(const char *))shim_next("rmdir");
    }
    if (real == NULL)
    {
        errno = ENOSYS;
        return -1;
    }
    return real(shim_norm_path(path, buf, sizeof(buf)));
}

/* ---------------------------------------------------------------- */
/*  Turbo C helpers                                                 */
/* ---------------------------------------------------------------- */
long filelength(int handle)
{
    off_t cur, end;

    cur = lseek(handle, 0, SEEK_CUR);
    if (cur == (off_t)-1)
    {
        return -1L;
    }
    end = lseek(handle, 0, SEEK_END);
    if (end == (off_t)-1)
    {
        return -1L;
    }
    (void)lseek(handle, cur, SEEK_SET);
    return (long)end;
}

long tell(int handle)
{
    return (long)lseek(handle, 0, SEEK_CUR);
}

int eof(int handle)
{
    char c;
    ssize_t n;
    off_t pos;

    pos = lseek(handle, 0, SEEK_CUR);
    n = read(handle, &c, 1);
    (void)lseek(handle, pos, SEEK_SET);
    return (n <= 0) ? 1 : 0;
}
