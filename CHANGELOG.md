# Changelog

งาน port **CU Writer 1.41** ให้รัน native บน Linux/terminal (ncurses TUI)
โดย **ไม่แก้ไฟล์ต้นฉบับใต้ `SRC/` แม้แต่ไบต์เดียว**

รูปแบบอ้างอิง [Keep a Changelog](https://keepachangelog.com/th/1.1.0/)
เวอร์ชันใช้ `v0.000N` โดยเลขหลังจุดคือ **ลำดับ milestone** ไม่ใช่ semver —
โปรแกรมยังไม่ครบฟีเจอร์ของต้นฉบับ (โมดูล printing ยังไม่ทำ)

repo: <https://github.com/ake117/ThaiWrite> · branch `port/linux-terminal`
เวอร์ชันที่ build อยู่ดูได้จาก `cw --version`

---

## [Unreleased]

> รายการงานที่ยังไม่เสร็จฉบับเต็ม (พร้อมข้อจำกัดที่รู้ตัวและสิ่งที่ทดสอบแล้ว)
> อยู่ที่ [`README.md` → งานที่ยังไม่เสร็จ](README.md#งานที่ยังไม่เสร็จ-todo)
> — ที่นี่สรุปเฉพาะสิ่งที่รอเข้าเวอร์ชันถัดไป

### ค้างอยู่
- ทดสอบใช้งานจริงแบบเต็มรูปแบบ: พิมพ์ไทยต่อเนื่อง, บันทึก/เปิดไฟล์ซ้ำ,
  พิมพ์ออกทางเครื่องพิมพ์, เมนูย่อยทุกตัว (ทดสอบแล้วเฉพาะ render, startup,
  เมนูหลัก และออกโปรแกรม)
- โมดูล printing (`SCUP/PRINTING.C`) ยังไม่รวมเข้า build

---

## [v0.0002] — 2026-10-17

[Release](https://github.com/ake117/ThaiWrite/releases/tag/v0.0002) · tag `v0.0002`

milestone 2: พระเกี้ยว/splash screen สมบูรณ์ + แก้บั๊กที่เจอจากการใช้งานจริง

### เพิ่ม
- `cw --version` / `-V` และ `cw --help` / `-h` — จัดการใน
  `port/renderer/port_cli.c` ด้วย constructor (ทำงานก่อน `main()` ของ CW.C
  ซึ่งแยก argv แบบ DOS) และฝังเวอร์ชันตอน build จาก `git describe`
- `CW_DUMP_ATTR=<ไฟล์>` — ดัมป์ค่า attr รายเซลล์ของ shadow screen
  (ไว้ตรวจว่าเซลล์ไหนเป็น reverse video / ยังเป็นพื้นหลังเดิม)
- `test_renderer` เพิ่มเป็น **60 checks** ครอบ splash box (ตำแหน่งกลางจอ,
  แถบพระเกี้ยว, guard ของ message box) และ crest attr

### แก้ไข
- **เมนูบาร์หาย** — `headmenu()` (MENU.C) เขียนชื่อเมนูแถวบนแบบ reverse video
  แล้ว `_line()` ขีดเส้นทับแถวเดียวกัน · `render_cell()` เดิมให้ dot (เส้น)
  ชนะข้อความ ทำให้ชื่อเมนูหายทั้งแถบ และไอเทมเมนูแถวแรกที่ทับเส้นกรอบก็หาย
  ด้วย ⇒ เซลล์ที่มีตัวอักษรจริง (base ไม่ใช่ช่องว่าง/มีสระ-วรรณยุกต์) ให้
  **ข้อความชนะ** ส่วนเซลล์ว่างยังให้เส้นชนะ กรอบกล่องจึงยังต่อเนื่อง
- **ALT-X ออกแล้วใช้ terminal ต่อไม่ได้** — `shim_raw_mode()` บันทึก termios
  ตอนอ่านคีย์ครั้งแรก (หลัง ncurses ตั้ง cbreak/noecho) และลง `atexit`
  ของตัวเอง ซึ่งรันหลัง `endwin()` แล้วเอา noecho ใส่กลับให้ shell ⇒
  `setgraph()` เก็บ termios **ก่อน** `initscr()` + ลง `atexit` และ signal
  handler (TERM/HUP/QUIT) ที่ `endwin()` แล้วคืนค่าดิบ (รันเป็นตัวสุดท้าย)
- **แถบดำด้านบน/ล่างพระเกี้ยว** — แถบพระเกี้ยวกว้าง 7 แถว แต่ bitmap สูง
  5 แถว แถวที่ไม่มีหมึกค้างเป็นพื้นหลังดำ ⇒ เติมพื้นขาวครบทุกแถวของแถบ
  ก่อนวาดรูปทับ
- **กล่องดำคั่นตัวอักษร `C U  W R I T E R`** — ครึ่งหลังของอักษรขยาย
  (`ENLARGEATTR`) ไม่ถูกเขียนจึงค้างเป็นพื้นหลังเดิม ⇒ เติม blank ด้วย attr
  เดียวกัน
- **พระเกี้ยวไม่เหมือนต้นฉบับ** — เปลี่ยนเป็นอักขระ Braille (U+2800) 2×4 ดอท
  ต่อเซลล์ = 32×20 พิกเซลแบบ 1:1 ไม่ย่อ และแก้ลำดับไบต์ของ bitmap
  (`_hprakeaw` เขียนด้วย `mov es:[di],ax` ⇒ AL = ครึ่งซ้าย)
- **ขอบขวากล่อง splash ไม่เสมอ** — ช่องว่างท้ายสตริงถูกวาดด้วย attr ของกล่อง
  ทำให้ขอบยื่น 1 คอลัมน์ ⇒ ล้างช่องนั้นให้เสมอขอบ

### ปรับ
- กล่อง splash **จัดกลางจอทั้งแนวนอนและแนวตั้ง** ในพื้นที่ว่าง (ไม่นับ UI
  ด้านบน 4 แถว: ไทเทิลบาร์/บรรทัดสถานะ/ruler และเผื่อกรอบล่าง 1 แถว)
  ⇒ จอ 24 แถว กล่องอยู่แถว 9–17 · จอ 80 คอลัมน์ อยู่คอลัมน์ 10–69
  คำนวณจากขนาด terminal จริงทุกครั้ง (`port_visible_cols/rows()`)
- พระเกี้ยวย้ายเข้าไปอยู่ในกล่อง splash (ซ้ายสุด 16 เซลล์) แบบ **ดำบนพื้นขาว**
  ตามสไตล์กล่อง splash ของต้นฉบับ
- `.gitignore`: `images/` (ภาพสกรีนช็อตที่ใช้คุยกัน ไม่ใช่ของโปรเจกต์),
  `port/renderer/version.h` (สร้างตอน build)

---

## [v0.0001] — 2026-10-14

[Release](https://github.com/ake117/ThaiWrite/releases/tag/v0.0001) · tag `v0.0001`

milestone แรกที่รันใช้งานได้จริงบน terminal

### เพิ่ม
- **shim layer** (`port/shim/`) แทน DOS/BIOS API ที่ต้นฉบับเรียก: `bioskey`
  (แปลง escape sequence ของ terminal เป็น scan code แบบ DOS), เครื่องพิมพ์,
  BIOS tick clock, conio, dos, io, graphics, process, dir และ header หลอก
  ให้ compile ผ่าน (`-std=gnu89`)
- **ncurses renderer** (`port/renderer/`) แทน display layer ทั้ง 7 object
  (`edisp/hdisp/adisp/cwgrphc/cwegrph/cwagrph/cscrn`) ด้วย shadow screen
  แบบ cell (base + top/upper/below + attr) และ dirty-cell flush
- แปลง **TIS-620 → UTF-8 ที่ขอบ terminal เท่านั้น** (0xA1–0xFB → U+0E01–U+0E5B)
  และแปลงอักขระเส้นของ `NORMAL.FON` เป็น Unicode Box Drawing (ถ้าปล่อยไบต์ดิบ
  จะกลายเป็น C1 control ที่ terminal ไม่แสดง ⇒ กรอบหายทั้งเส้น)
- รองรับ resize (ถามขนาดจริงด้วย `ioctl TIOCGWINSZ` เพราะ shim อ่านคีย์เอง
  ไม่ผ่าน ncurses จึงไม่มี SIGWINCH)
- `tools/cw-convert` — แปลง `.DOC` (TIS-620) เป็น **docx / odt / html / md /
  pdf / txt** พร้อมแปลง dot-command (`.pt` → heading) และรหัสจัดรูปแบบ
- `tools/check-src-pristine.sh` — guard ตรวจว่าไม่มีไฟล์ใต้ `SRC/` ถูกแก้
- wrapper `~/.local/bin/cw` — `cd` ไปโฟลเดอร์ของไฟล์แล้วส่งชื่อแบบ relative
  เพราะ main ของ DOS ถือว่า argument ที่ขึ้นต้นด้วย `/` คือ option (พาธแบบ
  POSIX ทำให้ `cup_option[16]` overflow แล้ว core dump)
- เพลงจุฬาลงกรณ์มหาวิทยาลัยตอนเปิดโปรแกรม + `errorsound()` ผ่าน `aplay`
  (non-blocking) · ปิดได้ด้วย `CW_NOSOUND=1`

### แก้ไข
- **กรอบ/กล่องเป็นเส้นหนา (บล็อกเทา)** ⇒ วาดเป็นเส้นบางด้วยอักษร Box Drawing
- **เพลงจุฬาฯ เล่นยาว ~73 วินาทีตอนเปิด** (ไม่ compile `SOUND.C` ที่ spin ตาม
  BIOS tick — shim ให้ `soundnoise()/errorsound()` แทน)
- ตัวอักษรไทยไม่ขึ้น (locale ต้องถูกตั้งก่อน `initscr()` ไม่งั้น ncursesw
  ทิ้งไบต์ ≥ 0x80)

---

## รูปแบบเวอร์ชัน

`v0.000N` = milestone ที่ N ของงาน port (ไม่ใช่ semver เพราะยังไม่ครบฟีเจอร์)
เวอร์ชันที่ build อยู่มาจาก `git describe --tags --dirty --always` ตอน build

```sh
git tag -n9                      # ดูข้อความของทุก tag
gh release list -R ake117/ThaiWrite   # ดู release บน GitHub
cw --version                     # เวอร์ชันที่ binary นี้ build มา
```
