# CU Writer 1.41 — Linux/CLI port

งาน port ซียูไรเตอร์ 1.41 ให้รันเป็น native บน Linux (และ Windows Terminal/WSL)
แบบ **ncurses TUI + export-first** โดยใช้ terminal ตรงๆ ไม่ต้องมี DOS/emulator

---

## 1. หลักการสำคัญ: ต้นฉบับไม่ถูกแตะ

**กฎเหล็ก: ทุกไฟล์ใต้ `SRC/`, `MANUAL/`, `*.FON`, `*.P24`, `*.P9`, `*.DOC`
คือ source ต้นฉบับของ CU Writer 1.41 — ห้ามแก้เด็ดขาด**

งาน port ของเราทั้งหมดอยู่ใน 2 โฟลเดอร์ของตัวเอง:

```
port/     shim layer + ncurses renderer + notes
tools/    เครื่องมือภายนอก (cw-convert)
```

ตรวจสอบได้ด้วย:

```sh
git diff --stat upstream/master HEAD -- . ':!port' ':!tools' ':!.gitignore'
# ต้องว่างเปล่า = ไม่มีไฟล์ต้นฉบับถูกแก้
```

หรือรัน guard script:

```sh
tools/check-src-pristine.sh
```

เหตุผลของนโยบายนี้:
* เทียบ/rebase กับ upstream ได้ตลอดเวลา ไม่มี merge conflict จาก source ต้นฉบับ
* ส่ง PR กลับ upstream ได้สะอาด (diff เป็นการ **เพิ่ม** ไฟล์ล้วน)
* ข้อบกพร่องของ source ต้นฉบับ (ดูข้อ 5) ถูกจัดการที่ชั้น build/shim ไม่ใช่โดยแก้ต้นฉบับ
* ถ้าจำเป็นต้องแก้จริง ๆ ให้ทำเป็น **build-time patch ลงสำเนา** ใน `build/` เสมอ

---

## 2. ที่มาและเครดิต (Provenance & Credits)

| รายการ | ค่า |
|---|---|
| ต้นฉบับ | CU Writer (ซียูไรเตอร์ / เวิร์ดจุฬา) 1.41, Chulalongkorn University |
| เผยแพร่ครั้งแรก | เมษายน 1989 ผ่าน ช่อง 7 (BBTV) — public domain |
| Source ที่ใช้ | <https://github.com/kytulendu/ThaiWrite> (CU Writer 1.41 source, ThaiWrite) |
| Commit ฐาน | `729a3b1` — *SCUW: Fix compile warning.* (23 มี.ค. 2026) |
| Fork งาน port นี้ | <https://github.com/ake117/ThaiWrite> |
| Encoding ต้นฉบับ | TIS-620 (Thai Industrial Standard 620-2533) |
| License ต้นฉบับ | Public domain (เผยแพร่ public domain ตั้งแต่ปี 1989) |
| License งาน port | The Unlicense (`LICENSE`) |

**ผู้พัฒนา CU-Writer** (จากคู่มือ 1.2 และ source 1.41):
สถาบันบริการคอมพิวเตอร์ จุฬาลงกรณ์มหาวิทยาลัย และภาควิชาวิศวกรรมคอมพิวเตอร์
คณะวิศวกรรมศาสตร์ จุฬาลงกรณ์มหาวิทยาลัย —
สำนวน หิรัญวงษ์, ไกรศร เลียนษี, ดำเกิง ธรรมเถกิงกิจ, ทศพล อภิกุลวณิช,
ธณัฎฐ์ จิระกาล, ประสาท โตปิตุมาตุคุณ, ลักษณ์ชัย ลือชัยชนะกุล, วิชัย ปฏิภาภรณ์,
วิทยา วงศ์อนวัช, สุทธิพงษ์ คนาคกร, สุบรรณ เย็นใจชน, สุวพจน์ จันทโรจนวงศ์

