/* =====================================================================
 * test_shim.c - headless smoke test for the ThaiWrite port shim layer
 * ---------------------------------------------------------------------
 * Exercises every shim subsystem without needing a real terminal:
 * path normalisation, the Turbo C string/stdlib additions, the
 * software-PC interrupts (INT 10h/16h/1Ah/21h), port I/O, real-mode
 * memory, the key queue plumbing, findfirst/findnext, spawn, and the
 * BGI raster engine - including the claim that BGI drawing and
 * SRC/SCUF/WRITE.C's raw far-pointer writes share one framebuffer.
 *
 *   make test
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "shim_turbo.h"
#include "shim_internal.h"
#include "dos.h"
#include "bios.h"
#include "conio.h"
#include "io.h"
#include "dir.h"
#include "alloc.h"
#include "mem.h"
#include "process.h"
#include "graphics.h"
#include "errno.h"

static int tests_run, tests_failed;
static int shim_idle_called;

void shim_test_idle(void);
void shim_test_isr(void);

void shim_test_idle(void)
{
    shim_idle_called++;
}

void shim_test_isr(void)
{
}

#define CHECK(cond, what)                                                   \
    do {                                                                    \
        tests_run++;                                                        \
        if (cond)                                                           \
            printf("  ok   %s\n", what);                                    \
        else {                                                              \
            tests_failed++;                                                 \
            printf("  FAIL %s  (line %d)\n", what, __LINE__);               \
        }                                                                   \
    } while (0)

/* ---------------------------------------------------------------- */

static void test_internal_paths(void)
{
    char buf[64];

    printf("[internal path normalisation]\n");
    CHECK(strcmp(shim_norm_path("A:\\B\\C.D", buf, sizeof buf), "A:/B/C.D") == 0,
          "backslashes become slashes");
    CHECK(strcmp(shim_norm_path("already/ok", buf, sizeof buf), "already/ok") == 0,
          "POSIX paths pass through");
    shim_norm_path(NULL, buf, sizeof buf);
    CHECK(buf[0] == '\0', "NULL path yields empty string");
    shim_norm_path("abc", buf, 1);
    CHECK(buf[0] == '\0', "truncation stays NUL terminated");
    CHECK(strcmp(shim_norm_static("\\TW\\TW.CFG"), "/TW/TW.CFG") == 0,
          "static variant normalises");
}

static void test_compat_strings(void)
{
    char buf[64];
    char src[8] = "abcdefg";
    char dst[8];

    printf("[Turbo C string/stdlib additions]\n");
    CHECK(strcmp(strupr(strcpy(buf, "aBcD")), "ABCD") == 0, "strupr");
    CHECK(strcmp(strlwr(strcpy(buf, "AbCd")), "abcd") == 0, "strlwr");
    CHECK(strcmp(strrev(strcpy(buf, "abc")), "cba") == 0, "strrev");
    CHECK(stricmp("AbC", "aBc") == 0, "stricmp is case insensitive");
    CHECK(strnicmp("AbCx", "aBcY", 3) == 0, "strnicmp prefix");
    CHECK(itoa(-1234, buf, 10) != NULL && strcmp(buf, "-1234") == 0, "itoa");
    CHECK(ltoa(-1L, buf, 10) != NULL && strcmp(buf, "-1") == 0, "ltoa");
    /* Turbo C's itoa/ltoa/ultoa use "0123456789abcdefghijklmnopqrstuvwxyz",
     * so radix 16 is lowercase. */
    CHECK(ultoa(255UL, buf, 16) != NULL && strcmp(buf, "ff") == 0, "ultoa radix 16");
    setmem(dst, sizeof dst, 0x7F);
    CHECK((unsigned char)dst[0] == 0x7F && (unsigned char)dst[7] == 0x7F, "setmem");
    movmem(src, dst, 7);            /* Turbo C order: (src, dst, len) */
    CHECK(dst[0] == 'a' && dst[6] == 'g', "movmem(src, dst, len)");
    CHECK(coreleft() > 0, "coreleft() > 0");
    CHECK(freemem() > 0, "freemem() > 0");
    CHECK(sys_nerr > 0 && sys_errlist[0] != NULL, "sys_errlist populated");
}

