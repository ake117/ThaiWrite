/* =====================================================================
 * shim_process.c - Turbo C <process.h> spawn*()/exec*() for the port
 * ---------------------------------------------------------------------
 * spawnl()/spawnv() and the p/e variants are implemented with fork()/
 * execv().  P_WAIT blocks and returns the child's exit status; P_NOWAIT
 * returns the child's pid; P_OVERLAY replaces the current process.
 *
 * Turbo C's callers (SRC/SCUW/CWSETUP.C) do:
 *
 *     if (spawnlp(P_WAIT, cup, cup, cup_option, filename, NULL) == -1)
 *         ... try cw_dir + "\\" + cup ...
 *
 * so a "program not found" result must be -1, not 127.  The lookup is
 * therefore done in the parent before forking, and -1 is returned when
 * nothing matches.
 *
 * Name resolution tries, in order:
 *   1. the name exactly as given (backslashes normalised to '/'),
 *   2. the same name with a trailing ".EXE"/".exe" stripped,
 *   3. the lower-cased form of either of those.
 * For the "p" (path-searching) variants each candidate is looked up on
 * $PATH first.  This lets the port install either literal TWPRINT.EXE
 * / TWFONT.EXE files or ordinary Unix names (twprint, twfont).
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "process.h"
#include "dir.h"            /* searchpath() */
#include "shim_internal.h"

#define SHIM_SPAWN_MAXARGS 64

/* Extension points for the port's terminal handling: set these to
 * leave/enter graphics mode around a spawned child. */
void (*shim_spawn_pre_hook)(void) = NULL;
void (*shim_spawn_post_hook)(void) = NULL;

/* ---------------------------------------------------------------- */
/*  Program lookup                                                  */
/* ---------------------------------------------------------------- */

static const char *shim_strip_exe(const char *name, char *buf, size_t buflen)
{
    size_t len = strlen(name);
    if (len > 4 &&
        (name[len - 4] == '.' ) &&
        (name[len - 3] == 'E' || name[len - 3] == 'e') &&
        (name[len - 2] == 'X' || name[len - 2] == 'x') &&
        (name[len - 1] == 'E' || name[len - 1] == 'e'))
    {
        size_t n = len - 4;
        if (n >= buflen)
            n = buflen - 1;
        memcpy(buf, name, n);
        buf[n] = '\0';
        return buf;
    }
    return name;
}

static void shim_lower(char *s)
{
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z')
            *s = (char)(*s - 'A' + 'a');
}

static int shim_try_exec(const char *name, char *resolved, size_t buflen,
                         int search)
{
    char norm[PATH_MAX];
    char alt[PATH_MAX];
    const char *cands[2];
    int i;

    shim_norm_path(name, norm, sizeof(norm));
    shim_norm_path(shim_strip_exe(name, alt, sizeof(alt)), alt, sizeof(alt));
    cands[0] = norm;
    cands[1] = alt;

    for (i = 0; i < 2; i++)
    {
        const char *c = cands[i];
        char lowered[PATH_MAX];

        if (c[0] == '\0')
            continue;

        if (search)
        {
            char *found = searchpath(c);
            if (found != NULL)
            {
                strncpy(resolved, found, buflen - 1);
                resolved[buflen - 1] = '\0';
                return 0;
            }
        }
        else if (access(c, X_OK) == 0)
        {
            strncpy(resolved, c, buflen - 1);
            resolved[buflen - 1] = '\0';
            return 0;
        }

        /* third candidate: lower case, for DOS names installed as
         * ordinary Unix files */
        if (c != alt)
        {
            strncpy(lowered, c, sizeof(lowered) - 1);
            lowered[sizeof(lowered) - 1] = '\0';
            shim_lower(lowered);
            if (search)
            {
                char *found = searchpath(lowered);
                if (found != NULL)
                {
                    strncpy(resolved, found, buflen - 1);
                    resolved[buflen - 1] = '\0';
                    return 0;
                }
            }
            else if (access(lowered, X_OK) == 0)
            {
                strncpy(resolved, lowered, buflen - 1);
                resolved[buflen - 1] = '\0';
                return 0;
            }
        }
    }
    return -1;
}

