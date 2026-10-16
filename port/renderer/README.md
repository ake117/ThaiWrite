# ThaiWrite display-layer port (ncurses backend)

Port of the CU-Writer 1.41 display layer (`SRC/COMMON/CWGRPHC.H`,
`CWGRPHC.C`, `EDISP.ASM`/`HDISP.ASM`/`ADISP.ASM`, `CSCRN.C`) to a
cell-based ncurses backend. Nothing under `thaiwrite/` is modified.

## Files

| file | ports | notes |
|---|---|---|
| `cwgrphc.h` | `CWGRPHC.H` | same macro names/arg order, same `font_attr` bits; adds `port_cell`, `port_flush`, `port_getch`, `whatlevel` |
| `cwgrphc.c` | `CWGRPHC.C` + `E/H/ADISP.ASM` + `CWFNTM.ASM` + `whatlevel` leveltable | one backend; all `h*/e*/a*` adapter aliases kept |
| `cscrn.h`/`cscrn.c` | `CSCRN.C` | `dispstrhgc`, `dispprintf`, `dispblank`, `togglefont`, `framebox`, `blockmsg`, `savescrn`, `resscrn` |
| `test_renderer.c` | — | smoke test for every entry point |

## Build

```
make            # builds libtwrenderer.a and test_renderer
make test       # runs headless assertions
./test_renderer # interactive run (needs a UTF-8 terminal)
```

## Model

The DOS originals painted an 8x20-pixel bitmap cell grid with
XOR-blitted Thai vowel/tone overlays. Here each cell keeps its overlay
state explicitly (`port_cell`: `base` + `top` + `upper` + `below` +
`attr` + coarse `dot[3]` for `plot`), a dirty-flag layer pushes cells
to ncurses, and `savepic`/`retpic`/`getwind`/`putwind` keep the exact
DOS byte-buffer sizes (20 bytes per scanline record).

## Known divergences

- `clsgraph` argument order is **(x1, y1, x2, y2)**. The original
  typedef *names* them `(x1, x2, y1, y2)` but every implementation and
  caller uses (x1, y1, x2, y2); the port documents the real order.
- Tone/vowel overlays on a cell that has a base glyph cannot stack on
  one ncurses cell; the base glyph wins visually, overlays stay in the
  cell state (so buffer round-trips are lossless).
- `hsetgraph/esetgraph/asetgraph` are no-ops: terminal setup lives in
  `setgraph()`; `settext()` in the port is `backend_settext_and_end()`.
- The Chulalongkorn crest uses the real 32x20 DOS bitmap
  (`chulalogo1`/`chulalogo2`, HDISP.ASM:152,157) but the port draws it
  **inside the splash box**, not the title bar: `port_draw_crest()` writes
  16x5 Braille cells = 32x20 dots, one dot per bitmap pixel (no
  downsampling). Dot masks live in `port_cell.over` and render as
  `U+2800 + mask` with the cell's attr (the splash draws it REVERSEATTR =
  black on white). `dispstrhgc` only intercepts the splash rows at their
  original column (col 16 = `21 - CENTER_FACTOR`, rows 5-13) — a plain
  `REVERSEATTR` message box elsewhere (MENU.C draws one at col 45) must
  stay untouched. It widens the box 16 columns to the left (44 -> 60) and
  shifts the whole box to the centre of the visible area
  (`splash_base()` = (`port_visible_cols()` - 60) / 2), so the crest sits
  inside the border on any terminal width. It also fills *every* interior
  row of the crest band with a same-attr blank before drawing the crest —
  the band covers rows 6-12 while the bitmap is only 5 rows tall, and the
  rows above/below the logo otherwise keep the old background and read as
  black bars. Same-attr blanks go into the second column of every
  ENLARGEATTR glyph for the same reason. `prakeaw()` is a no-op.
- super/subscript and enlarge are recorded per cell but rendered as
  normal glyphs (terminals cannot shift a single cell).

## Debugging

| env var | effect |
|---|---|
| `CW_DUMP_SCREEN=<file>` | dump the shadow screen as text (one line per row, `#` for `plot` dot cells, overlays appended) |
| `CW_DUMP_DOT=<file>` | dump the coarse dot bitmap of every cell (`y= x= d0= d1= d2=`) |
| `CW_DUMP_ATTR=<file>` | dump every cell's `attr` byte as 2 hex digits — the way to tell a *white* cell (`attr & 0x02`) from one that kept the background |
| `CW_DEBUG_FLUSH` | print terminal size / dirty-cell counts per flush to stderr |

`CW_DUMP_ATTR` is what caught the black bars above and below the crest:
rows 6 and 12 of the crest band had `attr 00` while the rest of the box
was `02` (REVERSEATTR).