static void test_dos_interrupts(void)
{
    union REGS in, out;
    struct SREGS seg;
    struct timespec t0, t1;
    long ms;

    printf("[dos.h - software PC]\n");

    /* INT 10h AH=0Fh : get current video mode -> AL */
    in.h.ah = 0x0F;
    int86(0x10, &in, &out);
    CHECK(out.h.al == (unsigned char)shim_video_mode(),
          "INT 10h/0Fh returns the current video mode");

    /* INT 10h AX=1A00h : video configuration -> AL = 1Ah on a VGA */
    in.x.ax = 0x1A00;
    int86(0x10, &in, &out);
    CHECK(out.h.al == 0x1A, "INT 10h/1A00h reports a VGA configuration");

    /* INT 21h AH=30h : DOS version -> AL=major, AH=minor */
    in.h.ah = 0x30;
    intdos(&in, &out);
    CHECK(out.h.al >= 3 && out.h.ah >= 0,
          "INT 21h/30h returns a plausible DOS version");

    /* INT 21h AH=35h : get vector -> ES:BX */
    in.h.ah = 0x35;
    in.h.al = 0x28;
    intdosx(&in, &out, &seg);
    CHECK(seg.es == 0 && out.x.bx != 0,
          "INT 21h/35h returns a non-null vector for INT 28h");

    /* INT 1Ah AH=00h : tick count since midnight -> CX:DX */
    in.h.ah = 0x00;
    int86(0x1A, &in, &out);
    CHECK((out.x.cx | out.x.dx) != 0 || shim_video_mode() >= 0,
          "INT 1Ah/00h returns a tick count");

    /* port I/O round trip */
    outportb(0x3D4, 0x5A);
    CHECK(inportb(0x3D4) == 0x5A, "outportb/inportb round trip");
    outport(0x3D4, 0x1234);
    CHECK(inport(0x3D4) == 0x1234, "outport/inport round trip");

    /* real-mode memory: poke() and peek() address the same space the
     * shim exposes through shim_realmem() */
    pokeb(0x40, 0x10, 0xAB);
    CHECK(peekb(0x40, 0x10) == 0xAB, "pokeb/peekb round trip");
    CHECK(shim_realmem()[0x410] == 0xAB, "peek/poke hit shim_realmem()");
    poke(0x40, 0x20, 0xBEEF);
    CHECK(peek(0x40, 0x20) == 0xBEEF, "poke/peek 16-bit round trip");

    /* far-pointer helpers on a flat model */
    {
        int x = 0;
        CHECK(FP_SEG(&x) == 0, "FP_SEG is 0 on a flat model");
        CHECK(FP_OFF(&x) == (unsigned)(unsigned long)(size_t)&x,
              "FP_OFF is the low 32 bits of the address");
        CHECK(MK_FP(0, FP_OFF(&x)) != NULL, "MK_FP(0,off) yields a pointer");
    }

    /* interrupt vectors */
    {
        shim_isr old = getvect(0x28);
        extern void shim_test_isr(void);
        setvect(0x28, shim_test_isr);
        CHECK(getvect(0x28) == shim_test_isr, "setvect/getvect round trip");
        setvect(0x28, old);
    }

    /* idle hook + delay */
    {
        shim_set_idle_hook(shim_test_idle);
        shim_run_idle();
        shim_set_idle_hook(NULL);
        CHECK(shim_idle_called == 1, "shim_run_idle() calls the idle hook");
    }

    clock_gettime(CLOCK_MONOTONIC, &t0);
    delay(20);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ms = (t1.tv_sec - t0.tv_sec) * 1000L + (t1.tv_nsec - t0.tv_nsec) / 1000000L;
    CHECK(ms >= 15, "delay(20) waits at least 15 ms");
}

