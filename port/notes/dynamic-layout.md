# T4: รายงานสแกน hard-coded layout ใน ThaiWrite SRC

วัตถุประสงค์: หาค่า 80 คอลัมน์ / 25 แถว / พิกัด layout ที่ตายตัวทั้งหมด เพื่อวางแผนทำ dynamic screen size
ขอบเขต: `/home/ake/cuwrite-linux-cli/thaiwrite/SRC` (COMMON, SCUW=cw.exe, SCUP=cuprint.exe, SCUF=font editor)

## 1. โมเดลหน้าจอปัจจุบัน — ทุกอย่างถูกตีความจากเซลล์อักขระ 8×20 พิกเซล

โปรแกรมทำงานใน **โหมดกราฟิกส์** (ไม่ใช่ text mode) แล้ววาดอักขระเองผ่าน font 8×20:
- ความกว้างจอ = 640 px → **ตายตัว 80 คอลัมน์ทุกโหมด** (640/8)
- ความสูงจอต่างกันตามการ์ดจอ → จำนวนแถวต่างกัน:
  - VGA 640×479 → 24 แถว (479/20)
  - AT&T 400 640×399 → 20 แถว
  - Hercules/EGA 640×347 → 17 แถว
- หน่วยพิกัดทั้งระบบ: **x = col*8, y = row*20** (พบกระจายทั่ว: SCUW/GRPH.C:29-30, SCUW/MENU.C:49, SCUP/SMENU.C:36, SCUP/PMENU.C:556-568, SCUF/FONTMSG.C:71,97,130, SCUF/POPUP.C:63-66, COMMON/CSCRN.C:275-301 คูณ 20 ตอน save/restore จอ)

สรุป: "80×25" ไม่ได้ถูกเขียนเป็นค่าเดียว แต่กระจายเป็น (a) 640/347/399/479 ตอนตั้งโหมด, (b) การคูณ 8 กับ 20 ทั่วโค้ด, (c) พิกัด col/row ตายตัวหลายร้อยจุด

## 2. จุดตั้งขนาดจอ / ขอบเขตพิกเซล (hard resolution)

| ไฟล์:บรรทัด | ค่า | ความหมาย |
|---|---|---|
| COMMON/CWEGRPH.C:26-38 | ax=0x11 (640×480 VGA), 0x10/0x0F (640×350 EGA) | ตั้งโหมดจอ |
| SCUW/CW.C:60 | `_rectangle(0,0,639,(VGA)?479:(ATT400)?399:347)` | กรอบจอ, แยก branch ตาม adapter |
| SCUP/PMENU.C:538 | `_rectangle(0,0,639,347)` | **ตายตัว 347 แม้ VGA** (cuprint ไม่รองรับจอสูง) |
| SCUF/FED.C:153-161,237 | 639, 347, setviewport(1,25,638,346) | กรอบ+viewport font editor |
| SCUF/POPUP.C:115-117 | `347/2 ± height*20/2` | กลางจอ hard-code 347 |
| SCUF/FONTMSG.C:36 | `center_x=320; center_y=348/2` | จุดกึ่งกลางจอ hard-code |
| SCUF/FED.C:223-224 | `center_x=320+align; center_y=347/2` | เช่นเดียวกัน |
| COMMON/CWEGRPH.C:64 | `return(80*y + x/8)` | offset ใน VRAM: 80 bytes/row |
| COMMON/CWAGRPH.C:43 | `0x2000*(y%4) + 80*(y/4) + x/8` | AT&T interleave, 80 bytes/row |
| SCUW/GRPH.C:31-38 | `y>=344`, `endy=344` | เพดานวาดกราฟิกในบรรทัด (344px ≈ 17 แถวของ Herc) |
| SCUF/FED.C:556 | `640/8/BYTE` | คอลัมน์ต่อจอใน font editor |

ASM (ADISP/EDISP/HDISP) มี scanline offset (เช่น `es:[di+180]` ใน HDISP.ASM) ผูกกับ 80 bytes/row เช่นกัน — port ใหม่ไม่ต้องแก้แต่ต้องแทนที่ชั้นนี้ทั้งก้อน

## 3. ขนาดหน้าต่างแก้ไข (SCUW) — ค่าเดียวที่ตั้งได้ตาม adapter