**ThaiWrite / source 1.41** — Khralkatorrix (kytulendu):
ปรับชื่อฟังก์ชันใน `COMMON/CSCR.C` (`line` → `_line`) เพื่อไม่ชนกับ BGI,
แก้ `MAKEFILE`/`TURBOC.CFG`, เพิ่มไฟล์ฟอนต์/คู่มือ/เอกสาร

**งาน port นี้** เป็นงานแยก (clean-room port) ที่ *อ่านและ compile* source ต้นฉบับ
โดยไม่ดัดแปลงเนื้อหาเดิม — โค้ดที่เราเพิ่ม (`port/`, `tools/`) เผยแพร่ภายใต้
**The Unlicense** (ดู `LICENSE`) ให้สอดคล้องกับสถานะ public domain ของ CU Writer 1.41

---

## 3. โครงสร้าง

```
port/shim/        แทน header ของ Borland Turbo C 2.01 (dos.h, bios.h, conio.h,
                  dir.h, process.h, io.h, graphics.h, ...) + shim_*.c
                  → libtwshim.a
                  รวมไฟล์ stub ชื่อมี backslash เช่น "..\common\proto.h"
                  เพราะ gcc ไม่แปล DOS separator ใน #include "..\common\x.h"
port/renderer/    ncurses backend สำหรับ API ชั้น display (CWGRPHC.H)
                  → libtwrenderer.a + test_renderer (headless, 46 tests)
port/notes/       รายงานการวิเคราะห์ (เช่น dynamic-layout.md)
tools/            cw-convert — แปลงไฟล์ TIS-620 → UTF-8 → docx/odt/html/md/txt/pdf
```

Source ต้นฉบับถูกอ้างอิงผ่าน path สัมพัทธ์ `../SRC` จากใน `port/shim/`
(ใน repo นี้ source อยู่ที่ `SRC/` ที่ root แล้ว ไม่มีสำเนา nested)

---

## 4. Build

```sh
make -C port/renderer      # libtwrenderer.a + test_renderer
make -C port/shim          # libtwshim.a
make -C port/shim check    # compile-check source ต้นฉบับกับ shim headers
```

Build ด้วย `-std=gnu89` (source เขียนแบบ K&R/gnu89)
`geninc/` เป็น symlink mirror ชื่อไฟล์ตัวพิมพ์เล็ก สร้างอัตโนมัติจาก target `geninc`
(ใช้ `-iquote` ไม่ใช่ `-I` เพื่อไม่ให้ mirror ไปบัง system header)

---

## 5. ข้อบกพร่องใน source ต้นฉบับ และวิธีจัดการโดยไม่แตะ SRC/

พบ 2 จุดที่ gcc ปฏิเสธ แต่ Turbo C 2.01 แค่เตือน:

1. **`SCUF/TUTIL1.C`** — `proto.h` ประกาศ `thaistrlen(unsigned char *)`
   แต่นิยามจริงเป็น `char *` ⇒ gcc error *conflicting types*
   → **จัดการที่ shim**: stub `port/shim/..\common\proto.h` ให้ประกาศให้ตรงกับนิยามจริง
2. **`SCUP/PRINTING.C`** — บรรทัด ~211–215 มี comment ซ้อน comment (`*/` ข้างในปิด outer)
   เหลือ `*/` โดน parse เป็น expression
   → ต้องเป็น build-time patch ลงสำเนาใน `build/` หรือตัดโมดูล printing ออก
   (สำหรับ CLI export-first โมดูล printing ไม่จำเป็นต่อเป้าหมายหลัก)

ทั้งสองไฟล์ถูก mark เป็น `XFAIL` ใน `port/shim/Makefile` เพื่อให้ `make check`
ยังใช้เป็น gate ได้ตามปกติ

---

## 6. Branch & remotes

