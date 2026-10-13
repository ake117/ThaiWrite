/* =====================================================================
 * bios.h - Turbo C <bios.h> replacement for the ThaiWrite Linux port
 * ---------------------------------------------------------------------
 * bioskey / biosprint / biostime are emulated in shim_bios.c:
 *   bioskey(0)  blocking key read   -> terminal key translated to a
 *                                     DOS BIOS key code (scan<<8 | ascii)
 *   bioskey(1)  key available?      -> 1 when a key is pending, else 0
 *   bioskey(2)  shift status flags  -> 0 (no modifier tracking)
 *   biosprint   printer write/status
 *   biostime    BIOS tick counter (18.2 Hz since midnight)
 * ===================================================================== */
#ifndef _SHIM_BIOS_H
#define _SHIM_BIOS_H

#include "shim_turbo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* biosprint() commands */
#define PRINTER_WRITE   0
#define PRINTER_STATUS  2

int  bioskey(int cmd);
int  biosprint(int cmd, int byte, int port);
long biostime(int cmd, long newtime);

/* Provided by the shim so SRC/COMMON/EKBD.C links even when the
 * original ASM display drivers (which used to define it) are dropped. */
int  asm_keypressed(void);

/* Raw key pump.  Reads whatever is pending on stdin and converts it to
 * DOS key codes in the shim's key queue.  Called by bioskey(1). */
void shim_key_pump(void);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_BIOS_H */
