# Makefile for the ThaiWrite (CU-Writer 1.41) Linux port shim layer.
#
# Nothing under thaiwrite/ is modified.  This directory supplies:
#
#   * the Turbo C 2.01 header replacements  (dos.h bios.h conio.h io.h
#     dir.h alloc.h mem.h process.h graphics.h stdio.h stdlib.h string.h
#     errno.h fcntl.h sys/stat.h ...)
#   * shim_*.c  - the user-space emulation behind those headers
#   * "..\common\*.h" stubs - files whose *names* contain backslashes,
#     because gcc does not translate the DOS separator in
#     #include "..\common\x.h"
#
# Targets:
#   make            build libtwshim.a
#   make check      compile the ported sources against the shim headers
#   make clean
#
# Two DOS-specific build problems the original sources have are solved
# here for the check target; the real port build must do the same:
#
#   1. backslash include paths - handled by the literal-name stubs in
#      this directory (gcc leaves the backslashes alone, so a file whose
#      name really is "..\common\cwgrphc.h" satisfies the lookup).
#   2. case-insensitive header names - the originals say "cwtype.h"
#      while the file on disk is CWTYPE.H.  geninc/ is generated as a
#      lower-cased symlink mirror of each source subdirectory; the
#      per-file-group -iquote order below resolves each name the way
#      DOS did (the including file's own directory first).
#
# The original sources are also compiled with -std=gnu89: they are
# Turbo C 2.01 code, i.e. C89 with extensions, and several modules call
# functions they never declared (Turbo C only warned; gcc 17 rejects
# implicit declarations in C99+ mode).

CC      = gcc
AR      = ar

SHIM_CFLAGS = -std=gnu99 -Wall -Wextra -O2 -I.
SRC_CFLAGS  = -std=gnu89 -O2 -x c -fsyntax-only

# shim_graphics.c uses sin()/cos(), so anything linking libtwshim.a needs -lm.

SHIM_SRCS = shim_compat.c shim_dos.c shim_bios.c shim_conio.c \
            shim_io.c shim_dir.c shim_process.c shim_graphics.c
SHIM_OBJS = $(SHIM_SRCS:.c=.o)

SRC = ../../SRC

all: libtwshim.a

libtwshim.a: $(SHIM_OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(SHIM_CFLAGS) -c -o $@ $<

# ---------------------------------------------------------------- #
#  Case-folded include mirror                                      #
# ---------------------------------------------------------------- #
MIRROR_DIRS = COMMON SCUF SCUP SCUW SCUW/THAISEP

geninc:
	@rm -rf geninc
	@mkdir -p geninc
	@for d in $(MIRROR_DIRS); do \
	    l=$$(echo $$d | tr A-Z a-z); \
	    mkdir -p geninc/$$l; \
	    for f in $$(ls $(SRC)/$$d 2>/dev/null); do \
	        case "$$f" in \
	            *.H|*.h|*.C|*.c) \
	                ln -sf "$$(cd $(SRC)/$$d && pwd)/$$f" "geninc/$$l/$$(echo $$f | tr A-Z a-z)"; \
	                ;; \
	        esac; \
	    done; \
	done

# ---------------------------------------------------------------- #
#  Compile-check the original sources                              #
# ---------------------------------------------------------------- #
# -iquote (not -I) so that the mirror cannot shadow a system header.
Q_COMMON  = -iquote geninc/common
Q_SCUF    = -iquote geninc/scuf
Q_SCUP    = -iquote geninc/scup
Q_SCUW    = -iquote geninc/scuw -iquote geninc/scuw/thaisep

INC_COMMON = -I. $(Q_COMMON) $(Q_SCUF) $(Q_SCUP) $(Q_SCUW)
INC_SCUF   = -I. $(Q_SCUF) $(Q_COMMON) $(Q_SCUP) $(Q_SCUW)
INC_SCUP   = -I. $(Q_SCUP) $(Q_COMMON) $(Q_SCUF) $(Q_SCUW)
INC_SCUW   = -I. $(Q_SCUW) $(Q_COMMON) $(Q_SCUF) $(Q_SCUP)

# Object lists taken from the original MAKEFILEs (MAKECOM/MAKEFED/
# MAKEPRN/MAKETW).  The .ASM display adapters are deliberately absent:
# port/renderer/cwgrphc.c replaces them.
COMMON_C = EKBD.C DIR.C CSCRN.C FILEUTIL.C SOUND.C CUSONG.C GRDETECT.C \
           TUTIL1.C
