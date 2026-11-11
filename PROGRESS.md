# Progress — PS5 port of Boxshelf

Read `PS5-PORT.md` first. This file is the log the owner and the monitoring assistant read. Append, do not rewrite
history. Newest entry at the bottom.

## Phase checklist

- [x] P0 Foundations (repo layout, build, preview harness, deploy script, native-vs-web decision) — PC only; not yet run on the console
- [ ] P1 Data (library roots, per-system styles, cover pipeline) — data layer + tests done on the PC; **open until the
      owner's real library is listed from the console** (`make library PS5_HOST=<ip>`), and the cover tools land
- [ ] P2 The shelf, 1:1 (scene, top bar with system tabs, backdrop, info, hints, input) — **owner checkpoint on the console**
      — scene/layout/input done and matched against Boxshelf in the PC preview; sort/filter panel, Menu and the
      Spectrum theme's moving layer still to do; **waiting for the console check**
- [ ] P3 Cases, 1:1 (per-system anatomy) — **owner checkpoint on the console** — real covers from the console done
      (PC preview); spines with band and title, glare, sheen, floor strips, backs still to do
- [ ] P4 Details / inspect
- [ ] P5 Museum
- [ ] P6 Settings, themes, sounds
- [ ] P7 Launch + polish

## Decisions

### D1 (2026-11-10) — Route A: native C++/OpenGL, not an embedded web engine
Evidence (spike, PC side only; the console was not reachable that day):
- The pinned PS5 payload SDK (`ps5-payload-dev/sdk` v0.42, `target/lib`, 49 stubs) has no WebKit / WebView /
  browser module stub and no header for one; the only web-related stubs are `libScePosixForWebKit` (a POSIX shim)
  and `libkernel_web`. ps5-opengl 1.0.2 has none either.
- Estante's notes (`estante-ps5/docs/CURL.md`, `src/runtime/runtime_shims.c`) record that a native title does
  **not** load `libScePosixForWebKit`: symbols bound to it resolve to address 0 at run time. Hosting the system's
  WebKit from a homebrew title would mean loading undocumented system modules by hand, with no input or
  compositor integration and (for a non-browser process) no JIT, for a 4 300-line JS + CSS-3D renderer.
- Route A is already proven on this console model/firmware by Estante (GL renderer, 3D cases, 60 fps).
Decision: **A**. Parity is built by hand in `ps5/src/app/`, measuring against Boxshelf running in a browser with
`renderer/mock.js`.

### D2 (2026-11-10) — Title id PPSA99850
Not used by Estante (99841), Trophy Manager (99842) or the emulators in PS5-PORT.md §5 (checked by `make test`).
**Not yet checked against the console's `/appdb`** (console was offline): do that before the first deploy.

### D3 (2026-11-10) — ps5-opengl SDK source
github.com/blackbearreloaded (ps5-opengl's upstream) answers 404 since at least 2026-11-10. `tools/fetch-opengl-sdk.sh`
now tries the upstream URL and then three public re-uploads; the archive is accepted only if it matches the
SHA-256 pinned by Estante (`30d8e51d…`) and its manifest matches (`0d432edc…`). The KernelForgeLabs re-upload
matched both, so the SDK is bit-identical to the one Estante builds with.

## Deviations from Boxshelf

- **Font (temporary):** the engine's baked Inter Medium is used until P2 bakes Boxshelf's own OFL fonts.
- ~~Vignette (temporary)~~ fixed in P2: the batch shader has a CSS radial/linear gradient shape (`Shape::radial`).
- **Font:** Boxshelf's own fonts are used since P2 (Lexend 500/600/800 and the wordmark faces, converted from its
  OFL woff2). The 3DS wordmark asks for Unbounded 800; Boxshelf only ships 700, which is what a browser draws too.
- **Wordmark gradients at 95deg** (Switch, Switch 2, Media) are drawn top-to-bottom; the glow (CSS drop-shadow
  16px) is a soft shadow box behind the word.
- **"♥" in the hint bar** is drawn as a shape (no baked face has the glyph; Boxshelf takes it from the system font).
- **Hint bar** shows the controller version only (the PS5 has no keyboard prompts). The ⧉ button is the DualSense
  Create button, which the system keeps for itself: Settings / Museum will need another button (decide in P6).
- **Clock:** Portuguese date like the owner's console; time zone from system params 4/5 (unconfirmed on PS5, logged).
- **Covers / spines / glare / sheen / per-case floor strips:** placeholders until P3 (cover in the game's hue with
  the system band; no spine text yet).
- **Home shelf** is left out while it would be empty (needs play history, P7).
- **Spectrum theme moving layer** (ribbons and dust, `themefx.js`) not drawn yet: only the colour wash (P6).

### D4 (2026-11-10) — Shelves and where each game comes from
- Shelf order = Boxshelf's (Home, main/systems.js CONSOLES order, retro rows, Media last). Shelves for systems a PS5
  emulator runs (GC, Wii, Wii U, Switch, PS2, 3DS, PS1, DS) and PS5 always exist (Boxshelf puts an "Add games" tile
  on an empty console shelf); PS4 / Media / PSP / PS3 / X360 / Switch 2 only when they have something.
- PS5 / PS4 games come from Elf Arsenal `/appdb` (PPSA → PS5, CUSA → PS4). Homebrew ids (PPSA9xxxx: Estante, the
  emulators, tools) go to the **Media** shelf, Boxshelf's shelf for apps; this app itself is left out.
- Emulator games: the HD layout of §5, the emulators' own folders (Estante's `*_roots`) and the folders in their
  configs (PS5-ECAM json, PS5SX2 gs.ini, SwanStation ini; formats unconfirmed, see §5). Same path twice = one game.
- Porpoise's mixed folder: GameCube vs Wii by extension until the disc id (disc-cache.tsv) says otherwise.

### D5 (2026-11-10) — Shelf screen rules taken from Boxshelf
- Measured against Boxshelf's renderer running in headless Chrome with `renderer/mock.js` at a true 1920x1080
  viewport (headless defaults to 1904x985: window 1936x1175 gives 1920x1080) and transitions switched off so the
  capture is the settled layout (`ps5/.local/ref/wrap.html`, not committed).
- `setup.css` overrides `style.css` for the shelf and is what Boxshelf shows: title strip at 42% + bh/2 + 8vh, halo
  2.6 bw x 1.9 bh without blur, floor ellipses 18%/26% and 55%/34%, no white rim on the selected case (inner glow),
  tab bar tinted by the selection glow, hint bar rgba(10,12,22,.78).
- Controller mapping = Boxshelf's PAD_HINTS: L2/R2 system, L1/R1 letter, △ flip (hold ♥), □ details, ✕ play,
  R3 inspect, L3 shuffle, ⧉ settings (hold museum), ☰ menu (hold sort). D-pad up/down also switch system (keyboard ↑↓).
- Shelf order of games = Boxshelf's scanner (`Intl.Collator` base sensitivity, numeric): case-insensitive, natural numbers.

