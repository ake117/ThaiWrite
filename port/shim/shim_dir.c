/* =====================================================================
 * shim_dir.c - Turbo C <dir.h> replacement for the ThaiWrite port
 * ---------------------------------------------------------------------
 * findfirst/findnext are implemented on opendir/readdir with DOS wildcard
 * and attribute semantics.  fnsplit/fnmerge are exact inverses and keep
 * the separator style found in the input.
 *
 * getdfree() is filled from statvfs().  SRC/SCUW/BLOCK.C declares
 * "struct dfree *dfreep;" without ever assigning it (harmless on DOS,
 * where address 0 is real memory); a constructor here points it at a
 * shim-owned struct so the port does not fault - see README.
 * ===================================================================== */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <limits.h>
#include <errno.h>

#include "shim_turbo.h"
#include "dir.h"
#include "shim_internal.h"

/* ---------------------------------------------------------------- */
/*  findfirst / findnext                                            */
/* ---------------------------------------------------------------- */
struct shim_find_state
{
    DIR *dir;
    char pattern[PATH_MAX];
    char dirpath[PATH_MAX];
    int  dir_only;
    int  file_only;
    int  next_index;            /* 0 -> ".", 1 -> "..", >=2 -> readdir */
};

static struct shim_find_state shim_find;

/* DOS's ff_name is only 13 bytes, so longer Unix names come back
 * truncated.  The port's own code can recover the real name from here
 * (valid until the next findfirst()/findnext()). */
static char shim_find_fullname_buf[PATH_MAX];

const char *shim_find_fullname(void)
{
    return shim_find_fullname_buf;
}

static void shim_find_close(void)
{
    if (shim_find.dir != NULL)
    {
        closedir(shim_find.dir);
        shim_find.dir = NULL;
    }
}

/* DOS wildcard match, case insensitive.  "*.*" matches every name, as
 * it does on DOS. */
static int shim_dos_match(const char *pattern, const char *name)
{
    if (strcmp(pattern, "*.*") == 0)
    {
        return 1;
    }
    return fnmatch(pattern, name, FNM_CASEFOLD) == 0;
}

static void shim_fill_ffblk(struct ffblk *ff, const char *name, const char *fullpath)
{
    struct stat st;

    memset(ff, 0, sizeof(*ff));
    strncpy(ff->ff_name, name, 12);
    ff->ff_name[12] = '\0';

    if (stat(fullpath, &st) == 0)
    {
        ff->ff_fsize = (unsigned long)st.st_size;
        if (S_ISDIR(st.st_mode))
        {
            ff->ff_attrib = FA_DIREC;
        }
        else
        {
            ff->ff_attrib = FA_NORMAL;
        }
    }
    else
    {
        ff->ff_attrib = FA_NORMAL;
        ff->ff_fsize = 0;
    }
}

/* Emit the next matching entry into ffblk.  Returns 0 on success. */
static int shim_find_next_entry(struct ffblk *ffblk)
{
    struct shim_find_state *s = &shim_find;

    while (1)
    {
        const char *name;
        char full[PATH_MAX];
        struct stat st;
        int is_dir;

        if (s->next_index == 0)
        {
            name = ".";
            s->next_index = 1;
        }
        else if (s->next_index == 1)
        {
            name = "..";
            s->next_index = 2;
        }
        else
        {
            struct dirent *de = readdir(s->dir);

            if (de == NULL)
            {
                return -1;
            }
            name = de->d_name;
        }

        snprintf(full, sizeof(full), "%s/%s", s->dirpath, name);
        if (stat(full, &st) != 0)
        {
            continue;
        }
        is_dir = S_ISDIR(st.st_mode);

        if (s->dir_only)
        {
            /* "." and ".." are always offered for FA_DIREC, as on DOS */
            if (!is_dir)
            {
                continue;
            }
            if (s->next_index > 2 && !shim_dos_match(s->pattern, name))
            {
                continue;
            }
        }
        else if (s->file_only)
        {
            if (is_dir)
            {
                continue;
            }
            if (!shim_dos_match(s->pattern, name))
            {
                continue;
            }
        }
        else
        {
            if (!shim_dos_match(s->pattern, name))
            {
                continue;
            }
        }

        strncpy(shim_find_fullname_buf, name, sizeof(shim_find_fullname_buf) - 1);
        shim_find_fullname_buf[sizeof(shim_find_fullname_buf) - 1] = '\0';
        shim_fill_ffblk(ffblk, name, full);
        return 0;
    }
}

