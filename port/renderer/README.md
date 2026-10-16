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
- The Chulalongkorn crest (`prakeaw`) blits the real 32x20 DOS bitmap
  (`chulalogo1`/`chulalogo2`, HDISP.ASM:152,157). A terminal cell can only
  show 2x4 dots, so it is downsampled to four Braille cells (8x4 dots) —
  a dot is on when its 4x5 pixel block is >= 40% ink. `port_cell.over`
  holds the dot mask and renders as `U+2800 + mask`.
- super/subscript and enlarge are recorded per cell but rendered as
  normal glyphs (terminals cannot shift a single cell).
