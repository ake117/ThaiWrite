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
โดยไม่ดัดแปลงเนื้อหาเดิม — สถานะลิขสิทธิ์ของโค้ดที่เราเพิ่มยังเปิดให้กำหนด
(ต้นฉบับเป็น public domain ⇒ แนะนำ Unlicense/CC0 ให้สอดคล้อง)

---

## 3. โครงสร้าง

```
port/shim/        แทน header ของ Borland Turbo C 2.01 (dos.h, bios.h, conio.h,
                  dir.h, process.h, io.h, graphics.h, ...) + shim_*.c
                  → libtwshim.a
                  รวมไฟล์ stub ชื่อมี backslash เช่น "..\common\proto.h"
                  เพราะ gcc ไม่แปล DOS separator ใน #include "..\common\x.h"
port/renderer/    ncurses backend สำหรับ API ชั้น display (CWGRPHC.H)
                  → libtwrenderer.a + test_renderer (headless, 41 tests)
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
port/fill-asm-symbols   = งาน port (ต่อยอดจาก master, เพิ่มไฟล์ล้วน)
legacy/port-scaffold    = ประวัติ repo port เดิมก่อนย้ายเข้ามา (เก็บไว้อ้างอิง)
```
