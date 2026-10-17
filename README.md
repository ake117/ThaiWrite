# CU Writer 1.41 — Linux / CLI port

ให้ซียูไรเตอร์ 1.41 (ซียูไรเตอร์ / เวิร์ดจุฬา — DOS word processor ไทย, WordStar clone, 1989)
กลับมารัน **native บน Linux และ Windows Terminal/WSL** ผ่าน ncurses TUI — ไม่ต้องใช้
DOSBox หรือ emulator ใด ๆ และเน้น **export-first**: พิมพ์/แก้เอกสารไทยใน terminal แล้ว
export เป็น `.docx` `.odt` `.html` `.md` `.txt` `.pdf` ได้ทันที

> **สถานะ:** milestone 2 — [`v0.0002`](https://github.com/ake117/ThaiWrite/releases/tag/v0.0002)
> ใช้งานได้จริง (เปิด/แก้/บันทึก/เมนู/splash) แต่ยังไม่ครบฟีเจอร์ของต้นฉบับ
> (โมดูล printing ยังไม่รวม) · ประวัติเวอร์ชันทั้งหมดอยู่ที่
> **[`CHANGELOG.md`](CHANGELOG.md)** · งานที่ยังไม่เสร็จและข้อจำกัดที่รู้ตัว
> อยู่ในหัวข้อ ["งานที่ยังไม่เสร็จ (TODO)"](#งานที่ยังไม่เสร็จ-todo) ด้านล่าง
>
> **Repo นี้เป็น fork ของ** [kytulendu/ThaiWrite](https://github.com/kytulendu/ThaiWrite)
> (source CU Writer 1.41 ฉบับดั้งเดิม) · **ไฟล์ต้นฉบับใต้ `SRC/` ไม่ถูกแก้ไข** ยังเหมือน
> upstream เป๊ะ ตรวจสอบได้ด้วย `tools/check-src-pristine.sh`
>
> README นี้แทนที่ README ต้นฉบับของ ThaiWrite — ดูของเดิมได้ที่
> [upstream master README](https://github.com/kytulendu/ThaiWrite/blob/master/README.md)
> หรือที่ branch `master` ของ fork นี้

| | |
|---|---|
| Branch งาน port | [`port/linux-terminal`](https://github.com/ake117/ThaiWrite/tree/port/linux-terminal) |
| ฐาน (upstream) | `729a3b1` — CU Writer 1.41 source / ThaiWrite |
| License งาน port | The Unlicense — [`LICENSE`](LICENSE) |
| License ต้นฉบับ | Public domain (CU Writer 1.41 เผยแพร่ public domain ตั้งแต่ปี 1989) |
| Encoding | TIS-620 ต้นฉบับ → UTF-8 ที่ชั้น boundary |
| แพลตฟอร์มเป้าหมาย | Linux (x86_64), Windows Terminal / WSL |

## งานที่ยังไม่เสร็จ (TODO)

อัปเดตล่าสุด `v0.0002` (2026-10-17) · อะไรที่เสร็จแล้วดูใน
[`CHANGELOG.md`](CHANGELOG.md) · ช่วยทดสอบแล้วเจอปัญหา → เปิด issue ได้เลย

### 1. ทดสอบ (สำคัญที่สุด — ยังไม่เคยทำแบบเต็มรูปแบบ)

- [ ] พิมพ์ไทยต่อเนื่องหลายหน้า (สระ/วรรณยุกต์/การันต์ซ้อนหลายชั้น) เทียบตำแหน่งกับ DOS
- [ ] บันทึก + เปิดไฟล์ซ้ำ ตรวจว่าไบต์ TIS-620 ไม่เสีย (ใช้ `TEST.DOC`, `TEST2.DOC`, `TEST3.DOC`, `TAIRUT.DOC`)
- [ ] แก้ไขขั้นสูง: ย้าย/ลบ/แทรกบล็อก, ค้นหา–แทนที่, เปลี่ยนหน้า/บรรทัด, tab
- [ ] เมนูย่อยทุกตัว (9 เมนูหลัก × ทุกคำสั่ง)
- [ ] resize หน้าต่างระหว่างแก้เอกสาร (ขยาย/ย่อ) — โค้ดรองรับแล้วแต่ยังไม่ทดสอบ
- [ ] Windows Terminal / WSL — README อ้างว่าใช้ได้ แต่ยังไม่เคยรันจริงบนเครื่อง Windows

> ที่ทดสอบแล้ว: startup, splash screen, การแสดงผล/attr, เมนูหลัก, ALT-X ออก,
> `--version`/`--help`, `cw-convert` กับไฟล์ตัวอย่าง

### 2. ฟีเจอร์ที่ยังไม่ครบ

- [ ] **โมดูล printing** — `SCUP/PRINTING.C` ยังไม่รวมเข้า build (source มี comment
      ซ้อน comment ที่ gcc ปฏิเสธ ต้อง build-time patch ลงสำเนาใน `build/`)
- [ ] **ตัวเลือกแบบ DOS ของต้นฉบับ** (`/h /v /e /m /a /n /w /p /s`) ยังไม่ map เป็น
      option ของ port — ตอนนี้มีแค่ `--version/-V` และ `--help/-h`
- [ ] **`tools/cw-convert`** ยังไม่มีชุดทดสอบอัตโนมัติ (ทดสอบมือกับ `TEST*.DOC`)
      และยังไม่รองรับตาราง/รูปภาพถ้ามีในเอกสาร

### 3. ข้อจำกัดที่รู้ตัว (ยอมรับไว้ — ไม่ใช่บั๊กที่รอแก้)

- **หนึ่งเซลล์แสดงได้อย่างเดียว**: DOS วาดข้อความกับเส้นคนละแถวพิกเซลในเซลล์เดียว
  จึงเห็นทั้งคู่ แต่ terminal แสดงได้อย่างเดียว → port ให้ *ข้อความชนะเส้น*
  ผลคือขอบกล่องที่ทับตัวอักษร (เช่นกรอบบน dropdown) จะเห็นเป็นตัวอักษรแทนเส้น
- **จอแคบกว่า ~63 คอลัมน์**: บรรทัดไทยยาวถูกตัดขอบขวา เพราะ UI ต้นฉบับออกแบบ
  มาสำหรับจอ 80 คอลัมน์ (จอ 80+ ทำงานปกติ)
- **พระเกี้ยว** ใช้ Braille 2×4 ดอทต่อเซลล์ = 32×20 พิกเซล ซึ่งเป็นความละเอียด
  สูงสุดของ text mode — ถ้าต้องการละเอียดกว่านี้ต้องเปลี่ยนไปใช้ graphics
  protocol (sixel/kitty) ซึ่งจะไม่ใช่ text mode อีก
- **เสียง** ใช้ `aplay` (alsa-utils) — ถ้าไม่มี `aplay` จะเงียบแต่โปรแกรมทำงานปกติ

## หลักการทำงาน

1. **ต้นฉบับไม่ถูกแตะ** — งานทั้งหมดเป็นการ *เพิ่ม* ไฟล์ใน `port/` และ `tools/`
   จุดที่ source ต้นฉบับ compile ไม่ผ่านกับ gcc ถูกจัดการที่ชั้น shim หรือ
   build-time patch ลงสำเนาใน `build/` เท่านั้น ไม่แก้ `SRC/`
2. **Dynamic display size** — ไม่ยึดติด 80x25 · ถามขนาดจริงด้วย `ioctl(TIOCGWINSZ)`
   ทุก flush แล้ว `resizeterm()` (shim อ่านคีย์เองไม่ผ่าน `getch()` จึงไม่มี `KEY_RESIZE`)
3. **Export-first** — ช่องทางออกหลักคือไฟล์เอกสาร ไม่ใช่การพิมพ์
4. **UTF-8 เป็นเรื่องของ boundary** — ภายในยังใช้ TIS-620 semantics ของ CU Writer
   แล้วแปลงตอนเข้า/ออกไฟล์

## โครงสร้าง

```
port/shim/       header replacement สำหรับ Borland Turbo C 2.01
                 (dos.h bios.h conio.h dir.h process.h io.h graphics.h ...)
                 + shim_*.c  →  libtwshim.a
port/renderer/   ncurses backend แทน API ชั้น display (CWGRPHC.H)
                 →  libtwrenderer.a + test_renderer (headless, 41 tests)
port/notes/      รายงานวิเคราะห์ (เช่น dynamic-layout.md)
tools/           cw-convert — แปลง TIS-620 → UTF-8 → docx/odt/html/md/txt/pdf
tools/check-src-pristine.sh — guard กันการเผลอแก้ต้นฉบับ
SRC/             source ต้นฉบับ CU Writer 1.41  ← ไม่ถูกแตะ
```

## Build

```sh
make -C port/renderer      # renderer + test_renderer
make -C port/shim          # shim layer → libtwshim.a
make -C port/shim check    # compile-check source ต้นฉบับกับ shim headers
```

## Branch & remotes

```
fork      https://github.com/ake117/ThaiWrite      ← งาน port
upstream  https://github.com/kytulendu/ThaiWrite   ← ต้นฉบับ

master                  = ต้นฉบับ 100% ใช้เทียบ / ทำ PR กลับ upstream
port/linux-terminal   = งาน port (ต่อยอดจาก master)
```

## License

งาน port (`port/`, `tools/`, README, `LICENSE`) เผยแพร่ภายใต้ **The Unlicense**
สอดคล้องกับสถานะ public domain ของ CU Writer 1.41 ต้นฉบับ

---

## Credits

CU-Writer developers
--------------------
From CU-Writer 1.2 manual and CU-Writer 1.41 source code.

* สถาบันบริการคอมพิวเตอร์ จุฬาลงกรณ์มหาวิทยาลัย และ ภาควิชาวิศวกรรมคอมพิวเตอร์ คณะวิศวกรรมศาสตร์ จุฬาลงกรณ์มหาวิทยาลัย
(Computer Service Centre and Faculty of Engineering, Chulalongkorn University)
* สำนวน หิรัญวงษ์ 
(Sumnuan Hirunwong)
* ไกรศร เลียนษี
(Kraisorn  Liansee)
* ดำเกิง ธรรมเถกิงกิจ
* ทศพล อภิกุลวณิช
(Thotsapol Aphikulvanich)
* ธณัฎฐ์ จิระกาล
(Tanat Chirakarn)
* ประสาท โตปิตุมาตุคุณ
(Prasart Topitumatukhun)
* ลักษณ์ชัย ลือชัยชนะกุล
* วิชัย ปฏิภาภรณ์
(Wichai Patipaporn)
* วิทยา วงศ์อนวัช
(Wittaya Wonganawat)
* สุทธิพงษ์ คนาคกร
(Suttipong Kanakakorn)
* สุบรรณ เย็นใจชน
(Subun Yenjaichon)
* สุวพจน์ จันทโรจนวงศ์

If you know any one who is in CU-Writer project that isn't in this list,
please in inform me. :)

---
* **ThaiWrite / source 1.41** — [kytulendu](https://github.com/kytulendu) (Khralkatorrix):
  ปรับชื่อฟังก์ชัน `line` → `_line` ใน `COMMON/CSCR.C` เพื่อไม่ชนกับ BGI,
  แก้ `MAKEFILE`/`TURBOC.CFG`, เพิ่มไฟล์ฟอนต์ คู่มือ และเอกสารตัวอย่าง
* **งาน port เป็น Linux/CLI นี้** — งานแยกที่ *อ่านและ compile* source ต้นฉบับ
  โดยไม่ดัดแปลงเนื้อหาเดิม · รายละเอียดเพิ่มเติมที่ [`port/README.md`](port/README.md)