```
fork      https://github.com/ake117/ThaiWrite      ← งานของเรา
upstream  https://github.com/kytulendu/ThaiWrite   ← ต้นฉบับ

master                  = ต้นฉบับ 100% (ไม่ถูกแตะ) ใช้เทียบ/ทำ PR
port/linux-terminal   = งาน port (ต่อยอดจาก master, เพิ่มไฟล์ล้วน)
legacy/port-scaffold    = ประวัติ repo port เดิมก่อนย้ายเข้ามา (เก็บไว้อ้างอิง)
```

## เสียง (PC speaker emulation)

เพลงจุฬาลงกรณ์มหาวิทยาลัยตอนเปิดโปรแกรม + เสียงเตือน `errorsound()` ใช้ `aplay`
(alsa-utils) เป็นตัวเล่น — shim เขียนคลื่นสี่เหลี่ยม 8-bit 8 kHz ให้ aplay
ผ่านคิวแบบ non-blocking ตัว editor จึงใช้งานต่อได้ทันทีระหว่างเพลงเล่น

- เพลงเล่นเฉพาะตอนรัน `cw` **ไม่ใส่ชื่อไฟล์** (ต้นฉบับเล่นเช่นเดียวกัน —
  ใส่ชื่อไฟล์ตอนรัน โค้ดเดิมจะคิวปุ่ม "พิมพ์ชื่อไฟล์" ซึ่งทำให้ `cusong()`
  เห็นว่ามีการกดปุ่มแล้วข้ามเพลง) · กดปุ่มใดก็ได้ระหว่างเพลงเพื่อข้าม
- ปิดเสียงทั้งหมด: ตั้ง env `CW_NOSOUND=1`
- debug คิวโน้ต: env `CW_TONE_DEBUG` (โน้ตจะพิมพ์ทับลงจอ)

---

## 7. การแสดงผล (display layer)

แปลง TIS-620 → UTF-8 ที่ขอบ terminal เท่านั้น (0xA1-0xFB → U+0E01-U+0E5B
แบบ 1:1) ส่วน **อักขระเส้น** ของฟอนต์ต้นฉบับ (`NORMAL.FON`) ต้องแปลงเป็น
**Unicode Box Drawing** ไม่ใช่ปล่อยไบต์ดิบ ซึ่งจะกลายเป็น C1 control
ที่ terminal ไม่แสดงผล (กรอบกล่องหายไปทั้งเส้น):

| ฟอนต์ต้นฉบับ | ใช้ที่ | Unicode |
|---|---|---|
| 0x8F / 0x90 / 0x91 / 0x92 / 0x93 | กากบาท + สามแยก | ┼ ┬ ┴ ┤ ├ |
| 0x95 / 0x96 | เส้นนอน / เส้นตั้ง | ─ │ |
| 0x98 / 0x99 / 0x9A / 0x9B | มุมทั้งสี่ | ┌ ┐ └ ┘ |

เส้นกรอบที่ต้นฉบับวาดด้วย `plot()` (`_rectangle`, เวอร์ชันกราฟิกของ
`framebox`) ถูกเก็บใน `port_cell.dot[]` เป็น mask ของคอลัมน์พิกเซลต่อ
กลุ่มแถว (dot[0] = แถว 0-6, dot[1] = 7-13, dot[2] = 14-19) — เส้นนอนเปิด
ทุกคอลัมน์ของกลุ่มเดียว (`0xff`) เส้นตั้งเปิดบิตเดียวซ้ำทุกกลุ่ม
(`0x01` = ขอบซ้าย, `0x80` = ขอบขวา) ตัว renderer เลือกอักษรเส้นบางให้ตรงรูป
โดยดูว่าเส้นต่อไปยังเซลล์ข้างเคียงหรือไม่ (มุม/สามแยก/กากบาท) — **เดิมวาด
เป็นครึ่งบล็อก ▀▄▌▐ ซึ่งทำให้กรอบดูหนากว่าเส้น 1 พิกเซลของต้นฉบับ**

**โลโก้พระเกี้ยว** — `prakeaw()` (EDISP.ASM `_eprakeaw`, HDISP.ASM
`_hprakeaw`) blit bitmap `chulalogo1`/`chulalogo2` (HDISP.ASM:152,157)
ขนาด 32x20 พิกเซล ที่ byte column 1 และ 3 ⇒ คอลัมน์ 1-4 ของแถว 0