/* ---------------------------------------------------------------- */
/*  spawn                                                           */
/* ---------------------------------------------------------------- */

static char **shim_collect_args(const char *arg0, va_list ap)
{
    static char *argv[SHIM_SPAWN_MAXARGS];
    int n = 0;

    argv[n++] = (char *)(size_t)(const void *)arg0;
    while (n < SHIM_SPAWN_MAXARGS - 1)
    {
        char *a = va_arg(ap, char *);
        argv[n++] = a;
        if (a == NULL)
            break;
    }
    argv[SHIM_SPAWN_MAXARGS - 1] = NULL;
    return argv;
}

static int shim_spawn(int mode, const char *path, char *const argv[],
                      char *const envp[], int search)
{
    char resolved[PATH_MAX];
    pid_t pid;
    int status = 0;

    if (path == NULL)
    {
        errno = ENOENT;
        return -1;
    }
    if (shim_try_exec(path, resolved, sizeof(resolved), search) != 0)
    {
        errno = ENOENT;
        return -1;
    }

    if (shim_spawn_pre_hook != NULL)
        shim_spawn_pre_hook();

    if (mode == P_OVERLAY)
    {
        if (envp != NULL)
            execve(resolved, argv, envp);
        else
            execv(resolved, argv);
        /* Only reached when the exec failed. */
        if (shim_spawn_post_hook != NULL)
            shim_spawn_post_hook();
        _exit(127);
    }

    pid = fork();
    if (pid < 0)
    {
        if (shim_spawn_post_hook != NULL)
            shim_spawn_post_hook();
        return -1;
    }
    if (pid == 0)
    {
        if (envp != NULL)
            execve(resolved, argv, envp);
        else
            execv(resolved, argv);
        _exit(127);
    }

    if (mode == P_NOWAIT)
    {
        if (shim_spawn_post_hook != NULL)
            shim_spawn_post_hook();
        return (int)pid;
    }

    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno != EINTR)
        {
            if (shim_spawn_post_hook != NULL)
                shim_spawn_post_hook();
            return -1;
        }
    }
    if (shim_spawn_post_hook != NULL)
        shim_spawn_post_hook();

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

/* ---------------------------------------------------------------- */
/*  Public entry points                                             */
/* ---------------------------------------------------------------- */

int spawnl(int mode, const char *path, const char *arg0, ...)
{
    char **argv;
    va_list ap;
    int rc;

    va_start(ap, arg0);
    argv = shim_collect_args(arg0, ap);
    va_end(ap);
    rc = shim_spawn(mode, path, argv, NULL, 0);
    return rc;
}

int spawnle(int mode, const char *path, const char *arg0, ...)
{
    char **argv;
    va_list ap;
    int rc;
    char *envp[2];

    va_start(ap, arg0);
    argv = shim_collect_args(arg0, ap);
    va_end(ap);
    envp[0] = NULL;
    rc = shim_spawn(mode, path, argv, envp, 0);
    return rc;
}

int spawnlp(int mode, const char *path, const char *arg0, ...)
{
    char **argv;
    va_list ap;
    int rc;

    va_start(ap, arg0);
    argv = shim_collect_args(arg0, ap);
    va_end(ap);
    rc = shim_spawn(mode, path, argv, NULL, 1);
    return rc;
}

int spawnlpe(int mode, const char *path, const char *arg0, ...)
{
    char **argv;
    va_list ap;
    int rc;
    char *envp[2];

    va_start(ap, arg0);
    argv = shim_collect_args(arg0, ap);
    va_end(ap);
    envp[0] = NULL;
    rc = shim_spawn(mode, path, argv, envp, 1);
    return rc;
}

int spawnv(int mode, const char *path, char *const argv[])
{
    return shim_spawn(mode, path, argv, NULL, 0);
}

int spawnve(int mode, const char *path, char *const argv[], char *const envp[])
{
    return shim_spawn(mode, path, argv, envp, 0);
}

int spawnvp(int mode, const char *path, char *const argv[])
{
    return shim_spawn(mode, path, argv, NULL, 1);
}
