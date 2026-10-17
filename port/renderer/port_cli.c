/*-----------------------------------------------------------------------*/
/*  port_cli.c - command line options handled before CU Writer's main()  */
/*                                                                       */
/*  ต้นฉบับเป็นโปรแกรม DOS: main() ของ CW.C แยก argv แบบ DOS (ไดรฟ์/พาธ  */
/*  ด้วย '\\' และถือว่า '/' นำหน้าคือ option) จึงรับ --version / --help    */
/*  ไม่ได้ และยังเสี่ยง crash ถ้าเจอพาธแบบ POSIX                         */
/*                                                                       */
/*  ไฟล์นี้ใช้ constructor (ทำงานก่อน main) อ่าน /proc/self/cmdline แล้ว  */
/*  ตอบ option ของ port ทันที — ไม่ต้องแก้ SRC/ แม้แต่ไบต์เดียว          */
/*-----------------------------------------------------------------------*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "version.h"           /* สร้างตอน build จาก git describe */

#define CW_BASE_VERSION "CU Writer 1.41"

static int arg_is(const char *arg, const char *a, const char *b)
{
    return (strcmp(arg, a) == 0) || (b != NULL && strcmp(arg, b) == 0);
}

static void print_version(void)
{
    printf("%s (Linux terminal port) %s\n", CW_BASE_VERSION, CW_PORT_VERSION);
    printf("ncurses TUI · branch port/linux-terminal · %s\n",
           "https://github.com/ake117/ThaiWrite");
}

static void print_usage(FILE *out)
{
    fprintf(out,
        "%s (Linux terminal port) %s\n"
        "\n"
        "usage: cw [option] [file]\n"
        "\n"
        "  file            เปิดแฟ้มข้อมูล CU Writer (.DOC, TIS-620)\n"
        "  -V, --version   แสดงเวอร์ชันแล้วจบการทำงาน\n"
        "  -h, --help      แสดงข้อความนี้แล้วจบการทำงาน\n"
        "\n"
        "ภายในโปรแกรม: ESC เปิดเมนู, ALT-X ออก\n"
        "แปลงไฟล์ออกเป็น docx/md/html: tools/cw-convert --help\n",
        CW_BASE_VERSION, CW_PORT_VERSION);
}

__attribute__((constructor))
static void port_cli(int argc, char **argv)
{
    char buf[4096];
    ssize_t n;
    int fd;
    char *arg;
    char *end;

    (void)argc;
    (void)argv;

    /*  constructor ไม่ได้ค่า argc/argv จึงอ่านจาก /proc (Linux)         */
    fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0)
        return;
    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return;
    buf[n] = '\0';

    arg = buf;
    end = buf + n;
    while (arg < end) {
        if (arg_is(arg, "-V", "--version")) {
            print_version();
            exit(0);
        }
        if (arg_is(arg, "-h", "--help")) {
            print_usage(stdout);
            exit(0);
        }
        arg += strlen(arg) + 1;
    }
}