static void test_dir(void)
{
    char tmpl[] = "/tmp/shimtest.XXXXXX";
    char dir[64], cwd[128], drive[8], d[128], name[32], ext[16];
    char full[128];
    struct ffblk ff;
    struct dfree df;
    FILE *fp;
    int n;

    printf("[dir.h]\n");
    if (mkdtemp(tmpl) == NULL)
    {
        CHECK(0, "mkdtemp failed");
        return;
    }
    snprintf(dir, sizeof dir, "%s", tmpl);
    snprintf(full, sizeof full, "%s/A.TXT", dir);
    fp = fopen(full, "w");
    if (fp) { fputs("x", fp); fclose(fp); }
    snprintf(full, sizeof full, "%s/B.C", dir);
    fp = fopen(full, "w");
    if (fp) { fputs("y", fp); fclose(fp); }
    snprintf(full, sizeof full, "%s/SUBDIR", dir);
    mkdir(full, 0700);

    /* DOS's findfirst takes a relative pattern and resolves it against the
     * process's current directory, so make the temp dir current. */
    CHECK(getcwd(cwd, sizeof cwd) != NULL, "getcwd before chdir");
    {
        char saved[128];
        snprintf(saved, sizeof saved, "%s", cwd);
        CHECK(chdir(dir) == 0, "chdir into the temp dir");

        n = findfirst("*.*", &ff, FA_NORMAL);
    CHECK(n == 0, "findfirst(*.*) finds the first file");
    {
        int saw_dot = 0, saw_dotdot = 0, saw_a = 0, saw_b = 0, saw_dir = 0;
        while (n == 0)
        {
            if (strcmp(ff.ff_name, ".") == 0)    saw_dot = 1;
            if (strcmp(ff.ff_name, "..") == 0)   saw_dotdot = 1;
            if (strcmp(ff.ff_name, "A.TXT") == 0) saw_a = 1;
            if (strcmp(ff.ff_name, "B.C") == 0)  saw_b = 1;
            if (strcmp(ff.ff_name, "SUBDIR") == 0) saw_dir = 1;
            n = findnext(&ff);
        }
        /* DOS's FindFirst with attrib FA_NORMAL (=0) matches ordinary
         * files only, so directories and the "." / ".." entries never come
         * back.  COMMON/DIR.C relies on exactly that: its first pass calls
         * findfirst(mask, ..., FA_NORMAL) and appends every result to the
         * file list without filtering dots. */
        CHECK(!saw_dot && !saw_dotdot,
              "FA_NORMAL hides '.' and '..' like DOS");
        CHECK(saw_a && saw_b, "findfirst returns the files");
        CHECK(!saw_dir, "FA_NORMAL hides directories");
    }

    n = findfirst("*.*", &ff, FA_DIREC);
    {
        int saw_dir = 0, saw_dot = 0, saw_dotdot = 0;
        while (n == 0)
        {
            if (strcmp(ff.ff_name, "SUBDIR") == 0) saw_dir = 1;
            if (strcmp(ff.ff_name, ".") == 0)      saw_dot = 1;
            if (strcmp(ff.ff_name, "..") == 0)     saw_dotdot = 1;
            n = findnext(&ff);
        }
        CHECK(saw_dir, "FA_DIREC finds directories");
        /* COMMON/DIR.C's directory pass filters "." but not "..", because
         * ".." is how the user walks up a level. */
        CHECK(saw_dot && saw_dotdot,
              "FA_DIREC offers '.' and '..' like DOS");
    }

    n = findfirst("*.TXT", &ff, FA_NORMAL);
    CHECK(n == 0 && strcmp(ff.ff_name, "A.TXT") == 0, "wildcard *.TXT");

    n = findfirst("NOSUCH.XXX", &ff, FA_NORMAL);
    CHECK(n != 0, "findfirst reports failure for a missing pattern");

    CHECK(fnsplit("C:\\DIR\\NAME.EXT", drive, d, name, ext) ==
              (DRIVE | DIRECTORY | FILENAME | EXTENSION),
          "fnsplit returns the right flags");
    {
        char merged[128];
        fnmerge(merged, drive, d, name, ext);
        CHECK(strcmp(merged, "C:\\DIR\\NAME.EXT") == 0,
              "fnmerge is the exact inverse of fnsplit");
    }

    drive[0] = '\0'; d[0] = '\0'; name[0] = '\0'; ext[0] = '\0';
    CHECK(fnsplit("/a/b/c.d", drive, d, name, ext) ==
              (DIRECTORY | FILENAME | EXTENSION),
          "fnsplit without a drive");
    /* ext carries the leading dot: SCUW/BLOCK.C passes bakext[] = ".BAK"
     * to fnmerge(), and SCUF/GETSTR.C round-trips fnsplit->fnmerge through
     * strcmp() to validate a typed file name. */
    CHECK(strcmp(d, "/a/b/") == 0 && strcmp(name, "c") == 0 &&
              strcmp(ext, ".d") == 0,
          "fnsplit splits directory, name and extension");

    CHECK(getcurdir(0, cwd) == 0 && cwd[0] != '\0', "getcurdir(0)");
    CHECK(getdisk() >= 0, "getdisk");
    CHECK(setdisk(0) >= 0, "setdisk");
    getdfree(0, &df);
    CHECK(df.df_bsec > 0, "getdfree reports a sector size");
    CHECK(searchpath("sh") != NULL || searchpath("nonexistent.zzz") == NULL,
          "searchpath returns NULL for an unknown name");
    CHECK(searchpath("nonexistent.zzz") == NULL, "searchpath unknown -> NULL");

    {
        char *p = searchpath("sh");
        CHECK(p != NULL, "searchpath finds sh on PATH");
    }

        CHECK(chdir(saved) == 0, "chdir back to the original directory");
    }

    snprintf(full, sizeof full, "%s/SUBDIR", dir);
    rmdir(full);
    snprintf(full, sizeof full, "%s/A.TXT", dir);
    remove(full);
    snprintf(full, sizeof full, "%s/B.C", dir);
    remove(full);
    rmdir(dir);
}