SCUF_C   = FED.C WRITE.C POPUP.C INT.C GETCH.C CONVERT.C KBD.C FILE.C \
           CUFSETUP.C FONTMSG.C TUTIL1.C
SCUP_C   = KBD.C GETSTR.C TUTIL1.C CUPSETUP.C PMENU.C SMENU.C PRTUTIL.C \
           PRINTING.C PRT.C PRTT.C PRTG.C PIC.C DOT.C CPI.C GLOBAL.C \
           PRNCNTRL.C ERROR.C PREPROC.C 24PINS.C ATTR.C CPTHAI.C PRTBUF24.C
SCUW_C   = SCRN.C GETSTR.C TUTIL1.C EDIT.C INS.C DEL.C MENU.C KBD.C \
           STATUS.C MOVE.C MOVEMENT.C WRAP.C ONSCRN.C FILE.C SEARCH.C \
           BLOCK.C CW.C CWSETUP.C

# Files known to be broken in the *source* (not in the shim).  Each
# needs a one-line patch in the port's copy of thaiwrite; they are
# listed here so the check stays a usable gate.
XFAIL = $(SRC)/SCUF/TUTIL1.C \
        $(SRC)/SCUP/PRINTING.C

# Why these two cannot be compiled without patching the ported copy:
#   SCUF/TUTIL1.C   - proto.h declares thaistrlen(unsigned char *) while
#                     the definition says char *; Turbo C only warned,
#                     gcc reports a conflicting type.
#   SCUP/PRINTING.C - lines 211-215 contain a comment inside a comment
#                     (the inner */ closes the outer /*), leaving a
#                     stray "*/" that gcc parses as an expression.

check: geninc $(SHIM_OBJS)
	@ok=0; fail=0; xfail=0; \
	run() { \
	    f="$$1"; inc="$$2"; log=$$(echo "$$f" | tr '/' '_').log; \
	    if $(CC) $(SRC_CFLAGS) $$inc "$$f" >/dev/null 2>$$log; then \
	        ok=$$((ok+1)); echo "OK   $$f"; rm -f $$log; \
	    else \
	        fail=$$((fail+1)); echo "FAIL $$f  (see $$log)"; \
	    fi; \
	}; \
	for n in $(COMMON_C); do \
	    case " $(XFAIL) " in *" $(SRC)/COMMON/$$n "*) \
	        xfail=$$((xfail+1)); echo "XFAIL $$n (known source defect)"; continue;; esac; \
	    run $(SRC)/COMMON/$$n "$(INC_COMMON)"; done; \
	for n in $(SCUF_C); do \
	    case " $(XFAIL) " in *" $(SRC)/SCUF/$$n "*) \
	        xfail=$$((xfail+1)); echo "XFAIL $$n (known source defect)"; continue;; esac; \
	    run $(SRC)/SCUF/$$n "$(INC_SCUF)"; done; \
	for n in $(SCUP_C); do \
	    case " $(XFAIL) " in *" $(SRC)/SCUP/$$n "*) \
	        xfail=$$((xfail+1)); echo "XFAIL $$n (known source defect)"; continue;; esac; \
	    run $(SRC)/SCUP/$$n "$(INC_SCUP)"; done; \
	for n in $(SCUW_C); do \
	    case " $(XFAIL) " in *" $(SRC)/SCUW/$$n "*) \
	        xfail=$$((xfail+1)); echo "XFAIL $$n (known source defect)"; continue;; esac; \
	    run $(SRC)/SCUW/$$n "$(INC_SCUW)"; done; \
	echo "---- $$ok ok, $$fail failed, $$xfail known-broken ----"; \
	test $$fail -eq 0

# ---------------------------------------------------------------- #
#  Smoke test                                                      #
# ---------------------------------------------------------------- #
TEST_CFLAGS = -std=gnu99 -Wall -Wextra -O2 -I.

test: libtwshim.a test_shim
	./test_shim

test_shim: test_shim.c libtwshim.a
	$(CC) $(TEST_CFLAGS) -o $@ test_shim.c libtwshim.a -lm

clean:
	rm -rf geninc *.log
	rm -f $(SHIM_OBJS) libtwshim.a test_shim

.PHONY: all check test clean geninc