int findfirst(const char *pathname, struct ffblk *ffblk, int attrib)
{
    char norm[PATH_MAX];
    const char *p;
    const char *slash;

    if (pathname == NULL || ffblk == NULL)
    {
        return -1;
    }

    shim_find_close();

    shim_norm_path(pathname, norm, sizeof(norm));
    p = norm;

    /* skip a DOS drive letter, e.g. "C:\..." -> "\..." */
    if (isalpha((unsigned char)p[0]) && p[1] == ':')
    {
        p += 2;
    }

    slash = strrchr(p, '/');
    if (slash == NULL)
    {
        snprintf(shim_find.dirpath, sizeof(shim_find.dirpath), ".");
        snprintf(shim_find.pattern, sizeof(shim_find.pattern), "%s", p);
    }
    else
    {
        size_t dirlen = (size_t)(slash - p);

        if (dirlen == 0)
        {
            snprintf(shim_find.dirpath, sizeof(shim_find.dirpath), "/");
        }
        else
        {
            if (dirlen >= sizeof(shim_find.dirpath))
            {
                dirlen = sizeof(shim_find.dirpath) - 1;
            }
            memcpy(shim_find.dirpath, p, dirlen);
            shim_find.dirpath[dirlen] = '\0';
        }
        snprintf(shim_find.pattern, sizeof(shim_find.pattern), "%s", slash + 1);
    }

    if (shim_find.pattern[0] == '\0')
    {
        snprintf(shim_find.pattern, sizeof(shim_find.pattern), "*.*");
    }

    shim_find.dir_only  = (attrib & FA_DIREC) != 0;
    shim_find.file_only = !shim_find.dir_only;
    shim_find.next_index = 0;

    shim_find.dir = opendir(shim_find.dirpath);
    if (shim_find.dir == NULL)
    {
        return -1;
    }

    return shim_find_next_entry(ffblk);
}

int findnext(struct ffblk *ffblk)
{
    if (shim_find.dir == NULL)
    {
        return -1;
    }
    return shim_find_next_entry(ffblk);
}

/* ---------------------------------------------------------------- */
/*  fnsplit / fnmerge                                               */
/* ---------------------------------------------------------------- */
int fnsplit(const char *path, char *drive, char *dir, char *name, char *ext)
{
    const char *p = path;
    const char *lastslash = NULL;
    const char *lastdot = NULL;
    const char *base;
    const char *q;
    int flags = 0;
    size_t n;

    if (path == NULL)
    {
        return 0;
    }

    if (drive != NULL)
    {
        drive[0] = '\0';
    }
    if (dir != NULL)
    {
        dir[0] = '\0';
    }
    if (name != NULL)
    {
        name[0] = '\0';
    }
    if (ext != NULL)
    {
        ext[0] = '\0';
    }

    /* drive letter */
    if (isalpha((unsigned char)p[0]) && p[1] == ':')
    {
        if (drive != NULL)
        {
            drive[0] = p[0];
            drive[1] = ':';
            drive[2] = '\0';
        }
        flags |= DRIVE;
        p += 2;
    }

    /* directory: up to and including the last separator */
    for (q = p; *q != '\0'; q++)
    {
        if (*q == '/' || *q == '\\')
        {
            lastslash = q;
        }
    }
    base = p;
    if (lastslash != NULL)
    {
        n = (size_t)(lastslash - p) + 1;
        if (dir != NULL)
        {
            if (n > (size_t)(MAXDIR - 1))
            {
                n = MAXDIR - 1;
            }
            memcpy(dir, p, n);
            dir[n] = '\0';
        }
        flags |= DIRECTORY;
        base = lastslash + 1;
    }

    /* extension: last '.' in the base name, not counting a leading '.' */
    for (q = base; *q != '\0'; q++)
    {
        if (*q == '.')
        {
            lastdot = q;
        }
    }
    if (lastdot != NULL && lastdot != base)
    {
        n = strlen(lastdot);
        if (ext != NULL)
        {
            if (n > (size_t)(MAXEXT - 1))
            {
                n = MAXEXT - 1;
            }
            memcpy(ext, lastdot, n);
            ext[n] = '\0';
        }
        flags |= EXTENSION;
    }
    else
    {
        lastdot = base + strlen(base);
    }

    /* name */
    n = (size_t)(lastdot - base);
    if (name != NULL)
    {
        if (n > (size_t)(MAXFILE - 1))
        {
            n = MAXFILE - 1;
        }
        memcpy(name, base, n);
        name[n] = '\0';
    }
    if (n > 0)
    {
        flags |= FILENAME;
    }

    return flags;
}