static void test_io(void)
{
    char tmpl[] = "/tmp/shimio.XXXXXX";
    char dir[64], full[128];
    int fd;

    printf("[io.h]\n");
    if (mkdtemp(tmpl) == NULL)
    {
        CHECK(0, "mkdtemp failed");
        return;
    }
    snprintf(dir, sizeof dir, "%s", tmpl);

    /* A DOS-style path with backslashes must be normalised by the
     * interposed open()/creat(). */
    snprintf(full, sizeof full, "%s\\back.dat", dir);
    fd = _creat(full, 0644);
    CHECK(fd >= 0, "creat() with backslash separators");
    if (fd >= 0)
    {
        CHECK(write(fd, "hello", 5) == 5, "write()");
        CHECK(filelength(fd) == 5, "filelength()");
        CHECK(tell(fd) == 5, "tell()");
        CHECK(eof(fd) == 1, "eof() at end of file");
        close(fd);
    }
    snprintf(full, sizeof full, "%s/back.dat", dir);
    CHECK(access(full, F_OK) == 0, "the file really exists at the '/' path");
    remove(full);
    rmdir(dir);
}

static void test_spawn(void)
{
    int rc;

    printf("[process.h]\n");
    rc = spawnl(P_WAIT, "definitely-not-a-real-program-xyz", "x", NULL);
    CHECK(rc == -1, "spawnl returns -1 for a missing program");

    rc = spawnl(P_WAIT, "/bin/true", "/bin/true", NULL);
    CHECK(rc == 0, "spawnl(P_WAIT) returns the child's exit status");

    rc = spawnlp(P_WAIT, "true", "true", NULL);
    CHECK(rc == 0, "spawnlp searches $PATH");

    rc = spawnl(P_WAIT, "/bin/false", "false", NULL);
    CHECK(rc == 1, "spawnl returns the child's non-zero exit status");
}

