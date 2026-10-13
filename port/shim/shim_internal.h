/* shim_internal.h - helpers shared between the shim implementation files.
 * Not intended to be included by the ported sources. */
#ifndef _SHIM_INTERNAL_H
#define _SHIM_INTERNAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Copy "path" into "buf" (up to buflen bytes, always NUL terminated)
 * replacing DOS '\' separators with '/'.  Returns buf.  A NULL path or
 * a zero buflen yields "". */
const char *shim_norm_path(const char *path, char *buf, size_t buflen);

/* Same, but returns a pointer to a thread-unsafe static buffer. */
const char *shim_norm_static(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_INTERNAL_H */