## Notes from the owner (2026-11-10)
- Estante's GameTDB tool crops everything to 2:3. This port needs each system's own cover shape (3DS landscape):
  `data::cover_spec()` gives it per system (unit-tested), and the cover tool must use it.
- PS5 / PS4 covers only look right with the store's portrait art, which comes from Estante's public
  `tools/ps5_covers_sync.py`.
- `emulator_covers_sync.py`, the newer `emulator_import.hpp` (Switch update/DLC detection) and their two tests
  exist only on the owner's other PC; handoff pending (owner decides: branch on the fork, or copied by hand to
  `handoff-from-estante` on the desktop). Until then this port uses the public Estante (`0077695`) rules.

## Open questions for the owner

1. ~~Is PPSA99850 free?~~ Owner: not used by any of his apps. Still to confirm read-only on `/appdb` when the console is on.
2. ~~Folder~~ Owner: `C:\Users\Kauan\Documents\PS5App\` is fine.
3. ~~emulator_covers_sync.py~~ Owner: exists only on his PC, handoff pending (see Notes).

## Log

## 2026-11-10 — setup — repository created, brief written
**Done:** fork of `sassiestrhihno/boxshelf-source` prepared with `PS5-PORT.md` and this file. No code written.
**Verified on:** nothing yet.
**Next step:** read `PS5-PORT.md`, then start phase 0.

## 2026-11-10 — P0 — foundations: engine copied, empty scene, app folder builds
**Done:**
- Cloned Estante read-only to `C:\Users\Kauan\Documents\PS5App\estante-ps5` (commit `0077695`); nothing committed there.
- WSL toolchain installed (`clang-18 lld-18 make cmake ninja-build ccache unzip` + Mesa EGL dev packages, via
  `wsl -u root`, no password needed). `tools/doctor.sh`: all required tools OK except clang-format/clang-tidy (lint only).
- `ps5/`: Estante's engine and build system (gfx, core, platform, runtime, GL canvas, 3D case pass, build/deploy
  tooling, libc builder) **without** its UI (`main.cpp`, `experience`, `i18n`, `library`/importer are left for P1+).
  Renamed engine identifiers: `namespace estante` → `boxshelf`, `ESTANTE_*` macros → `BOXSHELF_*`, log tag
  `[BOXSHELF]`; file headers say "engine from Estante".
- New: `src/app/scene.{hpp,cpp}` (empty Boxshelf backdrop: `#07070c`, vignette, floor shadow, sizes from
  style.css), `src/main.cpp`, `tools/preview-boxshelf.cpp` + `tools/preview-boxshelf-gl.sh` (same scene code on
  Mesa EGL → PNG), `tools/test-boxshelf.sh`, trimmed `Makefile` (app / preview / test / deploy / doctor / clean).
- `sce_sys/param.json`: PPSA99850, "Boxshelf", 01.001.000. `icon0.png` = Boxshelf's `renderer/app-icon.png`
  upscaled to 512×512 on `#07070c`. Estante's pic0/pic1/snd0/background art removed.
- Decisions D1–D3 above (native route, title id, SDK mirror verified by pinned hash).
- Console: scanned 192.168.2.0/24 for :6969 read-only; no host answered (PS5 off/asleep). Nothing sent to it.

**Files:** `ps5/**` (new), `PROGRESS.md`.

**Tests run (WSL, in `ps5/`):**
- `make preview` → `.local/gl-empty.png` rendered (Mesa llvmpipe).
- `make app` → `dist/PPSA99850/` + `dist/PPSA99850.zip`; eboot.bin and libc.prx `integrity: valid`.
- `make test` → param.json ok, assets validated, preview smoke ok.
- `make deploy PS5_HOST=192.168.2.120 DEPLOY_DRY_RUN=1` → "Would publish 7 files", no network request.

**Verified on:** PC preview + build only. **Not run on the console.**

**Screenshots:** `ps5/.local/gl-empty.png` (dark backdrop with floor shadow; vignette banding noted in Deviations).

**Deviations:** temporary font and vignette approximation (see "Deviations from Boxshelf").

**Open questions for the owner:** 1–3 in the section above.

**Next step:** P1 — port Estante's importer (`library.*`, `emulator_import.hpp`, roots from §5) into a unit-tested
data layer; per-system style table read from `renderer/app.js` (`STYLE`, incl. PS1/DS); cover pipeline plan.

## 2026-11-10 — P1 — data layer: styles, scanner, shelves, cover geometry (PC only)
**Done:**
- `src/data/systems.*`: Boxshelf's case style table, copied value by value from `renderer/app.js` STYLE + WORDMARKS
  and `renderer/systems-extra.js` (psx = jewel case, landscape 1.136, no band; nds 0.9). 16 shelves incl. Home and
  Media. `case_size()` = app.js `place()` sizing.
- `src/data/scan.*`: per-system file rules (main/systems.js extensions narrowed to what each PS5 emulator opens),
  Switch base-vs-update/DLC guard (16-hex title id: base ends 000), GC/Wii by disc id, title cleaning (Estante's
  `clean_rom_title`, plus `_` → space), stable ids, default roots (HD + emulator folders), config parsers, Elf
  Arsenal `/api/fs/list` + `/appdb` parsers, `build_shelves()` (D4).
- `src/data/covers.*`: `cover_spec()` (covers.js geometry: 420 × round(420/ratio), band 8.5 %, face aspect inside the
  frame) and `CoverCache` (LRU by count and bytes; the newest cover always stays).
- `tools/fetch-listings.py` (read-only GETs: `/appdb`, `/api/fs/download` of the 5 emulator configs, `/api/fs/list`
  per root) + `tools/list-library.cpp` (the app's own data code over the saved listings) → `make library PS5_HOST=<ip>`.

**Cover pipeline plan (P1 → P3):**
1. Sources, best first: (a) owner's PC tools write art to the console — PS5/PS4 store portrait art
   (`ps5_covers_sync.py`), emulator games GameTDB front + back (`emulator_covers_sync.py`), both cropped to
   `cover_spec(system).face_aspect`, not 2:3; (b) the emulators' own covers already on the console (SwanStation
   `covers/boxart`, Porpoise `covers/<discid>.png`, PS5SX2 `covers/<serial>.jpg`, ProsperoEden `covers/<hash>.tga`);
   (c) composed cover in the system's style (covers.js `drawFront`/`iconFront`: band + art/icon + title), never a
   small square on a blur (lesson 2).
2. Decode with Estante's `cover_decode.c` (stb) on a worker thread, at the size the slot needs; selected case at
   full size, neighbours smaller.
3. `CoverCache`: start at 24 covers / 96 MB, tune on the console in P2.

**Files:** `ps5/src/data/{systems,scan,covers}.{hpp,cpp}`, `ps5/tests/test_data.cpp`, `ps5/tools/{list-library.cpp,
fetch-listings.py,test-boxshelf.sh}`, `ps5/Makefile`, `PROGRESS.md`.