`SCUW/CWSETUP.C:129-132` — **จุดเดียวที่ dynamic อยู่แล้ว:**
```c
wind.row = 4;      /* แถวบนของพื้นที่แก้ไข: 0=ชื่อไฟล์, 1=เมนูแถบ, 2=status, 3=ruler */
wind.col = 1;
wind.length = 78;  /* จำนวนคอลัมน์พื้นที่แก้ไข: 80-ขอบซ้าย1-ขอบขวา1 */
wind.width = (scrmode == VGA) ? 19 : (scrmode == ATT400) ? 15 : 13;  /* จำนวนบรรทัด */
```
- `struct windowtype` (SCUW/VAR.H) มี row/col/length/width ครบ → **โครงสร้างนี้พร้อมใช้ทำ dynamic** ถ้าทุกจุดอ้าง wind.* แทนเลขตายตัว
- ใช้ `wind.*` แบบ relative อยู่ 92 จุด (EDIT.C 42, ONSCRN.C 12, CW.C 8, SEARCH.C 8, MOVEMENT.C 6, MOVE.C 5, INS.C 5, GRPH.C 2, WRAP.C 1, SCRN.C 1) — ดี แต่ยังผสมกับเลขตายตัวด้านล่าง

ข้อจำกัดเชิงตรรกะ (ไม่ใช่ 80 คอลัมน์แต่คู่กัน):
- `SCUW/CONST.H`: `MAXCOL 266`, `MAXRMAR 256` — คอลัมน์ตรรกะเกินจอ, เลื่อนจอด้วย `firstcol` (horizontal scroll) → dynamic ต้องคง model นี้ไว้
- buffer `struct Workline` ขนาด `MAXCOL+3` ผูกกับ MAXCOL
- margin: `rightmar` default 65 (option "rightmargin" CWSETUP.C:180), `leftmar` 1, `lineperpage` 33 — เป็นค่าเอกสาร/ตั้งได้ ไม่ใช่ของจอ

## 4. พิกัด row/col ตายตัวใน SCUW (ที่ต้องกลายเป็น relative)

### 4.1 แถบสถานะแถว 0 และ 2 — `SCUW/STATUS.C` (ทั้งไฟล์แทบทุกบรรทัด)
- แถว 0: ไดรฟ์ col 50-`SHRINK_FACTOR`, ชื่อไฟล์ col 58/68-`SHRINK_FACTOR`
- แถว 2 (status): หน้า col 4/8, บรรทัด col 11/17, คอลัมน์ col 21/27, โหมดใส่ col 33, ภาษา col 41, attr col 49, "บันทึกขอบ" col 62
- SHRINK_FACTOR=10, CENTER_FACTOR=5 มาจากการแปลงจอ 90→80 คอลัมน์ (`SCUW/CONVERT.H:15-22`) — **คือค่าชดเชย legacy ที่ควรลบทิ้งตอน refactor**

### 4.2 แถบเมนู — `SCUW/MENU.C`
- `xlev1[] = {1,9,16,24,31,36,41,49,55,63}` (บรรทัด 356-362) ตำแหน่งหัวเมนู 10 ชั้น, ผูกกับความกว้างข้อความไทยตายตัว
- `headmenu()` (บรรทัด 441-501): `_line(0,24,80,24)`, `_line(0,43,80,43)` และคู่ 72-136, 128-200, …, 504-639 — เส้นใต้เมนู hard-code ทั้งชุด **และมี 2 รุ่นตาม WANT_TO_USE_GRAPH (จอ 80 กับ 90 คอลัมน์)**
- เมนู dropdown ใช้ `savescrn/resscrn` กับพิกัดตายตัว เช่น บรรทัด 834-855 `savescrn(19,4,...)`, 950-955 `savescrn(16-CENTER_FACTOR,5,...)`

### 4.3 กล่อง dialog กลางจอ (สมมติจอกว้าง ~80, แถวสูงสุด ~25)
ใช้รูปแบบ `xx - CENTER_FACTOR` (= ชดเชยจากจอ 90 col) ทั่วไป:

