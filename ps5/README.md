# Boxshelf for PS5 (native)

Native C++/OpenGL port of Boxshelf's UX for a jailbroken PS5. Title id **PPSA99850**. See `../PS5-PORT.md` (the
brief) and `../PROGRESS.md` (the log). Boxshelf's own sources stay untouched in the repository root as the reference.

The engine (build system, GL renderer, 3D case pass, preview harness) comes from the owner's Estante
(github.com/kauansilvalopes/estante), itself built on BlackBearReloaded's ps5-native-app-boilerplate,
ps5-homebrew-ui and ps5-opengl (GPL-3.0). The UI in `src/app/` is a port of Boxshelf's renderer.

## What it does

- Boxshelf's shelf: one tab per platform, 3D cases with each system's own box (spines, glare, rounded corners,
  floor reflection), the Spectrum theme with its moving light, letter jump (L1/R1), shelves (L2/R2).
- Your library, read on the console: PS5/PS4 titles and the folders your emulators use (PS5SX2, SwanStationPS5,
  Porpoise, PS5-ECAM, ProsperoEden). Homebrew apps and ports go to **Others**.
- Box art downloaded by the app from the sources Boxshelf uses (libretro thumbnails, GameTDB), fronts and backs.
- ✕ Play starts the game in its emulator (straight into the game where the emulator allows it); △ turns the case
  to its back.
- ☰ Menu: move a game to another shelf, box art (find missing / download all again), box backs on or off, demo
  shelves, language (English, Português, Español).

## Install

Needs a jailbroken PS5 with Elf Arsenal (HTTP API on port 6969, FTP on 2121) and ShadowMountPlus; developed on
firmware 11.60. Unzip `PPSA99850.zip` from the release into `/data/homebrew/` on the console (the folder must be
`/data/homebrew/PPSA99850`), let ShadowMountPlus register it, then open **Boxshelf** from the home screen. Open each
emulator at least once so its settings exist. The app writes only to its own folder (`/download0/boxshelf`).

## Layout

| Path | What |
|---|---|
| `src/app/` | Boxshelf scene (the port of `renderer/app.js` / `style.css`) |
| `src/main.cpp` | Console entry point |
| `src/gfx`, `src/core`, `src/platform`, `src/runtime`, `src/gl_*`, `src/gl3d.*` | Engine from Estante |
| `tools/preview-boxshelf*.{cpp,sh}` | PC preview: renders the real scene code with Mesa EGL to `.local/gl-<mode>.png` |
| `sce_sys/` | `param.json` and the launcher icon (Boxshelf's own icon) |

## Commands (WSL Ubuntu: clang-18, make, ninja, unzip, Mesa EGL)

```
make preview [MODES="empty"]   # .local/gl-*.png
make test                      # host checks + preview smoke test
make app                       # dist/PPSA99850/ and dist/PPSA99850.zip
make deploy PS5_HOST=<ip>      # FTP to /data/homebrew/PPSA99850 (only with the owner's go-ahead)
```