**Tests run (WSL, `ps5/`):** `make test` → "data tests: 171 checks, 0 failed" (ASan + UBSan), "list-library ok"
(made-up listings: 4 roots, PS2 / Switch folder / 3DS / Wii U from a config root / PS5 + Media from appdb),
preview ok. `make app` → builds with the data layer, `integrity: valid` (cJSON's vendored isnan warnings only).

**Verified on:** unit tests + PC tool on made-up listings. **Not on the console; the owner's real library has not
been listed yet** (console offline all session).

**Deviations:** none new.

**Open questions for the owner:** none new (handoff choice is his, see Notes).

**Next step:** when the console is on: `make library PS5_HOST=<ip>` (read-only), check `/appdb` for PPSA99850, fix
rules against the real listing, close P1. Meanwhile start P2 (shelf scene, `place()`, top bar) in the preview with
made-up games.

## 2026-11-10 — P2 — the shelf screen, matched against Boxshelf in the preview
**Done:**
- Reference captures of Boxshelf itself (renderer + mock.js in headless Chrome, settled, 1920x1080; see D5).
- Engine: CSS perspective camera in `gl3d` (screen = o + (p − o)·d/(d − z); the old 30° view is the default),
  72 3D items per frame, `Shape::radial` CSS radial/linear gradients in the 2D batch, `DrawList::recolor()` for
  gradient text.
- Fonts: Boxshelf's Lexend 500/600/800 + the per-shelf wordmark faces (Chakra Petch, Syne, Unbounded, Michroma,
  Audiowide, DM Serif Display italic, Bungee), converted from its OFL woff2 → `third_party/boxshelf-fonts`,
  baked by `tools/bake-fonts.sh`; licences copied next to the fonts and into the app's assets.
- `src/app/`: `css.hpp` (hsl, FNV-1a over UTF-16 = covers.js `hash`, CSS cubic-bezier, transition glides),
  `shelf.*` (prepare(): Surprise me / Add games / Open <emulator> tiles in Boxshelf's order; go / letter / setSystem;
  `place()`), `scene.*` (Spectrum wash with 0.8 s cross-fade after 120 ms, vignette, floor, breathing halo, 3D cases
  back-to-front with the selected one's reflection and inner glow, per-shelf wordmark + tag + count, tab capsule with
  gliding pill, clock row, counter, Menu, title strip with rise, PAD_HINTS bar, letter flash, loading / empty
  state), `ui_fonts.*`, `clock.hpp`, `elf_client.*` + `library_loader.*` (the console reads /appdb, the configs and
  every root on a worker thread through Elf Arsenal; GET only).
- `main.cpp`: pad → InputTracker → shelf; library loaded in the background.
- `tools/deploy.sh`: refuses to deploy if `/appdb` shows PPSA99850 owned by another app.

**Files:** `ps5/src/app/*`, `ps5/src/gl3d.*`, `ps5/src/gl_canvas.hpp`, `ps5/src/gl_main.cpp`, `ps5/src/gfx/draw_list.*`,
`ps5/src/gfx/gl_batch.cpp`, `ps5/src/data/scan.*`, `ps5/src/main.cpp`, `ps5/tests/test_shelf.cpp`,
`ps5/tools/{preview-boxshelf.cpp,preview-boxshelf-gl.sh,bake-fonts.sh,deploy.sh,test-boxshelf.sh}`,
`ps5/third_party/boxshelf-fonts/*`, `ps5/assets/fonts/*`, `PROGRESS.md`.

**Tests run (WSL, `ps5/`):** `make test` → data 171/0 failed, shelf 57/0 failed (ASan + UBSan), list-library ok,
preview ok. `make preview` → 12 modes. `make app` → builds, `integrity: valid`.

**Verified on:** unit tests + PC preview (Mesa) compared with Boxshelf's own render. **Not on the console.**
Side by side (`ps5/.local/ref/ref-gc-sel5.png` vs `ps5/.local/gl-gc.png`, `ref-n3ds-sel2.png` vs `gl-n3ds.png`):
case positions within ~2 px (selected 775–1137 in both; stacks start at 160 / 1198 vs 158 / 1198; 3DS landscape
678–1218 vs 678–1220), title strip, counter, Menu, tab bar, hint bar at the same places.
Differences left: covers / spine text / glare / sheen / floor strips (P3), Spectrum's moving ribbons (P6), keyboard
hints in the reference vs controller hints here (intended).

**Screenshots:** `ps5/.local/gl-{gc,gc-first,n3ds,sw,ps2,psx,wii,wiiu,ps5,anim,empty-shelf,loading}.png`,
references `ps5/.local/ref/ref-{gc-first,gc-sel5,n3ds-sel2}.png`.

**Deviations:** see "Deviations from Boxshelf" (fonts, ♥, Create button, clock, placeholders, Home, theme layer).

**Open questions for the owner:** 4. The Create (⧉) button: Boxshelf uses it for Settings / hold Museum, but the
PS5 keeps it for screenshots. Which button should open Settings? (Default until you say: touchpad click.)

**Console, read-only (2026-11-10, after the owner switched it on, 192.168.2.120):** `/appdb` has 29 titles and
**PPSA99850 is free**. `make library PS5_HOST=192.168.2.120`: 16 roots listed; GameCube 2, Wii 1, Wii U 1, Switch 8
(one folder each), PS2 105, PS4 2, PS5 11, 3DS 5, PS1 87, DS 0, Media 10. Rules to fix (found on the real list):
- PS2 files are often named without spaces ("2002fifaworldcup.chd"): the title must come from the disc serial
  (PS5SX2's covers / Estante's serial cache), not the file name.
- No-Intro writes an apostrophe as "_" ("Luigi_s Mansion"): turn "_s " into "'s ".
- `/appdb` apps that are not games (YouTube, PC5) land on the PS5 shelf; homebrew ports (Breath of the Wild
  PPSA90212, TLOZ Ocarina of Time 4k PPSA99620) land on Media. Boxshelf has a "Custom" shelf for PC ports / recomps
  and Media for streaming apps — see open question 5.
Nothing was written to the console.

**Open questions for the owner:** 5. Ports/recomps (Breath of the Wild, Ocarina of Time 4k): a "Custom" shelf as in
Boxshelf (default), or Media? And YouTube on Media (default) rather than PS5?

**Next step:** owner checkpoint on the console (deploy needs his go-ahead). Then the sort/filter panel and the
Menu, and P3 (case anatomy: real covers, spines with band and title, glare, sheen, floor strips).

## 2026-11-10 — P2 checkpoint + P3 — owner looked on the console; real covers from the console
**P2 on the console (owner):** the owner deployed it himself (the Claude Code safety classifier refused the FTP
deploy as "Production Deploy" even with his go-ahead; I gave him the command and did not work around it). He saw
the shelves with the placeholder covers ("band on top, random colours") and said it looks good: go on.
Decisions from him: ports (Breath of the Wild, Ocarina 4k) on a Custom shelf; YouTube on Media.

