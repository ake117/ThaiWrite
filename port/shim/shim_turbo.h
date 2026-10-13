/* =====================================================================
 * shim_turbo.h - Turbo C 2.01 language/compiler compatibility layer
 * ---------------------------------------------------------------------
 * Part of the ThaiWrite (CU-Writer 1.41) Linux port shim.
 * Nothing under thaiwrite/ is modified; this header lets the original
 * Turbo C sources compile with gcc on a flat 64-bit Linux target.
 *
 * The original 16-bit memory model keywords are removed (they are
 * meaningless on a flat model).  The real work is done in dos.h /
 * bios.h / graphics.h, which emulate the DOS+BIOS services in
 * user space.
 * ===================================================================== */
#ifndef _SHIM_TURBO_H
#define _SHIM_TURBO_H

/* ---- 16-bit memory model qualifiers -> nothing --------------------- */
#ifndef far
#define far
#endif
#ifndef near
#define near
#endif
#ifndef huge
#define huge
#endif
#ifndef _seg
#define _seg
#endif
#ifndef _cdecl
#define _cdecl
#endif
#ifndef _pascal
#define _pascal
#endif
#ifndef _stdcall
#define _stdcall
#endif
#ifndef _fastcall
#define _fastcall
#endif
#ifndef _export
#define _export
#endif
#ifndef _loadds
#define _loadds
#endif
#ifndef _saversegs
#define _saversegs
#endif
#ifndef _regparm
#define _regparm
#endif

/* "void interrupt far foo(void)" -> "void foo(void)".
 * Set SHIM_NO_INTERRUPT_MACRO before including a shim header to keep
 * the identifier "interrupt" intact (it is only used as a qualifier in
 * SRC/SCUF/INT.C). */
#ifndef SHIM_NO_INTERRUPT_MACRO
#ifndef interrupt
#define interrupt
#endif
#endif

/* ---- 16-bit "int" ---------------------------------------------------
 * The original code assumes unsigned int == 16 bit inside the register
 * unions, so those use unsigned short explicitly (see dos.h).  Where a
 * plain "int" is used it is safe: no DOS call in CU-Writer stores more
 * than 16 significant bits in an int. */

/* ---- far-pointer helpers ------------------------------------------- */
/* In Turbo C a "far pointer" is a 32-bit seg:off pair and these macros
 * split/join it.  On a flat model the pointer value itself is the
 * address, so SEG is always 0 and OFF is the address. */
#define FP_SEG(fp)      ((unsigned)(0))
#define FP_OFF(fp)      ((unsigned)(unsigned long)(fp))
#define MK_FP(seg, off) ((void *)(unsigned long)(((unsigned long)(seg) << 16) | ((unsigned)(off))))
#define _FP_SEG(fp)     FP_SEG(fp)
#define _FP_OFF(fp)     FP_OFF(fp)
#define _MK_FP(s, o)    MK_FP((s), (o))

/* ---- pseudo register names (_AX ...) --------------------------------
 * Turbo C exposes the "current" register values through these macros.
 * The shim keeps a shadow copy in shim_dos.c. */
extern unsigned short _shim_AX, _shim_BX, _shim_CX, _shim_DX;
extern unsigned short _shim_SI, _shim_DI, _shim_ES, _shim_DS;
#define _AX  _shim_AX
#define _BX  _shim_BX
#define _CX  _shim_CX
#define _DX  _shim_DX
#define _SI  _shim_SI
#define _DI  _shim_DI
#define _ES  _shim_ES
#define _DS  _shim_DS

/* ---- misc Turbo C spellings ---------------------------------------- */
#ifndef _far
#define _far
#endif
#ifndef _near
#define _near
#endif
#ifndef _huge
#define _huge
#endif

#endif /* _SHIM_TURBO_H */
