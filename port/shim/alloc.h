/* =====================================================================
 * alloc.h - Turbo C <alloc.h> replacement for the ThaiWrite Linux port
 * ---------------------------------------------------------------------
 * malloc/calloc/realloc/free come from <stdlib.h>.  The Turbo C-only
 * far-heap and heap-inspection entry points are provided as small
 * compatibility stubs in shim_compat.c (the port uses the normal heap).
 * ===================================================================== */
#ifndef _SHIM_ALLOC_H
#define _SHIM_ALLOC_H

#include "shim_turbo.h"
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned int coreleft(void);
unsigned int freemem(void);
unsigned long farcoreleft(void);   /* Turbo C: free far-heap bytes (here: free heap) */

/* brk()/sbrk() come from <unistd.h>; nothing else is needed here. */

/* far heap: mapped onto the normal heap */
void *farmalloc(unsigned long size);
void  farfree(void *block);
void *farcalloc(unsigned long units, unsigned long unitsz);
void *farrealloc(void *block, unsigned long size);

/* Turbo C memory-model heap controls */
int _heapchk(void);
int _heapset(unsigned int fill);
int _heapwalk(void *entry);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_ALLOC_H */
