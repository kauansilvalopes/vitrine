# Progress — PS5 port of Boxshelf

Read `PS5-PORT.md` first. This file is the log the owner and the monitoring assistant read. Append, do not rewrite
history. Newest entry at the bottom.

## Phase checklist

- [x] P0 Foundations (repo layout, build, preview harness, deploy script, native-vs-web decision) — PC only; not yet run on the console
- [ ] P1 Data (library roots, per-system styles, cover pipeline)
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

## Open questions for the owner

1. Is **PPSA99850** free on your console? (I will check `/appdb` read-only once the PS5 is on; I will only change
   the id if it is taken.)
2. This PC is not the one in PS5-PORT.md §9: user `Kauan` (not `TheBe`), no Git Bash (git only in WSL), Python 3.15
   without Pillow until I installed it. Work lives in `C:\Users\Kauan\Documents\PS5App\`. OK to keep it there?
3. PS5-PORT.md §4 lists `tools/emulator_covers_sync.py` in Estante; it is not in the public Estante repo
   (commit `0077695`). Is there a newer copy somewhere, or should P1 write it from scratch?

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