**Done (P3, covers):**
- `src/data/cover_index.*`: maps each game to box art the emulators already keep on the console (read with
  Elf Arsenal, GET only): PS1 SwanStation serials → `covers/default/<serial>.jpg`; PS2 `chd-serials.txt` →
  `covers/<serial>.jpg` (+ spine, back) and, for squashed file names, the title from Estante's `ps2-titles.tsv`
  on the console; GC/Wii Porpoise `disc-cache.tsv` (v2 columns) → `covers/<id>.png` (+ back, spine), and the
  disc decides GameCube vs Wii; Switch ProsperoEden `<hash>.name` titles → `<hash>.tga`; 3DS PS5-ECAM
  `library.json` → `covers/boxart/3ds/<gameId>.tga`, the game icon as fallback; PS5/PS4/apps
  `/user/appmeta/<id>/icon0.png`. Caches name older homes (/mnt/ext1/...): games match on file / folder name.
- SwanStation `usb_folder` may be a path ("Emulation/roms/psx", the owner's setting): accepted.
- `src/app/cover_loader.*`: worker thread downloads, decodes (Estante's stb `decode_cover_rect`) and crops each
  cover to its system's shape (`cover_spec`, 360 px wide); square art is composed: games get the system band +
  the art at the full face width + its own colours below (lesson 2: never a small picture); Media apps get
  Boxshelf's `iconFront` look. The shelf asks for the covers in view, nearest first, keeps 32 decoded.
- Tools: `fetch-listings.py` also saves the cover indexes and cover folder listings; `list-library` reports
  coverage; `fetch-covers.py` copies the console's covers to `.local/console` (not committed) so the preview can
  show them (`make preview MODES="real-psx-10 real-sw-4 ..."`, `BOXSHELF_CONSOLE_DIR`).

**Coverage on the owner's console (read-only):** box art for **161 of 232** games — GC 2/2, Wii 1/1, Switch 8/8
(icons, composed), PS4 2/2, PS5 10/10 (icons, composed), Custom 2/2, Media 9/9, 3DS 5/5 (one via its icon),
PS1 77/87, **PS2 45/105**, Wii U 0/1. The missing PS2 covers are not on the console at all: they need the PC
cover tool (GameTDB / PS2 covers) — waiting for the Estante handoff (`emulator_covers_sync.py`).

**Tests run:** `make test` → data 200/0, shelf 57/0, list-library ok, preview ok; previews `real-psx-10`,
`real-sw-4`, `real-n3ds-3`, `real-gc-1`, `real-ps5-4`, `real-media-3` with the console's covers; `make app` builds.

**Verified on:** unit tests + PC preview with the owner's real covers copied read-only. **Not yet on the console**
(the console went to sleep before a new deploy).

**Deviations:** composed covers have no title line yet (covers.js draws the title; needs the text-to-texture pass
that the spines need too); PS5/PS4 use the square `icon0` until the store portrait art tool is ported.

**Next step:** owner redeploys to see real covers; then spines (band + title), glare, sheen, floor strips, backs.

**Console check (owner's screenshots, 2026-11-10 14:12–14:13, read from `/user/av_contents/photo` read-only and
converted from JPEG XR into `ps5/.local/ps5-shots/`):** the redeployed build shows the real covers on the console:
PS2 (Ben 10: Ultimate Alien, title from the serial database), 3DS boxes (Kirby, Luigi's Mansion, Dragon Ball
Fusions) in the landscape case. Clock 14:12 matches the capture time, so the time-zone parameters (4/5) are right.
PS2 games without art on the console keep the placeholder (expected). Verified **on the console**.

**Owner feedback on those screenshots → fixed (PC preview, not yet redeployed):**
- "A light around the cover looks odd": the selected case's inner glow was drawn as flat 2D strips over a face in
  perspective. Removed (Deviation: Boxshelf's subtle `inset` glow returns when it can be drawn in the 3D pass).
- "Some covers don't fill the case (PS5, Switch)": every cover now sits in the case's plastic frame (`edge`, case
  colour; style.css `.f-front` padding) with the clear plastic's rim light and edge darkening baked in; square art
  (Switch icons, PS5/PS4 `icon0`, 3DS icon fallback) is cropped to fill the face. Tried the 3840x2160 `pic0.png`
  key art for PS5: cropped to a case it was often a meaningless close-up, so `icon0` (it carries the logo) is used.

**Owner: "take the art from where Boxshelf takes it".** Done (PC; upload waits for the owner):
- `tools/boxshelf_covers.py`: port of Boxshelf's `main/covers.js` sources and name matching (lbindex.js `norm`,
  `lookupNames`, `matchTitle`, `libretroMatch`): 1. libretro thumbnails Named_Boxarts, 2. LaunchBox Games Database
  (Metadata.zip indexed once every 60 days; Box - Front and its fallbacks; the only source with PS5 boxes),
  3. GameTDB coverHQ by disc id (GC/Wii). Pictures are shrunk to ≤1200 px JPEG (the console decoder stops at 2400).
  Result on the owner's library: **215 covers** (PS1, PS2 incl. the 60 that had none, GC, Wii, Wii U, 3DS, Switch,
  PS4, 5 PS5). Not found: titles the console reports in Portuguese ("Indiana Jones e o Grande Círculo", "LEGO
  Batman: O Legado…"), EA FC 27, PC5, Ocarina 4k, Akumajou D X — those keep the console's art.
  Deviation from Boxshelf: the part before " and " must match exactly (Boxshelf's close call took "Indiana Jones and
  the Staff of Kings" to "…the Great Circle").
- `tools/upload-covers.py`: FTP into a new folder of this app only, `/data/boxshelf/covers/<game id>.jpg`
  (dry run: 215 files). **Writes to the console: run only with the owner's go-ahead.**
- App: `/data/boxshelf/covers` wins over the emulators' art; the emulator's picture stays as fallback.
- Preview (`real-*` modes) checked with those covers: real PS5 / Switch / PS2 boxes on every case.

## 2026-11-10 (later) — covers downloaded by the app itself; case anatomy 1:1

**Owner: "everything automated, the user just opens the app".** The app now fetches its own box art on the console
(nothing to run on a PC):
- `src/app/art_fetcher.*`: background thread with HTTPS (PacBrew libcurl + OpenSSL through `src/net/console_curl.c`
  from Estante). Sources and matching are Boxshelf's (`src/data/art_match.*`, the C++ port of lbindex.js `norm`,
  `lookupNames`, `matchTitle`, `libretroMatch`; same answer as the Python tool on all 438 library entries):
  libretro Named_Boxarts (index cached 7 days), GameTDB coverHQ by disc id (GC/Wii), GameTDB title databases
  (switchtdb / 3dstdb zips, weekly, read with miniz). Pictures are shrunk to ≤1200 px JPEG (stb_image_write) into
  `/download0/boxshelf/covers/<key>.jpg` (the app's own sandbox folder) and appear on the shelf as they arrive.
  Misses are remembered (versioned file) only when the sources answered.
- Priority: `/data/boxshelf/covers` (PC tool) > the app's downloads > emulator caches > `icon0`.
- Licences of the linked libraries in `ps5/assets/licenses/`, THIRD_PARTY_NOTICES updated.
- **Verified on:** PC (host build of the same fetcher: 184/211 libretro + GC/Wii, Switch 6/8 via GameTDB). **Not yet
  on the console.** LaunchBox (the only PS5 box source) is still PC-tool only.

**Owner: "the case still looks thick; look at how Boxshelf does it, 1:1".** Pixel comparison with Boxshelf showed
the geometry already matched; the missing part was the finish. Now drawn in the 3D pass (`gl3d.cpp` shader):
- Spines (style.css `.f-left`, setup.css `.f-right.spine2`) on both sides: case colour, `.spine-band` (top 9 %,
  system label, "EMU"/"ADD" on the emulator/setup tiles) and `.spine-title` (Lexend 600, bd×.46, `spineInk`, top to
  bottom, ellipsis), with setup.css's shading film (brighter on the selected case). Rendered every frame into one
  spine atlas (`GlCanvas`, a row per case + band tiles).
- `.glare` (104° band, stops on Boxshelf's 3-face gradient line, slid by `(-ry/110 - x/(bw*7))*83.25 %`; .55 on side
  cases), `.sheen` sweep 380 ms after landing (.85 s, cubic-bezier(.3,.6,.3,1)).
- `.f-front::after` film (top rim light, darker edges) moved from the baked cover into the shader so side cases get
  it at .55 like Boxshelf.
- Rounded corners (`rc`) only on the side away from the visible spine, mirrored right of the selection (`.rside`).
- `.dim` darkens only the front (spines and sides stay full colour); no lighting on the case faces (CSS planes).
- **Verified on:** PC preview, side by side with the Boxshelf capture `ref-gc-sel5` (spines, glare and corners
  match); `make test` (data 224/0, shelf 57/0, list-library, preview) and `make app` build. **Not yet on the
  console.**
- Deviation: band label uses Lexend 800 (700 is not baked); the selected case's right side is not hidden behind the
  "opening side" look (it faces away anyway).

**Next step:** owner deploys and sends screenshots; then LaunchBox on the console (PS5 boxes), floor strips, backs.

## 2026-11-10 (evening) — owner's shelf decisions, Flip, Play

**Owner: "no museum; do the rest. Flip doesn't work. Drop Surprise me; emulator apps show on every console tab:
rename Custom to Others and put the apps there."**
- Museum (P5) dropped by the owner.
- Console shelves now hold only the games (Deviation from app.js prepare(), owner's decision): no "Surprise me", no
  "Open <emulator>" tile; an empty console shelf keeps "Add games". The Custom shelf is now **Others** ("Apps &
  ports", spine label APP) and holds every PPSA9xxxx title: ports, emulators, tools (Estante, Store, ...).
- **△ Flip** (tap; hold stays for favourite, not done yet): the selected case turns 180° (.75 s,
  cubic-bezier(.3,.95,.25,1)); ○ turns it back; moving resets it. The back is the emulator cache's own back art
  when there is one (PS2 / GC / Wii), else covers.js `drawBack()` rendered into the back target: gradient, system
  band, title (2 lines), four "screenshots" (drawScene reduced to sky, moon and hills: the other scene kinds are not
  ported — Deviation), info rows (Region / File / Emulator / Serial; Type / Title ID for apps).
- **✕ Play**: Estante's route (`src/app/launcher.*`, own thread): /api/state first (another game open → message);
  Porpoise / PS5-ECAM / SwanStation / ProsperoEden start straight into the game through Porpoise's
  home-launcher.elf patched and sent to the ELF loader (9021) — PS1 folder → its .cue, Switch folder → biggest
  .nsp/.xci; PS5SX2 gets `/data/PCSX2/logs/disc-launch.txt` over FTP then opens; anything else (and any failure)
  `/launch?titleId=`. The console confirms within ~6 s. Messages in Boxshelf's `.toast` (white pill, turning ring).
  Note for the owner: Play sends Porpoise's launcher to the ELF loader, as Estante does.
- **Verified on:** unit tests (data 224/0, shelf 54/0), PC preview (`gc-flip`, `gc-turn`, `real-ps2-10-flip`,
  `n3ds-flip`, `real-custom-0`, `gc-play`: on the PC the launcher answers "needs the console"), `make app` builds.
  **Nothing of this has run on the console yet.**

**Next:** owner tests on the console; then □ Details, Menu / sort panel, settings, LaunchBox on the console for PS5
boxes, hold-△ favourites, recently played, pt-BR / es.

## 2026-11-10 (night) — names, Spectrum layer, languages, quick menu, N64 shelf

**Owner: "are my game names right? is the background modern like Boxshelf? add the languages; there must always be
a menu — I have two Zelda recomps, how would I put them on the right shelf?"**
- Names: checked the real library (219 games, list-library). Clean except PS1 folders without apostrophes
  ("Tony Hawks Pro Skater", "Disneys Tarzan"...). The loader now takes the proper name from the libretro index the
  art fetcher keeps (`/download0/boxshelf/libretro-<sys>.txt`) **only when it differs by apostrophes alone**
  (libretro's file names also turn ':' into ' - ' and '&' into '_': those are not taken). 13 PS1 titles fixed
  (host check against the PC copy of the index). On the console it applies from the second start (the index is
  downloaded on the first).
- Background: Boxshelf's Spectrum theme has a moving layer (themefx.js `spectrum()`): three slow ribbons of light
  (a wide 9 % band, a 1.2 % band and a hairline per ribbon) and 80 rising, twinkling dust motes in the shelf's hue,
  over the wash and under the vignette. Ported (canvas 'lighter' blending approximated by normal blending).
- Languages: `src/app/i18n.*` — English (Boxshelf's words), Português, Español; every interface string, the clock
  date, the launch messages, the made-up back. Default = the console's language (system param 1; pt-BR/pt-PT ->
  pt, es/es-LA -> es, else en); the menu changes it.
- Quick menu (☰ / Options; app.js "The quick menu", quickshelf.css): glass panel from the right, the shelf steps
  aside (translate only — the .92 scale is not done: Deviation), game header (cover, shelf, title, meta), rows
  with the white pill, chips. Rows now: **Move to shelf** (any shelf, N64 included; the original shelf undoes it) and
  **Language**. Saved in `/download0/boxshelf/settings.json` (`src/app/settings.*`), applied at load after the
  art lookup; moved installed titles still open by their own title id. Owner's request: not in Boxshelf (Deviation).
- New **N64** shelf with Boxshelf's systems-extra.js style (card box 1.38, "NINTENDO 64" band, .wm-n64); it only
  shows when a game is moved there (no N64 emulator on the PS5).
- **Verified on:** unit tests (data 228/0, shelf 64/0 incl. move_item and the clock in 3 languages), PC preview
  (`real-custom-0-menu` in pt, Spectrum layer on `real-sw-4`), `make app` builds. **Not on the console yet.**

## 2026-11-10 (late) — real backs, Box art in the menu

**Owner: "why does the back have no art? There must be a setting to download the covers again or fetch the missing
ones — some games have no cover."**
- Backs, from where Boxshelf takes them (main/covers.js):
  - **In the app** (automatic): GameTDB's full cover scan (back | spine | front) — GameCube/Wii by disc id
    (coverfullHQ/coverfull), Switch/3DS by title (title databases) — with the back cut by Boxshelf's fold search
    (img-tools.js spine(), part 'back', main.js CASE shapes), ported to C (`art_back_to_jpeg`). Saved as
    `/download0/boxshelf/covers/<key>-back.jpg`; shown on △ Flip as they arrive.
  - **PC tool** (LaunchBox "Box - Back", English-only regions first as Boxshelf does): `boxshelf_covers.py` now also
    writes `<key>-back.jpg`; `upload-covers.py` sends them with the fronts; the app takes `/data/boxshelf/covers/
    <key>-back.jpg` over an emulator's back. PS1/PS2/PS4/PS5 backs only come this way until LaunchBox runs on the
    console.
- Menu → **Box art**: "Find missing" (asks again for every front or back not here, ignoring the week of misses) and
  "Download all again" (fetches everything again). Progress in the toast ("Box art 12 / 60"), "Box art up to date"
  at the end; the row shows "Downloading …" while it runs.
- **Verified on:** PC — host run of the app's fetcher on the owner's GC/Wii/Switch/3DS games: 12 of 16 backs
  (GC 2/2, Wii 1/1, 3DS 4/5, Switch 6/8), every cut checked by eye against the scans; PC tool: 193 LaunchBox backs
  for PS1/PS2/PS4/PS5 (checked a sample); preview `real-ps2-10-flip` shows Blade II's real back. Unit tests (data
  228/0, shelf 64/0), `make app` builds. **Not on the console yet; the LaunchBox backs are on the PC until the owner
  runs the upload.**

## 2026-11-10 (late night) — backs option, demo shelves

**Owner: "the user must also have the option to download the backs; I want to post screenshots and videos: can you
put games of every platform, a mock, with nice covers?"**
- Menu → **Box backs: Download / Off** (saved; default Download). Off: no backs are asked for; turning it on fetches
  the missing ones at once.
- Menu → **Demo shelves: Off / On** (saved). 120 well-known games on 14 shelves (GameCube, Wii, Wii U, Switch, PSP,
  PS2, PS3, PS4, Xbox 360, 3DS, PlayStation, DS, N64) in `src/app/demo_library.cpp` — **titles only**: their box
  art is downloaded at run time by the app's fetcher like any game's (libretro; Switch from GameTDB's title list) into
  the app's own folder, so nothing copyrighted is in the repository. PS3 / PS4 / Xbox 360 use games libretro has
  (its sets there are small). The PS5 shelf keeps the console's own installed titles (no PS5 boxes on libretro /
  GameTDB). Demo games don't start (toast). `tools/demo-check.sh` checks every title with the app's matcher.
- A box art run asked for while another is going now waits for it and starts after.
- **Verified on:** PC — demo-check: 110 of 120 titles on libretro, the 10 Switch ones through GameTDB; the app's
  fetcher on the PC downloaded **120 / 120 fronts** and 18 backs; previews `demo-ps2-5`, `demo-n64-3`, `demo-x360-3`,
  `demo-gc-6-flip`. Unit tests (data 228/0, shelf 64/0), `make app` builds. **Not on the console yet.**

## 2026-11-10 — release ps5-v0.1.0

Owner said "sobe". Pushed branch `ps5-port`, tag `ps5-v0.1.0`, GitHub pre-release "Boxshelf PS5 0.1.0" with
`PPSA99850.zip` (SHA-256 eab81920680d25f439032ea63b2925a9812f3c1ef4838b7b71366462faedef3f), built from b1fea61 after
the full test run. Marked pre-release: Flip, Play, the menu, backs and demo shelves are verified on the PC preview
only. The tour videos (`.local/video`) are not published: they show box art.

## 2026-11-10 — release ps5-v0.2.0

Owner: "take the demo part out and publish it as a release". The demo shelves are gone from the app (menu row,
setting, main); their title list moved to `ps5/tools/demo_library.*`, used only by the PC tour video and
`tools/demo-check.sh`. contentVersion 01.002.000. Tests (data 228/0, shelf 64/0), preview of the menu in pt, `make
app`. Pushed `ps5-port` (cb43b82), tag `ps5-v0.2.0`, GitHub release "Boxshelf PS5 0.2.0" (full release, marked latest)
with `PPSA99850.zip` (SHA-256 755525b11f34b31e14fe8904a0779bfddb9b95f2ce4179bdc508bacd9985f252). The notes say Flip,
Play, the menu and the backs were tested on the PC preview; the console check of this build is still to come.

## 2026-11-10 — renamed to Vitrine; release ps5-v0.2.1

Boxshelf's author asks forks for their own name and logo (PS5-PORT.md rule 8). The rename itself (titleName
"Vitrine", new icon, i18n / scene text, User-Agent, README, notices, tests; contentVersion 01.002.001) was committed
by the owner's other assistant (12c417f, c3acc3a), which could not build. Here:
- Pulled ps5-port; `make test` (data 228/0, shelf 64/0, list-library, preview smoke) and `make app` pass.
  `dist/PPSA99850/sce_sys/param.json` says titleName "Vitrine" / 01.002.001, and the package's icon0.png is the new
  one (byte-identical to sce_sys/icon0.png, different from 0.2.0's) — checked on the PC build output.
- Searched for "Boxshelf" the user can see: none left in the app's strings or assets. The PC cover tool's
  User-Agent now says Vitrine (e3b7e47). Kept on purpose: the internal identifiers (namespace boxshelf,
  /download0/boxshelf, /data/boxshelf/covers) and the package contentId (UP9000-PPSA99850_00-BOXSHELFPS5APP01, not
  shown to users; changing it could make the console treat the update as another title).
- Release: tag ps5-v0.2.1, pre-release "Vitrine 0.2.1", `PPSA99850.zip` (folder PPSA99850/ at its root), SHA-256
  79a906e0caf8ad40658cb91e39d1095941320716294df4fad605a28a6fc44566. Not run on the console yet.

## 2026-11-10 — release ps5-v0.2.2 (new icon and palette)

The repository is now github.com/kauansilvalopes/vitrine (the old address redirects; this clone still pushes through
the old remote URL: repointing it was blocked by the tool's safety check, the owner can run `git remote set-url`).
Pulled da677c1 (the owner's new icon in the new palette, 512x512, not changed here). contentVersion 01.002.002.
`make test` (data 228/0, shelf 64/0, list-library, preview smoke) and `make app` pass; in dist/PPSA99850:
titleName "Vitrine", contentVersion 01.002.002, icon0.png byte-identical to sce_sys/icon0.png, also inside the zip
(checked on the PC build output). Release: tag ps5-v0.2.2, "Vitrine 0.2.2", marked Latest, `PPSA99850.zip`
(PPSA99850/ at its root), SHA-256 ea6f6e7d81794485f07e644469b8f1310aabab468885887d87dae0f68e861fea. Not run on the
console.

## 2026-11-10 — hotfix ps5-v0.2.4

Owner: "a shelf the user has no games for must not show (no GameCube games, no GameCube tab)" and "buttons without a
function yet must not show (R3 inspect, settings, square details) until they are done".
- `build_shelves` keeps only shelves with games: no empty console tab (even with its emulator installed), no empty
  PS5 or Home tab (Deviation from Boxshelf's "Add games" tiles, owner's decision). Test updated (data 232/0).
- Hint bar: only L2/R2 System, △ Flip, ✕ Play, ☰ Menu. Gone until their features exist: □ Details, R3 Inspect,
  L3 Shuffle, Settings / Museum, the held favourite (△) and sort (☰). The top bar's sound button is gone too (no
  audio yet); the controller indicator stays.
- Verified: unit tests (data 232/0, shelf 64/0), PC preview with the owner's real
  library (`real-gc`, `real-sw-4` in pt: the empty DS tab is gone, four hints), `make app`. Not run on the console.
- The push met a new commit on ps5-port (19c7287, the "VI" icon made of game cases). Merged it (no rewrite). The tag
  ps5-v0.2.3 was already pushed on the hotfix commit without that icon, so it has no release; the hotfix ships as
  **ps5-v0.2.4** with the new icon: contentVersion 01.002.004, tests (data 232/0, shelf 64/0), `make app`, the
  zip's icon0.png is the VI icon. "Vitrine 0.2.4", Latest, `PPSA99850.zip`, SHA-256
  e98ccef5d191e52d245c2a0c9650a4477420f71bfa19feabb528de1d01fc7b54.

## 2026-11-10 — release ps5-v0.2.5 (Roman VI icon + the hotfix)

The owner asked to wait for a new logo before releasing the hotfix; then the other assistant's task came: release the
Roman "VI" icon (ff0bc46, 512x512, not changed here) as 0.2.3. Tags ps5-v0.2.3 and ps5-v0.2.4 already existed
(pushed before the wait, no release on either; nothing deleted), and the hotfix above was still unpushed, so both
ship together as **ps5-v0.2.5**: merged ff0bc46 (no rewrite), contentVersion 01.002.005. Verified on the PC build:
tests (data 232/0, shelf 64/0, list-library, preview smoke), `make app`; dist/PPSA99850 says "Vitrine" /
01.002.005 and the zip's icon0.png is byte-identical to ff0bc46's. Release "Vitrine 0.2.5", Latest,
`PPSA99850.zip` (PPSA99850/ at its root), SHA-256 45251ad72becd1dbbbd597687bf69a5cbed938217021546f80f48b1980f337f2.
Not run on the console.

## 2026-11-10 (night) — towards Boxshelf's level: Home, favourites, sort / filter, Details, Inspect

Owner: "do the remaining steps so the app is as good as Boxshelf". Local commits, not pushed (no "sobe" yet):
- **Favourites and Home** (ec5d73a): hold △ toggles a favourite (saved; the heart badge of `.slot.fav
  .f-front::before` drawn in the 3D pass); a start the console confirms goes into the play history; the **Home**
  shelf (app.js buildHome: the 12 last played, then the favourites A–Z) comes first when it has games, every case in
  its own console's box (app.js `--bw = --ch * ratio`); its info line says "PS2 · played 3 days ago · Favorite".
  **L3 Shuffle** lands on a random game. Uppercase now covers á/é/í… ("INÍCIO").
- **Sort and filter** (8630317): Menu → Sort (A–Z, last played, most played) and Filter (all, favourites, played,
  not played yet), saved; hold ☰ opens the menu on Sort. A shelf the filter empties goes; if nothing is left the
  empty screen says how to bring the games back. Boxshelf's year / rating / genre / play-time sorts and its decade
  and genre filters need game metadata the app does not have yet (Deviation).
- **Details** (10a9ad4): □ opens Boxshelf's details panel (the shelf steps aside): hero in the game's hue (no blur
  pass), cover, "PS2 · 11 of 105", title, chips, Last time / Launches tiles, Game info (region, format, emulator,
  serial, file), Your copy (front / back present), "Plays in …"; ✕ Play and △ Favorite buttons; ←/→ walk the shelf
  with it open; ○ / □ close. No description, logo, screenshots or trailer yet (no metadata source on the console).
- **Inspect**: R3 brings the case forward; right stick / D-pad turn, L2 / R2 zoom; ✕ opens the case (door at ~55°),
  ✕ takes the disc out (its label is the cover), ✕ plays; ○ steps back; the hint bar follows each step.
- Hint bar now: System, Flip (hold ♥), Details, Play, Inspect, Shuffle, Menu (hold Sort). Only Settings / Museum
  stay hidden.
- **Verified on:** unit tests (data 232/0, shelf 68/0 incl. set_home), PC previews (`demo-home-*`, `demo-gc-0` with
  BOXSHELF_FILTER=fav, `real-custom-0-menu`, `real-ps2-10-details`, `real-sw-4-details`, `real-gc-1-inspect`), `make
  app`. **None of it has run on the console yet.**

**Still missing for Boxshelf's level:** PS5 boxes (and PS1/PS2 backs) downloaded on the console from LaunchBox;
game metadata (description, year, genre, players) for the richer Details and sorts; sounds (no audio layer yet);
the Settings screen (Share).

## 2026-11-10 — release ps5-v0.3.0

Owner: "pode subir então nova versão". contentVersion 01.003.000; tests (data 232/0, shelf 68/0, list-library,
preview smoke) and `make app` pass; dist says "Vitrine" / 01.003.000 with the Roman VI icon. Tag ps5-v0.3.0, release
"Vitrine 0.3.0", Latest, `PPSA99850.zip` (PPSA99850/ at its root), SHA-256
2be46f4e37f1623f8b1c57262570a0b209664433ae82f3c363b27deff6017571. Contents: Home + favourites + history, shuffle,
sort / filter, Details, Inspect (the entry above). Verified on the PC only; not run on the console.

## 2026-11-10 (late night) — Inspect rebuilt, launch screen

Owner: "skip themes and sounds, do the rest; Inspect (open the case, see the disc) is very buggy, research and make
it like Boxshelf".
- **Why Inspect looked broken:** the engine draws without a depth buffer (faces sorted back to front), fine for a
  closed case on the shelf, wrong for an open case turned freely (lid, tray and disc overlapped in the wrong order),
  and the lid only opened 55 degrees over a black tray.
- **Rebuilt after inspect3d.js** (c67584a, 4845b85): a pass of its own with depth and 4x MSAA (resolved into a
  texture shown over the shelf dimmed by rgba(3,4,8,.72)); a case 48 % of the screen tall (30 degree camera 3.9 case
  heights away); hollow tray (back plate printed with the back, four walls, the inside lifted from black like
  inspect3d.js, the disc cradle and the hub); printed spine; lid on its hinge opening to 165 degrees with the booklet
  and two clips inside; the disc (cover label inside the case border, clear hub, silver data side) sized by app.js
  MEDIA (GameCube .62 and seated .1 above the middle, Wii .86 ...); "take out": the case slides away left and turns
  30 degrees, the disc comes forward 1.55x and spins slowly when left alone; Switch / 3DS hold a cartridge (dark shell,
  label, gold contacts) in a cradle. Bug found on the way: the depth clear ran with depth writes off (fixed).
- **Launch screen** (app.js #launch): Cross fades in the console's tint, the cover card in the case colour (lCardIn,
  lFloat, the glare crossing it), the console's wordmark, title, the 1.8 s loading bar, the emulator's name, and the
  start's messages under it (no toast); it steps aside 2.5 s after the console confirms the game, or stays with the
  error until Circle.
- **Verified on:** PC previews (`real-gc-1-inspect` with BOXSHELF_ISP 0/1/2 and BOXSHELF_ISP_TURN, `real-sw-4-inspect`,
  `real-sw-4-play`), tests (data 232/0, shelf 68/0), `make app`. Not on the console.

## 2026-11-10 (night) — LaunchBox on the console, hiding games

- **LaunchBox** (3e5ec39): the app reads LaunchBox's Metadata.zip while it downloads (streamed through the zip's
  inflate, never stored), keeps one small TSV per platform (refreshed every 60 days), and uses it for PS5 / PS4 boxes,
  the back of every system it covers, and game info (overview, year, genres, developer, publisher, players, ratings)
  shown in Details, the made-up back and the new sorts and filters.
- **Hide a game** (app.js ui.hidden): Menu > Hide this game takes it off every shelf, Home included; Menu > Hidden games
  brings back one or all. Saved in settings.json.
- **Verified on:** PC host run of the streamed fetch (about 30 s from LaunchBox's server, 213 of 231 library games
  matched, PS5 boxes downloaded), previews `real-ps5-1-details` and `real-gc-1-menu`, tests (data 232/0, shelf 68/0),
  `make app`. Not on the console.

## 2026-11-10 (night, later) — play time, visual finish

- **Play time** (5539337): a session opens when the console confirms a game. A thread asks /api/state every 15 s, and
  the session closes once the game is no longer running. It is kept in settings.json, so a restart of the app still
  finishes it. A session over 12 h is not counted (console left on or asleep). Details shows three tiles: Time played,
  Last time and Launches (app.js fmtPlay). "Most played" now sorts by time. A unit test covers the settings round trip
  (hidden, ms, session).
- **Shelf moves aside like #scene** (a48e57b): translateX(-17vw) and scale(.92) with the menu or Details open. This is
  a 2D transform of the 3D projection (Gl3D::set_view).
- **Made-up fronts carry the title** (covers.js drawFront): there is a fronts atlas on the GPU, one cell per coverless
  case. The title is centred and shrinks to fit 4 lines. It has a dark outline and a white-to-hue gradient fill.
- **Frosted glass** (0ee7065): the menu and Details sit over a real blur of the frame, cases included. This is the
  renderer's glass pass, now replaying the 3D cases too.
- **Inspect, PSP** (4cf3a56): UMD with an oval well and the disc in a clear shell. Preview mode `demo-<sys>-inspect`.
- **Start-up** (4ab1cdf): the VITRINE splash shows while the library is read, with the current step under it. It is
  held until the covers in view are ready (at most 1.5 s). Then the cases ripple in from the chosen one (70 ms per
  step, .75 s each), the HUD fades up and the splash scales out. Preview mode `intro-<ms>`.
- **Still to do:**
  - Rename and collections: both need text input (the PS5 on-screen keyboard, untested here).
  - Choosing another cover.
- **Verified on:** PC previews (`real-gc-1-details`, `real-ps2-3`, `real-ps2-3-menu`, `real-gc-1-flip`,
  `demo-psp-2-inspect` with BOXSHELF_ISP 1/2, `loading`, `intro-350`), tests (data 232/0, shelf 73/0), `make app`.
  Not run on the console. The play clock in particular depends on how the console reports a closed game, which only
  the console can show.

## 2026-11-10 — deployed to the console

Owner: "manda para o meu ps5 a atualização". `tools/deploy.sh` now accepts the title already installed as "Vitrine"
(it only accepted "Boxshelf"). /api/state showed no game running; `PS5_HOST=192.168.2.120 tools/deploy.sh` built and
published 31 files to /data/homebrew/PPSA99850/ (eboot.bin and param.json last). settings.json and the caches in
/download0 were not touched. Awaiting the owner's test on the console.

## 2026-11-10 — release ps5-v0.4.0

Owner: "sobe tudo". contentVersion 01.004.000; tests (data 232/0, shelf 73/0, list-library, preview smoke) and
`make app` pass. Tag ps5-v0.4.0, release "Vitrine 0.4.0", Latest, `PPSA99850.zip` (PPSA99850/ at its root, 31 files),
SHA-256 b817550dad0f7cd0d3fe742f536e5cddde15c68c1e14910692e2a839f2e4fd93. Contents: LaunchBox, play time, hiding
games, Inspect rebuilt (case, disc, cartridge, UMD), launch screen, glass panels, titled made-up fronts, start-up
splash and ripple. Verified on the PC; the same build was deployed to the console, owner's test pending.

## 2026-11-10 (late) — box art crash and PS5SX2 backs (from the console's klog)

Owner: the menu said "Em dia" (up to date), yet "find missing" still had 27 games to look for. Many PS5SX2 backs did not
show, and the app crashed while those 27 downloaded. The log was read from the console (Elf Arsenal /api/klogs, read
only).

- **Crash:** SIGILL in the art fetcher thread. Symbolised with build/llvm-pie.elf (base 0x400000): `operator new`
  failing in `https_get` → `on_body` (string growth) under `ArtFetcher::fetch_back`. The app heap was a fixed 128 MB
  mspace (src/runtime/app_heap.c). With -fno-exceptions, a failed new traps. Full-size LaunchBox/GameTDB pictures
  (body up to 24 MB growing by doubling, plus the full decode) exhausted it.
  - **Fix:** the heap is 512 MB, and an allocation the heap cannot serve falls back to the system's malloc (and
    realloc/memalign) instead of failing. https_get reserves the Content-Length up front.
  - **Logging:** the 5 s frame log now prints heap live/peak/failures.
- **PS5SX2 backs not shown:** "cover failed" lines for /data/PCSX2/covers/*_back.jpg had two causes.
  - Some do not exist: PS5SX2 writes `<serial>_back.missing` when it has none. The library loader now lists
    /data/PCSX2/covers once and only keeps fronts, backs and spines that are there. A missing back is then fetched
    from LaunchBox.
  - The others exist (a 1516x2157 baseline JPEG decodes fine with the app's decoder on the PC). They failed while the
    heap was exhausted by the fetch.
- **Menu → Box art:** shows "N without box art" when games still have a made-up front. "Up to date" only shows when
  none do.
- The "box art matched for 332 of 232" count is fixed (a game is counted once).
- **Verified on:** tests (data 232/0, shelf 73/0), `make app`, preview `real-ps2-3-menu`. Not yet on the console.
- Deployed to the console (owner: "sim manda ai"): no game running, 31 files, eboot.bin 30908328 bytes listed on the
  console. Awaiting the owner's test of "find missing".