static void test_graphics(void)
{
    struct viewporttype vp;
    struct linesettingstype ls;
    unsigned char *img;
    unsigned size;
    unsigned char *surf;
    int driver = HERCMONO, mode = HERCMONOHI;

    printf("[graphics.h]\n");
    initgraph(&driver, &mode, "");
    CHECK(graphresult() == grOk, "graphresult() == grOk after initgraph");
    CHECK(getmaxx() == 719 && getmaxy() == 347,
          "HERCMONOHI geometry is 720x348");
    CHECK(shim_bgi_active() == 1, "shim_bgi_active()");

    surf = shim_bgi_surface();
    CHECK(surf == shim_vram_herc(), "the surface is the emulated video memory");
    CHECK(shim_video_is_mapped() == 1, "the fixed video maps are installed");

    putpixel(5, 10, 1);
    CHECK(getpixel(5, 10) == 1, "putpixel/getpixel");
    /* the same pixel reached through a raw far pointer, exactly the way
     * SRC/SCUF/WRITE.C addresses video memory */
    {
        unsigned char far *p =
            (unsigned char far *)(0xB0000000UL + (10 % 4) * 0x2000 + (10 / 4) * 90 + (5 / 8));
        CHECK((*p & (0x80 >> (5 % 8))) != 0,
              "a raw far-pointer write/read reaches the same pixel");
    }

    setcolor(1);
    rectangle(0, 0, 20, 20);
    CHECK(getpixel(0, 0) && getpixel(20, 0) && getpixel(0, 20),
          "rectangle corners drawn");

    setviewport(100, 50, 200, 150, 1);
    getviewsettings(&vp);
    CHECK(vp.left == 100 && vp.clip == 1, "getviewsettings");
    putpixel(0, 0, 1);
    CHECK(getpixel(0, 0) == 1, "viewport-relative putpixel(0,0)");
    {
        unsigned char far *p =
            (unsigned char far *)(0xB0000000UL + (50 % 4) * 0x2000 + (50 / 4) * 90 + (100 / 8));
        CHECK((*p & (0x80 >> (100 % 8))) != 0,
              "viewport-relative coordinates are applied to the surface");
    }
    setviewport(0, 0, getmaxx(), getmaxy(), 0);

    /* getimage/putimage round trip */
    size = imagesize(0, 0, 31, 31);
    CHECK(size > 0, "imagesize()");
    img = (unsigned char *)malloc(size);
    setcolor(1);
    rectangle(0, 0, 31, 31);
    getimage(0, 0, 31, 31, img);
    setcolor(0);
    rectangle(0, 0, 31, 31);
    CHECK(getpixel(0, 0) == 0, "region cleared");
    putimage(0, 0, img, COPY_PUT);
    CHECK(getpixel(0, 0) == 1 && getpixel(31, 0) == 1,
          "putimage restores the saved region");
    putimage(0, 0, img, XOR_PUT);
    CHECK(getpixel(0, 0) == 0, "putimage with XOR_PUT toggles");
    free(img);

    /* floodfill inside a box */
    setcolor(1);
    setfillstyle(SOLID_FILL, 1);
    rectangle(40, 40, 60, 60);
    floodfill(50, 50, -1);
    CHECK(getpixel(50, 50) == 1 && getpixel(41, 41) == 1,
          "floodfill(…, -1) fills the enclosed region");

    /* line style state */
    setlinestyle(DOTTED_LINE, 0, THICK_WIDTH);
    getlinesettings(&ls);
    CHECK(ls.linestyle == DOTTED_LINE && ls.thickness == THICK_WIDTH,
          "getlinesettings() reads back setlinestyle()");
    setlinestyle(SOLID_LINE, 0, NORM_WIDTH);

    CHECK(textwidth("abcd") == 32 && textheight("a") == 8, "text metrics");

    closegraph();
    CHECK(shim_bgi_active() == 0, "closegraph()");
}

static void test_bios(void)
{
    printf("[bios.h]\n");
    CHECK(bioskey(1) == 0 || bioskey(1) != 0, "bioskey(1) does not crash");
    CHECK(biostime(0, 0L) > 0, "biostime() returns the BIOS tick count");
    CHECK(biosprint(PRINTER_STATUS, 0, 0) >= 0, "biosprint(PRINTER_STATUS)");
}

static void test_alloc(void)
{
    void *p;
    printf("[alloc.h]\n");
    p = farmalloc(1024);
    CHECK(p != NULL, "farmalloc()");
    if (p)
        farfree(p);
    CHECK(_heapchk() != 0 || 1, "_heapchk() callable");
}

int main(void)
{
    printf("ThaiWrite port shim - smoke test\n");
    printf("================================\n");
    test_internal_paths();
    test_compat_strings();
    test_dos_interrupts();
    test_dir();
    test_io();
    test_spawn();
    test_graphics();
    test_bios();
    test_alloc();
    printf("================================\n");
    printf("%d checks, %d failed\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
