# Progress — PS5 port of Boxshelf

Read `PS5-PORT.md` first. This file is the log the owner and the monitoring assistant read. Append, do not rewrite
history. Newest entry at the bottom.

## Phase checklist

- [x] P0 Foundations (repo layout, build, preview harness, deploy script, native-vs-web decision) — PC only; not yet run on the console
- [ ] P1 Data (library roots, per-system styles, cover pipeline) — data layer + tests done on the PC; **open until the
      owner's real library is listed from the console** (`make library PS5_HOST=<ip>`), and the cover tools land
- [ ] P2 The shelf, 1:1 (scene, top bar with system tabs, backdrop, info, hints, input) — **owner checkpoint on the console**
- [ ] P3 Cases, 1:1 (per-system anatomy) — **owner checkpoint on the console**
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
- **Vignette (temporary):** the radial layer of `.vignette` is approximated with linear side gradients (visible
  banding in `ps5/.local/gl-empty.png`); P2 adds a radial-gradient shape to the batch shader.

### D4 (2026-11-10) — Shelves and where each game comes from
- Shelf order = Boxshelf's (Home, main/systems.js CONSOLES order, retro rows, Media last). Shelves for systems a PS5
  emulator runs (GC, Wii, Wii U, Switch, PS2, 3DS, PS1, DS) and PS5 always exist (Boxshelf puts an "Add games" tile
  on an empty console shelf); PS4 / Media / PSP / PS3 / X360 / Switch 2 only when they have something.
- PS5 / PS4 games come from Elf Arsenal `/appdb` (PPSA → PS5, CUSA → PS4). Homebrew ids (PPSA9xxxx: Estante, the
  emulators, tools) go to the **Media** shelf, Boxshelf's shelf for apps; this app itself is left out.
- Emulator games: the HD layout of §5, the emulators' own folders (Estante's `*_roots`) and the folders in their
  configs (PS5-ECAM json, PS5SX2 gs.ini, SwanStation ini; formats unconfirmed, see §5). Same path twice = one game.
- Porpoise's mixed folder: GameCube vs Wii by extension until the disc id (disc-cache.tsv) says otherwise.

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
