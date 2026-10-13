# =====================================================================
# port/Makefile - สร้าง binary  "cw"  (CU Writer 1.41 - CLI port)
# ---------------------------------------------------------------------
# compile source ต้นฉบับจาก ../SRC/ โดย**ไม่แก้ไฟล์** (ใช้ shim headers
# + geninc mirror ของ port/shim) แล้ว link กับ libtwshim.a + libtwrenderer.a
#
#   make            สร้าง cw
#   make clean      ลบ build/ + cw
#
# object list ตาม SRC/SCUW/SCUW.TLR + SRC/COMMON/MAKEFILE ของต้นฉบับ
# โดยตัด 7 object ของ display layer ออก เพราะ port/renderer แทนให้แล้ว:
#   edisp  hdisp  adisp  cwgrphc  cwegrph  cwagrph  cscrn
# =====================================================================

CC      = gcc
MAKE    = make

# -Ishim  : ให้ gcc หาไฟล์ stub ชื่อมี backslash ("..\common\x.h") เจอ
# -iquote : geninc mirror ชื่อไฟล์ตัวพิมพ์เล็ก (แบบเดียวกับ port/shim check)
# -w      : source ต้นฉบับเป็น Turbo C 2.01 เขียนหลวม gcc เตือนเยอะมาก
CFLAGS  = -std=gnu89 -x c -O2 -w

# -iquote ต้องเรียงตาม "โฟลเดอร์ของไฟล์ที่ include เองมาก่อน" แบบ DOS
# ไม่งั้นชื่อไฟล์ชนกันข้ามโฟลเดอร์ (เช่น INC.H มีทั้งใน SCUF และ SCUW)
Q_COMMON  = -iquote shim/geninc/common
Q_SCUF    = -iquote shim/geninc/scuf
Q_SCUP    = -iquote shim/geninc/scup
Q_SCUW    = -iquote shim/geninc/scuw -iquote shim/geninc/scuw/thaisep

INC_C   = -Ishim $(Q_COMMON) $(Q_SCUF) $(Q_SCUP) $(Q_SCUW)
INC_W   = -Ishim $(Q_SCUW) $(Q_COMMON) $(Q_SCUF) $(Q_SCUP)
INC_T   = -Ishim -iquote shim/geninc/scuw/thaisep $(Q_COMMON) $(Q_SCUW) $(Q_SCUF) $(Q_SCUP)

SRC     = ../SRC
BUILD   = build
LIBS    = shim/libtwshim.a renderer/libtwrenderer.a -lncursesw -lm

COMMON  = EKBD DIR FILEUTIL GRDETECT CUSONG STAMPDAT
SCUW    = SCRN GETSTR TUTIL1 EDIT INS DEL MENU KBD STATUS \
          MOVE MOVEMENT WRAP ONSCRN FILE SEARCH BLOCK CW CWSETUP
THAISEP = MAINSYLS THAIENG WOWAEN FINDCHAR HOHIP HUNAKAD JULCUT KARAN \
          LEADCON MAIMALAI MAIMUAN OANG NSTRCMP RORUA RU SARAE SARAA \
          SARAAE SARAIE SARAO SARAAA SARAU SARAUE SARAUEE SARAAM GLOBAL

C_OBJS  = $(addprefix $(BUILD)/c_,$(addsuffix .o,$(COMMON)))
W_OBJS  = $(addprefix $(BUILD)/w_,$(addsuffix .o,$(SCUW)))
T_OBJS  = $(addprefix $(BUILD)/t_,$(addsuffix .o,$(THAISEP)))
OBJS    = $(C_OBJS) $(W_OBJS) $(T_OBJS)

all: cw

cw: $(OBJS) shim/libtwshim.a renderer/libtwrenderer.a
	$(CC) -o $@ $(OBJS) $(LIBS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/c_%.o: $(SRC)/COMMON/%.C | $(BUILD)
	$(CC) $(CFLAGS) $(INC_C) -c -o $@ $<

# SOUND.C: ต้นฉบับมี #ifdef NOSOUND ที่ทำให้ soundnoise()/errorsound() เป็น no-op
# บน Linux ไม่มี PC speaker ให้ขับ และ biosdelay() ของ SOUND.C จะสpin ตาม
# BIOS tick ทำให้เพลงจุฬาฯ ตอนเปิดโปรแกรมยาว ~73 วินาที
# → ไม่ compile SOUND.C แล้ว (เดิม define NOSOUND) — shim_dos.c เป็นผู้ให้
#   soundnoise()/errorsound() แทน แบบ non-blocking ผ่าน aplay (ดู shim_dos.c)

$(BUILD)/w_%.o: $(SRC)/SCUW/%.C | $(BUILD)
	$(CC) $(CFLAGS) $(INC_W) -c -o $@ $<

$(BUILD)/t_%.o: $(SRC)/SCUW/THAISEP/%.C | $(BUILD)
	$(CC) $(CFLAGS) $(INC_T) -c -o $@ $<

# geninc (symlink mirror ชื่อตัวพิมพ์เล็ก) สร้างโดย port/shim
shim/geninc:
	$(MAKE) -C shim geninc

$(OBJS): | shim/geninc

clean:
	rm -rf $(BUILD) cw

.PHONY: all clean
