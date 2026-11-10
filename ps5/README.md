# Boxshelf for PS5 (native)

Native C++/OpenGL port of Boxshelf's UX for a jailbroken PS5. Title id **PPSA99850**. See `../PS5-PORT.md` (the
brief) and `../PROGRESS.md` (the log). Boxshelf's own sources stay untouched in the repository root as the reference.

The engine (build system, GL renderer, 3D case pass, preview harness) comes from the owner's Estante
(github.com/kauansilvalopes/estante), itself built on BlackBearReloaded's ps5-native-app-boilerplate,
ps5-homebrew-ui and ps5-opengl (GPL-3.0). The UI in `src/app/` is a port of Boxshelf's renderer.

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