**งาน port ย้ายพระเกี้ยวไปวาดในกล่อง splash screen** แทนไทเทิลบาร์ เพราะ
ไทเทิลบาร์มีที่ให้แค่ 4 เซลล์ x 1 บรรทัด — เพดาน 8x4 ดอท ย่อ 32x20 พิกเซล
แล้วเหลือก้อนทึบดูไม่ออกว่าเป็นรูป

- `dispstrhgc` ตรวจจับกล่อง splash (แถว 5-13 ที่ใช้ `REVERSEATTR`) แล้ว
  **ขยายกล่องไปทางซ้าย 16 คอลัมน์** (ขอบซ้ายจาก col 17 → col 1 ขอบขวา
  คงไว้ col 60) เพื่อให้มีที่วางพระเกี้ยวในกล่องโดยไม่ล้นขอบ
- พระเกี้ยววางในกล่องซ้ายสุด **16 เซลล์ x 5 บรรทัด = 32x20 ดอท = 1:1
  กับ bitmap ต้นฉบับทุกพิกเซล ไม่ย่อ** (`port_draw_crest()`) และวาดด้วย
  `REVERSEATTR` เหมือนกล่อง ⇒ **ดำบนพื้นขาว** กลืนกับแถบ splash
- หนึ่งเซลล์ = 2x4 ดอทของอักขระ **Braille** (U+2800-U+28FF) เก็บดอทมาสก์
  ใน `port_cell.over`; เซลล์ที่ไม่มีหมึกก็ตั้ง attr เดียวกันเพื่อไม่ให้เป็น
  ช่องดำคั่นกลางแถบ
- `prakeaw()` ของต้นฉบับถูกเรียกตอนเปิดจอ แต่ใน port เป็น no-op (ย้ายการ
  วาดไป splash แล้ว) — ธงไทเทิลบาร์จึงเป็น `┌─────จุฬาลงกรณ์มหาวิทยาลัย`

ลำดับไบต์ของ bitmap: `_hprakeaw` เขียนด้วย `mov es:[di],ax` ⇒ **AL อยู่
address ต่ำ = ครึ่งซ้าย** (little-endian) และภายในไบต์บิต 7 = พิกเซลซ้ายสุด

### ความต่างจากต้นฉบับที่ตั้งใจ (intentional)

- ข้อความที่เปิด `^E` (`ENLARGEATTR`) ได้ความกว้าง 2 คอลัมน์ต่ออักขระ
  เท่าต้นฉบับ แต่ glyph ไม่ถูกยืด — บน terminal จึงเห็นเป็นตัวอักษรห่างกัน
  (หัวกล่อง splash: `C U   W R I T E R`) ไม่ใช่ glyph หนาสองเท่า
  · ช่องว่างครึ่งหลังของอักษรขยายเดิม "ไม่ถูกเขียน" จึงค้างเป็นพื้นหลัง
  เดิมและเห็นเป็นกล่องดำคั่นกลางแถบ reverse — port เติม blank ด้วย attr
  เดียวกันให้แล้ว
- กล่อง splash ถูกขยายไปทางซ้าย 16 คอลัมน์ (จาก col 17 → col 1) เพื่อวาง
  พระเกี้ยวในกล่อง — ตำแหน่งข้อความในกล่องคงเดิมทุกคอลัมน์
- `port_cell.over` เขียนไว้ที่ byte 8 ของ `serialize_cell` จึงตามไปกับ
  getwind/putwind เฉพาะเมื่อ caller ใช้ record กว้าง ≥ 9 ไบต์ (เส้นทาง
  ปกติของ DOS ใช้ 1 ไบต์/record) — `prakeaw()` วาดใหม่ตอนเปิดจอทุกครั้ง
  เหมือน flow ของต้นฉบับอยู่แล้ว