| ไฟล์ | ตัวอย่าง | ความหมาย |
|---|---|---|
| SCUW/CW.C:56-75 | splash `21-CENTER_FACTOR`, แถว 5-13 | หน้าจอเปิดโปรแกรม กล่องกว้าง 44 col ตรึง row 5-13 |
| SCUW/FILE.C:22-29 | `savescrn(18-CF,9,71-CF,11)` | กล่อง save |
| SCUW/ONSCRN.C:98-192 | `framebox(25-CF,4,25-CF+39..41,6)` | margin/lineperpage 3 กล่อง ตรึง row 4-6, prompt row 10 |
| SCUW/MOVE.C:219-244 | `framebox(30-CF,4,...+29,6)`, `(22-CF,4,...+49,6)` | goto page/line |
| SCUW/SEARCH.C:31-98 | col `29-CF`, `51-CF`, row 5-7 | search/replace |
| SCUW/MENU.C:602-675 | row 10, col `22-CF`, `27-CF` + `blockmsg(10)` | ข้อความแจ้ง |
| SCUW/GRPH.C:67-70, BLOCK.C, INS.C, GETSTR.C:223 | `blockmsg(10)` กลางจอ row 10, prompt row 11 | กล่อง message ทุกที่ |
| COMMON/CSCRN.C:241 | `blockmsg(y): framebox(13,y-1,13+53,y+1)` | **กล่องกว้าง 55 col ตรึงซ้าย col 13** — ใช้ทั้ง 3 โปรแกรม |
| COMMON/DIR.C:66,101,135,147 | col*13+9, row+9, `framebox(4,8,74,14)` | browser ไดเรกทอรี: 6 คอลัมน์ × 13 ช่อง |

### 4.4 ค่า row ตายตัวที่ผูกกับโครงจอ (25 แถว legacy)
- row 0 = ชื่อไฟล์/ไดรฟ์, row 1 = เมนูแถบ, row 2 = status, row 3 = ruler (`writetab` ONSCRN.C) — **ตรึง wind.row=4 ไว้แบบไม่มีค่าคำนวณ**
- `SCUW/GRPH.C:30`: `y = y*20 + 84` — 84 = 4 แถวบน × 20 + 4 → ผูกกับ wind.row แบบ hard-code

## 5. SCUP (cuprint.exe) — แย่กว่า: ตายตัวทั้งหน้าจอ

- โครงหน้าจอ `SCUP/PMENU.C:538-575`: กรอบ `639×347`, เส้น `y=24`, หัวเรื่อง col 6/25/31, `_rectangle(mcol*8-2, 62, ..., 225)` — กรอบเมนู y 62-225 ตายตัว
- เมนูทั้งหมด `writemenu()` วาง col `mcol[0..2]` (3, +mlenght+1, …) row 3-15 ตายตัว
- ทุก input ใช้ row **12** (`getname(...,49/52/57/59/62/64,12,...)`, PMENU.C บรรทัดราว 205-400) และ row 13 สำหรับแสดงผล
- `SCUP/PRINTING.C:85-225`: แถบควบคุมการพิมพ์ row 12-13, col `10-scol`…`58-scol` (scol=7), บรรทัดตัวอย่าง `dispprintf(2,13,"%-77.77s")` — **77 ตัวอักษรตายตัว**
- ค่าพิมพ์ (ไม่ใช่จอ แต่เกี่ยวข้อง): `maxcol=80` (GLOBAL.C:125), `maxpaper=80:136` (PRINTING.C:435), `lineperpage=33`
- SCUP ไม่มี wind-like struct เลย — **ต้องรื้อทั้งโมดูล layout**

## 6. SCUF (font editor) — วางบน canvas 640×347 ตายตัว
- จุดกลาง `center_x=320`, `center_y=347/2` (FED.C:223, FONTMSG.C:36)
- popup: `menu_left = center_x - width*8/2 - 6`, `menu_top = 160 - h*20/2` (POPUP.C:63-66 รุ่นหนึ่ง) และ `347/2 ± ...` (รุ่นสอง) — **สองสูตรปนกันในไฟล์เดียว**
- viewport แก้ฟอนต์ `setviewport(1+align, 25, 638+align, 346)` (FED.C:237), แถบสถานะ INT.C row 16, col 50
- `align`/`herc_align` คือค่าชดเชย Hercules อีกชั้นที่ควรตายไปพร้อม DOS target

## 7. สรุปเชิงแผน: ทำ dynamic screen size

สิ่งที่ช่วยไว้แล้ว:
1. ทุก output ผ่าน indirection table `prchar_ptr/prblank_ptr/…` (CWGRPHC.C:100-176) และ `dispstrhgc/dispprintf/framebox/savescrn` — **มี choke point เดียวสำหรับวาดจออยู่แล้ว (COMMON/CSCRN.C + CWGRPHC)**
2. `wind{row,col,length,width}` มีอยู่และถูกใช้ 92 จุด
3. เจตนา "scroll แนวนอน" มีอยู่แล้วผ่าน `firstcol` → จอแคบกว่า 80 ทำได้โดยไม่แตะโครงสร้างข้อมูล (MAXCOL=266 คือขีดจริงของบรรทัด)

