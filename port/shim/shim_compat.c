/* =====================================================================
 * shim_compat.c - Turbo C library compatibility helpers
 * ---------------------------------------------------------------------
 * Things Turbo C had and glibc does not:
 *   strupr/strlwr/strrev/stricmp/strcmpi/strnicmp/strncmpi
 *   itoa/ltoa/ultoa
 *   movmem/setmem
 *   coreleft/freemem/brk/farmalloc/...   (mapped onto the normal heap)
 *   sys_errlist[]                        (built from strerror)
 * plus the shim's own path normaliser.
 * ===================================================================== */
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

#include "shim_internal.h"
#include "dos.h"

/* ---------------------------------------------------------------- */
/*  Path normalisation                                              */
/* ---------------------------------------------------------------- */
const char *shim_norm_path(const char *path, char *buf, size_t buflen)
{
    size_t i = 0;

    if (buf == NULL || buflen == 0)
    {
        return "";
    }
    if (path == NULL)
    {
        buf[0] = '\0';
        return buf;
    }
    while (path[i] != '\0' && i + 1 < buflen)
    {
        buf[i] = (path[i] == '\\') ? '/' : path[i];
        i++;
    }
    buf[i] = '\0';
    return buf;
}

const char *shim_norm_static(const char *path)
{
    static char buf[PATH_MAX * 2];
    return shim_norm_path(path, buf, sizeof(buf));
}

/* ---------------------------------------------------------------- */
/*  Case conversion (Turbo C spelling)                              */
/* ---------------------------------------------------------------- */
char *strupr(char *s)
{
    char *p = s;
    while (*p)
    {
        *p = (char)toupper((unsigned char)*p);
        p++;
    }
    return s;
}

char *strlwr(char *s)
{
    char *p = s;
    while (*p)
    {
        *p = (char)tolower((unsigned char)*p);
        p++;
    }
    return s;
}

char *strrev(char *s)
{
    char *a, *b;

    if (s == NULL)
    {
        return s;
    }
    a = s;
    b = s + strlen(s);
    if (b == s)
    {
        return s;
    }
    b--;
    while (a < b)
    {
        char c = *a;
        *a++ = *b;
        *b-- = c;
    }
    return s;
}

int stricmp(const char *s1, const char *s2)
{
    return strcasecmp(s1, s2);
}

int strcmpi(const char *s1, const char *s2)
{
    return strcasecmp(s1, s2);
}

int strnicmp(const char *s1, const char *s2, size_t n)
{
    return strncasecmp(s1, s2, n);
}

int strncmpi(const char *s1, const char *s2, size_t n)
{
    return strncasecmp(s1, s2, n);
}

/* ---------------------------------------------------------------- */
/*  Integer -> string (Turbo C spelling)                            */
/* ---------------------------------------------------------------- */
static char *shim_uitoa(unsigned long value, char *string, int radix, int neg)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char tmp[70];
    int i = 0, j = 0;

    if (radix < 2 || radix > 36)
    {
        string[0] = '\0';
        return string;
    }
    do
    {
        tmp[i++] = digits[value % (unsigned long)radix];
        value /= (unsigned long)radix;
    } while (value != 0 && i < (int)sizeof(tmp) - 1);

    if (neg)
    {
        tmp[i++] = '-';
    }
    while (i > 0)
    {
        string[j++] = tmp[--i];
    }
    string[j] = '\0';
    return string;
}

char *itoa(int value, char *string, int radix)
{
    if (radix == 10 && value < 0)
    {
        return shim_uitoa((unsigned long)(-(long)value), string, radix, 1);
    }
    return shim_uitoa((unsigned long)(unsigned int)value, string, radix, 0);
}

char *ltoa(long value, char *string, int radix)
{
    if (radix == 10 && value < 0)
    {
        return shim_uitoa((unsigned long)(-value), string, radix, 1);
    }
    return shim_uitoa((unsigned long)value, string, radix, 0);
}

char *ultoa(unsigned long value, char *string, int radix)
{
    return shim_uitoa(value, string, radix, 0);
}

/* ---------------------------------------------------------------- */
/*  mem.h helpers                                                   */
/* ---------------------------------------------------------------- */
void movmem(void *src, void *dst, unsigned len)
{
    memmove(dst, src, (size_t)len);
}

void setmem(void *dst, unsigned len, char value)
{
    memset(dst, value, (size_t)len);
}

/* ---------------------------------------------------------------- */
/*  Heap helpers - Turbo C's "coreleft" and far-heap spellings      */
/* ---------------------------------------------------------------- */
unsigned int coreleft(void)
{
    /* There is no DOS heap here; report a large constant so callers that
     * use it only as a sanity check keep working. */
    return 0xFFFF;
}

unsigned int freemem(void)
{
    return coreleft();
}

unsigned long farcoreleft(void)
{
    /* Turbo C returned the free far-heap bytes.  The port has a single
     * flat heap, so report a large constant for the same reason as
     * coreleft(): callers only sanity-check it. */
    return 0xFFFFuL;
}

void *farmalloc(unsigned long size)
{
    return malloc((size_t)size);
}

void farfree(void *block)
{
    free(block);
}

void *farcalloc(unsigned long units, unsigned long unitsz)
{
    return calloc((size_t)units, (size_t)unitsz);
}

void *farrealloc(void *block, unsigned long size)
{
    return realloc(block, (size_t)size);
}

int _heapchk(void)
{
    return 0;
}

int _heapset(unsigned int fill)
{
    (void)fill;
    return 0;
}

int _heapwalk(void *entry)
{
    (void)entry;
    return 0;
}

/* ---------------------------------------------------------------- */
/*  sys_errlist[] - SRC/COMMON/CSCRN.C prints sys_errlist[errno]    */
/* ---------------------------------------------------------------- */
char *shim_sys_errlist[256];
int   shim_sys_nerr = 256;

static char shim_errlist_unknown[] = "Unknown error";

__attribute__((constructor))
static void shim_errlist_init(void)
{
    int i;

    for (i = 0; i < 256; i++)
    {
        const char *msg = strerror(i);

        if (msg == NULL || strncmp(msg, "Unknown error", 13) == 0)
        {
            shim_sys_errlist[i] = shim_errlist_unknown;
        }
        else
        {
            shim_sys_errlist[i] = (char *)msg;
        }
    }
}

/* ---------------------------------------------------------------- */
/*  Pseudo-register shadow values (_AX ... in shim_turbo.h)         */
/* ---------------------------------------------------------------- */
unsigned short _shim_AX, _shim_BX, _shim_CX, _shim_DX;
unsigned short _shim_SI, _shim_DI, _shim_ES, _shim_DS;