void fnmerge(char *path, const char *drive, const char *dir,
             const char *name, const char *ext)
{
    size_t len = 0;

    path[0] = '\0';
    if (drive != NULL)
    {
        strcat(path, drive);
        len = strlen(path);
    }
    if (dir != NULL)
    {
        strcat(path, dir);
        len = strlen(path);
    }
    if (name != NULL)
    {
        strcat(path, name);
    }
    if (ext != NULL)
    {
        strcat(path, ext);
    }
    (void)len;
}

/* ---------------------------------------------------------------- */
/*  Drive letters - a single pseudo drive                           */
/* ---------------------------------------------------------------- */
int getdisk(void)
{
    return 0;                   /* "A:" */
}

int setdisk(int drive)
{
    if (drive == 0)
    {
        return 0;
    }
    return -1;                  /* only one drive exists */
}

int getcurdir(int drive, char *direc)
{
    char cwd[PATH_MAX];

    (void)drive;
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        if (direc != NULL)
        {
            direc[0] = '\0';
        }
        return -1;
    }
    /* Turbo C returned the directory without the drive prefix, and an
     * empty string for the root. */
    if (strcmp(cwd, "/") == 0)
    {
        if (direc != NULL)
        {
            direc[0] = '\0';
        }
        return 0;
    }
    if (direc != NULL)
    {
        strncpy(direc, cwd + 1, MAXDIR - 1);
        direc[MAXDIR - 1] = '\0';
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/*  searchpath - look for "file" on PATH                            */
/* ---------------------------------------------------------------- */
char *searchpath(const char *file)
{
    static char result[PATH_MAX];
    const char *path = getenv("PATH");
    char *dup, *tok;

    if (file == NULL)
    {
        return NULL;
    }
    if (strchr(file, '/') != NULL)
    {
        if (access(file, F_OK) == 0)
        {
            strncpy(result, file, sizeof(result) - 1);
            result[sizeof(result) - 1] = '\0';
            return result;
        }
        return NULL;
    }
    if (path == NULL)
    {
        return NULL;
    }
    dup = strdup(path);
    if (dup == NULL)
    {
        return NULL;
    }
    for (tok = strtok(dup, ":"); tok != NULL; tok = strtok(NULL, ":"))
    {
        snprintf(result, sizeof(result), "%s/%s", tok, file);
        if (access(result, F_OK) == 0)
        {
            free(dup);
            return result;
        }
    }
    free(dup);
    return NULL;
}

/* ---------------------------------------------------------------- */
/*  getdfree - free space on a "drive"                              */
/* ---------------------------------------------------------------- */
struct shim_dfree_holder
{
    struct dfree d;
};
static struct shim_dfree_holder shim_df_holder;

void getdfree(unsigned char drive, struct dfree *dfreep)
{
    struct statvfs vfs;
    const char *path = ".";

    (void)drive;
    if (dfreep == NULL)
    {
        return;
    }
    if (statvfs(path, &vfs) != 0)
    {
        dfreep->df_avail = 0;
        dfreep->df_total = 0;
        dfreep->df_bsec = 512;
        dfreep->df_sclus = 1;
        return;
    }
    dfreep->df_bsec = (unsigned)vfs.f_bsize;
    dfreep->df_sclus = (unsigned)vfs.f_frsize;
    dfreep->df_avail = (unsigned)((vfs.f_bavail * vfs.f_frsize) / (vfs.f_bsize ? vfs.f_bsize : 1));
    dfreep->df_total = (unsigned)((vfs.f_blocks * vfs.f_frsize) / (vfs.f_bsize ? vfs.f_bsize : 1));
}

/* SRC/SCUW/BLOCK.C leaves "struct dfree *dfreep;" uninitialised.  Point
 * it at the shim's struct before main() runs.  Weak so that programs
 * which do not link BLOCK.C still link. */
extern struct dfree *dfreep __attribute__((weak));

__attribute__((constructor))
static void shim_init_dfreep(void)
{
    struct dfree **slot = &dfreep;

    if (slot != NULL)
    {
        *slot = &shim_df_holder.d;
    }
}
