/* =====================================================================
 * process.h - Turbo C <process.h> replacement for the ThaiWrite port
 * ---------------------------------------------------------------------
 * The spawn*() family is implemented in shim_process.c with fork()/
 * execvp().  P_WAIT blocks until the child exits; the return value is
 * the child's exit status, or -1 when the program could not be run
 * (matching Turbo C, whose callers test "== -1").
 *
 * The original sources spawn "TWPRINT.EXE" and "TWFONT.EXE".  shim_spawn
 * first looks for that literal name on PATH, then for the same name with
 * the ".EXE" suffix stripped, so the port can install either real files
 * named TWPRINT.EXE / TWFONT.EXE or ordinary Unix names (twprint, ...).
 * ===================================================================== */
#ifndef _SHIM_PROCESS_H
#define _SHIM_PROCESS_H

#include "shim_turbo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Turbo C's P_* constants */
#define P_WAIT   0
#define P_NOWAIT 1
#define P_OVERLAY 2

int spawnl(int mode, const char *path, const char *arg0, ...);
int spawnle(int mode, const char *path, const char *arg0, ...);
int spawnlp(int mode, const char *path, const char *arg0, ...);
int spawnlpe(int mode, const char *path, const char *arg0, ...);
int spawnv(int mode, const char *path, char *const argv[]);
int spawnve(int mode, const char *path, char *const argv[], char *const envp[]);
int spawnvp(int mode, const char *path, char *const argv[]);

int execl(const char *path, const char *arg0, ...);
int execlp(const char *path, const char *arg0, ...);
int execv(const char *path, char *const argv[]);
int execvp(const char *path, char *const argv[]);

/* Extension points for the port's terminal handling.  Set these to
 * leave/enter graphics mode around a spawned child (the DOS original
 * called settext() before spawnl() and setgraph() after, from the
 * caller).  Both default to NULL. */
extern void (*shim_spawn_pre_hook)(void);
extern void (*shim_spawn_post_hook)(void);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_PROCESS_H */