งานที่ต้องทำ (เรียงตามผลตอบแทน):
1. **เพิ่ม global `screen_cols` / `screen_rows`** (คำนวณจาก detect จริง: จาก scrmode หรือ getmaxx()/8, getmaxy()/20 ตอน port) แล้วตั้ง `wind` จากค่านี้แทนค่าตายตัวใน CWSETUP.C:129-132
2. **แทน CENTER_FACTOR/SHRINK_FACTOR ด้วยการจัดกึ่งกลางจริง**: เขียน helper เช่น `centered_box(width, height)` / `msgbox(y)` ใน COMMON แล้วแทน call ทั้งหมดที่อยู่ในตาราง 4.3 — blockmsg (CSCRN.C:241) คือจุดเริ่มที่คุ้มที่สุด เพราะแก้ที่เดียวกระทบ 3 โปรแกรม
3. **แถบ status/เมนู (SCUW/STATUS.C, MENU.C)**: เปลี่ยน col ตายตัวเป็น layout จากขวา (filename อยู่ท้ายแถว 0, relmar ชิดขวา row 2) และให้ headmenu วาดตามความกว้างจริงของ `thaistrlen()` + คำนวณ xlev1[] ตอน runtime
4. **เส้นขอบจอ**: แทน 639/347/399/479 ด้วย `getmaxx()/getmaxy()` (มีฟังก์ชันอยู่แล้วใน BGI และ POPUP.C:55 ใช้แล้ว) — PMENU.C:538 ที่ตายตัว 347 ต้องแก้ก่อนอื่นใน SCUP
5. **row ตายตัว 12/13/10 ใน SCUP/SCUF**: ให้เป็น `screen_rows-2/-1` และจัดกึ่งกลางด้วย helper เดียวกับข้อ 2
6. **กำจัดค่าชดเชย legacy**: CENTER_FACTOR=5, SHRINK_FACTOR=10, `align/herc_align` — ลบพร้อมกับ refactor เพราะอ่านสับสนและผูกกับจอ 90/80 col ของยุกต์ก่อน
7. **ข้อจำกัดที่ต้องยอมรับ**: สัดส่วนเซลล์ 8×20 ฝังใน font และ ASM renderer; ถ้า port ใหม่รักษาเซลล์ 8×20 ต่อไป dynamic จะหมายถึง "จำนวน row/col เปลี่ยนตาม terminal" เท่านั้น ซึ่งเพียงพอสำหรับ terminal 80×N ถึง 200×60

ขนาดจออ้างอิงสำหรับทดสอบ: 80×17 (Hercules/EGA, แย่สุดที่โค้ดรองรับวันนี้), 80×24 (VGA), 80×20 (AT&T) และเป้าหมายใหม่ ≥80×25 / >80 col

## ภาคผนวก: รายการตำแหน่งที่พบ (สรุปตามไฟล์)

- SCUW/STATUS.C: ทั้งไฟล์ ~20 จุด col ตายตัว (แถว 0 และ 2)
- SCUW/MENU.C: xlev1[] 10 ค่า, headmenu 12 คู่ _line, savescrn/พิกัด dropdown ~25 จุด, dialog row 10 หลายจุด
- SCUW/CW.C:60,56-75: กรอบ 639×{479,399,347}, splash 44×9
- SCUW/ONSCRN.C: 3 dialog (row 4-6, 10), blockmsg(10)
- SCUW/SEARCH.C: 6 จุด (col 29/51-CF, row 5-7)
- SCUW/MOVE.C: 2 dialog (row 4-6)
- SCUW/FILE.C: 2 dialog (row 9-11, row 10)
- SCUW/BLOCK.C: blockmsg 5/10/11 ×4
- SCUW/GRPH.C: 25-CF row 10, y*20+84, เพดาน 344
- SCUW/INS.C, GETSTR.C, CWSETUP.C: dialog row 4-6/10
- COMMON/CSCRN.C: blockmsg กล่อง 55 col, showerrno row 10-11, savescrn*20
- COMMON/DIR.C: framebox(4,8,74,14), col*13+9, row+9
- COMMON/CWEGRPH.C/CWAGRPH.C: offset 80/row
- SCUP/PMENU.C: กรอบ 639×347, เมนู y=62-225, row 12 เป็นหลัก, mcol[] คอลัมน์เมนู
- SCUP/PRINTING.C: row 12-13, col 3-58 (scol=7), %-77.77s
- SCUP/SMENU.C: _rectangle x*8-2, y*20+2 ตายตัว
- SCUP/GLOBAL.C: maxcol=80, lineperpage=33, rightmargin=80 (ค่าพิมพ์ — แยกจากจอ)
- SCUF/FED.C, POPUP.C, FONTMSG.C, INT.C: center 320/174, viewport (1,25,638,346), row 16, popup สองสูตร
