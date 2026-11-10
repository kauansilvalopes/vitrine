# PS5 port of Boxshelf — brief for the AI that builds it

> Para o dono (pt-BR): este arquivo é o que a IA nova lê primeiro. Ela deve registrar tudo em `PROGRESS.md`
> (o assistente antigo só monitora esse arquivo e o `git log`). Nada de publicar sem você mandar.

Read this whole file, then `PROGRESS.md`, before touching anything. Keep both up to date.

## 1. Mission

Build **a native PS5 app that is a 1:1 port of Boxshelf's UX** (this repository is Boxshelf's GPL-3.0 source; see
`README.md`). The owner (kauansilvalopes, Brazilian, writes pt-BR) already has a PS5 app called **Estante**
(github.com/kauansilvalopes/estante, title id PPSA99841, live and used). Estante is *not* to be touched: this is a
separate, competing project ("I am my own competitor"), started from Boxshelf's design, not from Estante's.

What "1:1" means for the owner, in his words:
- "the UX is perfect for what I wanted; the app should be practically a fork of it for PS5";
- covers are the protagonist; the **top menu, the shelf, the background, the cases, the details, the museum**, all as
  in Boxshelf;
- "the cover changes for each system, e.g. 3DS looks different": every system has its own case (shape, colour, band,
  spine). Do not flatten everything to one shape.

When Boxshelf and your taste disagree, Boxshelf wins. When Boxshelf cannot work on a PS5 (Windows-only things such
as `.glb` consoles, Electron, Steam library, RetroAchievements login, Chromecast…), hide or replace the feature and
write the decision in `PROGRESS.md` under "Deviations".

## 2. Hard rules

1. **Licence.** Boxshelf is GPL-3.0 (the owner's code is GPL-3.0-or-later too). Keep `LICENSE`, keep the credit to
   Boxshelf (sassiestrhihno) in README and `THIRD-PARTY-NOTICES.md`. Do not copy code with an incompatible licence.
2. **No copyrighted content in the repo**: no ROMs, BIOS, keys, firmware, game covers, box art, logos you were not
   given the right to ship. Cover art is downloaded at run time or by the owner's PC tools, for personal use, and is
   never committed. Fonts: Boxshelf bundles OFL fonts (Lexend etc.); those are fine.
3. **No AI attribution anywhere**: no `Co-Authored-By`, no "Generated with…" line in commits, PRs, releases or files.
   The owner is angry about it. Commit as the repository's configured git user.
4. **Never type or guess passwords.** If a step needs the owner's password or an interactive login, stop and give
   the owner the command (`! <command>` runs it inside the session).
5. **Console safety.** Do not deploy to the console, send payloads, change its settings or delete anything on it
   without the owner saying so in this session. Reading (listing folders, reading configs) is fine. Back up a config
   before editing it. Never touch the Bazzite/Linux side.
6. **Git.** Work on branch `ps5-port`. Small local commits are welcome. **Do not push, tag, release, or open PRs
   unless the owner says "sobe"/"push"/"release".** Never rewrite published history.
7. **Report faithfully.** Say what was verified where: unit test, PC preview render, or the real console. Never
   claim something works on the console if you only saw it in the PC preview.
8. **Name and logo.** Boxshelf's author asks that forks use their own name and logo (the GPL covers the code, not the name or the logo; saying "based on Boxshelf" is welcome). This app is called **Vitrine** and has its own icon. Never ship the name "Boxshelf" or Boxshelf's logo as the app's own: user-visible text, `titleName`, icon, release titles and store entries say Vitrine. Internal identifiers (`namespace boxshelf`, `/download0/boxshelf`, `/data/boxshelf/covers`) stay so saved settings and caches keep working.
9. Answer the owner in pt-BR, short and direct. Code, comments and docs in English.

## 3. Read this first: what Boxshelf is, and where its UX lives

Electron + plain JS/CSS (no framework); Android via Capacitor. The renderer (`renderer/`) never touches the OS: it
only talks to `window.boxshelf` (`docs/PLATFORM-API.md`, 141 methods, `renderer/mock.js` fakes it with demo data so
the UI runs in a plain browser: **run the original in a browser with the mock to see the target behaviour**).

| What | Where |
|---|---|
| The shelf scene, `place()` layout, keyboard/pad input, tabs, info line, hints | `renderer/app.js` (4298 lines), `renderer/style.css` (833) |
| Per-system case styles (`STYLE`) | `renderer/app.js` lines 8–30 |
| The 3D case (CSS 3D: front, back, left/right sides, bevels, glare, sheen, dim) | `renderer/style.css` 60–135, slot HTML in `app.js` ~341 |
| Background (blurred art, vignette, floor glow, halo) | `style.css` `.bg-layer .vignette .floor .sel-halo`, `app.js` bg code |
| Top bar (system word + count, glass tabs capsule, counter, buttons) | `index.html` `header.hud-top`, `style.css` 30–57, 501–514 |
| Details / inspect (open the case, disc, manual) | `renderer/inspect3d.js`, `inspect.css`, `app.js` |
| Museum (whole collection as a shelving wall) | `renderer/museum.js` (2246 lines) |
| Settings, themes, sounds, tour, setup wizard | `settings.js`, `themefx.js`, `sound.js`, `tour.js`, `setup.js` |
| Library logic: scanner, systems, covers, GameTDB, launcher | `main/scanner.js`, `systems.js`, `covers.js`, `gametdb.js`, `launcher.js`, `shared/titles.js` |

### 3.1 The facts that were already measured (do not re-derive)

Scene: `#scene{perspective:1800px; perspective-origin:50% 42%}`, track at `left:50%; top:42%`.

`place()` (selected case and its neighbours; `bw` = case width, `bd` = depth, `sg` = side sign, `ad` = distance):
- selected: `x=0, z=bw*1.05, ry=22°`;
- others: `x = sg*(bw*1.22 + (ad-1)*(bw*.22 + bd*.9))`, `z = -bw*.7 - ad*6`, `ry = -sg*60°`;
- non-selected cases get a `.dim` film (opacity .34) and half-strength glare; the selected one gets a white ring +
  coloured glow (`--glow`) + `.sel-halo` (radial, blurred, "breathing" 3.4 s).
- `bh = min(vh*.4, vw*.22/ratio, 520..620)`, `bw = bh*ratio`, `bd = bw*depth`.

Case style table (`ratio` = width/height, `depth` and `edge` as fractions of the case **width**, `rc` corner radius,
colours for the plastic frame (`caseColor`), the sides (`caseSide`), the spine text (`spineInk`), a `band` (top strip
shown on generated covers) and a `spineBand` (label on the top of the spine)):

| id | ratio | depth | edge | rc | caseColor | caseSide | spineInk | spineBand (bg / fg / label) |
|---|---|---|---|---|---|---|---|---|
| ps5 | .787 | .089 | .03 | .065 | #2b62d9 | #1f4cb3 | #111318 | #fff / #111318 / PS5 |
| ps4 | .787 | .089 | .03 | .065 | #1d4ed8 | #153a9e | #fff | #1d4ed8 / #fff / PS4 |
| ps3 | .787 | .10 | .03 | .065 | #203553 | #16263f | #fff | #0c0d10 / #fff / PS3 |
| ps2 | .711 | .104 | .03 | .035 | #0f1d55 | #0a1440 | #fff | #0f1d55 / #dfe6ff / PS2 |
| psp | .589 | .14 | .03 | .055 | #101014 | #07070a | #e8e8f0 | #0a0a0d / #fff / PSP |
| gc | .712 | .105 | .022 | .03 | #16161b | #0b0b0e | #e8e8f0 | #2c2c35 / #c3c3cf / GCN |
| wii | .711 | .104 | .03 | .04 | #f2f3f5 | #d9dbe0 | #4a4f58 | #fff / #8e949e / Wii |
| wiiu | .711 | .104 | .03 | .04 | #1d8fd1 | #146ea6 | #fff | #fff / #1d8fd1 / Wii U |
| sw | .612 | .096 | .035 | .06 | #d8101f | #a30a16 | #fff | #d8101f / #fff / SW |
| n3ds | **1.127** (landscape) | .10 | .03 | .05 | #f4f4f6 | #dcdde2 | #3a3d45 | #fff / #d40f2a / 3DS |
| x360 | .711 | .104 | .018 | .04 | #1a7f1a | #0f5a12 | #fff | #107c10 / #fff / X360 |

PS1 and DS are not in this snapshot's table (check `renderer/` for `systems-extra`-style presets, otherwise pick a
sensible jewel-case / DS case and note it). `app.js` is authoritative; re-read it, this table is only a head start.

Case anatomy to copy: the front is the cover inside a **plastic frame** of `edge` (case colour) with a top-edge rim
highlight and a faint darkening at the far edges, a moving **specular glare band** (104°) and a one-shot **sheen**
when a case lands; the spine is the case colour with the `spineBand` on top and the title running down in
`spineInk`; real depth, rounded corners (`rc`), floor reflection under the shelf.

## 4. The target platform

A **jailbroken PS5** (owner's: Slim, firmware 11.60; other firmwares untested). Homebrew apps are native C++ built with
the PS5 payload SDK; the GPU path is `ps5-opengl` + `ps5-homebrew-ui` by BlackBearReloaded (GPL-3.0). An app is a folder
`/data/homebrew/<TITLE_ID>/` (`eboot.bin`, `sce_sys/param.json`, `sce_sys/icon0.png`, assets) mounted by ShadowMountPlus and
uploaded over FTP (port 2121, anonymous). **Elf Arsenal** serves an HTTP API on port 6969 (`/appdb`, `/api/state`,
`/api/fs/list?path=`, `/api/fs/download?path=`, launching, …). The app runs in a sandbox with the filesystem
reached through that API from localhost.

**Reference code you may copy from, on this PC: `C:\Users\TheBe\Documents\PS5App\estante-ps5`** (the owner's own GPL
code; read-only for you, never commit there). Reuse its engine, not its UI:
- build system: `Makefile`, `tools/build.sh`, `tools/fetch-opengl-sdk.sh` (pins the SDK), `make app`, `make deploy PS5_HOST=<ip>` (WSL Ubuntu, clang-18);
- renderer: `src/gl_canvas.*`, `src/gl3d.*` (textured 3D case with reflection, shine, flip, spine/back render
  targets), `src/gfx`, `src/core`, `src/platform`, `src/runtime`, fonts/atlas baking in `tools/`;
- data: `src/library.*`, `src/cover_decode.c` (stb-based), `src/emulator_import.hpp` (title cleaning, Switch
  update/DLC guard, `title_key`), launch code in `src/main.cpp` (`direct_launch_job`, `queue_launch`);
- tools: `tools/ps5_covers_sync.py` (store portrait art), `tools/emulator_covers_sync.py` (GameTDB covers + backs for
  Switch/3DS/Wii U/Wii/DS), `tools/preview-estante-gl.sh` + `tools/preview-estante.cpp` (renders every screen on the PC
  with Mesa EGL: **this is how you iterate without the console**), `tests/`.
- `DESIGN.md`, `KNOWN-ISSUES.md`, `ESTANTE.md` there explain decisions and limits.

### 4.1 Decision gate for phase 0: native GL or the unchanged web renderer?

Boxshelf's renderer only needs `window.boxshelf`. Two ways to get 100 % parity:
- **A. Native C++/GL re-implementation** (Estante's route; proven on the console). Parity must be built by hand.
- **B. Embed a web engine and run `renderer/` unchanged**, implementing `window.boxshelf` natively. Free parity if
  the PS5 can host a WebKit view from homebrew and its CSS 3D is fast enough. Unproven: investigate for at most a
  short spike (can a homebrew app create a WebKit/NKWebKit view? input? 60 fps with the 3D shelf?). Record evidence.

Default if B is not clearly viable within the spike: **A**. Write the decision and its evidence in `PROGRESS.md`.

## 5. The owner's console and library (facts, as of 2026-11-10)

- PS5 OS address changes (DHCP); on 2026-11-10 it was `192.168.2.120` (scan `192.168.2.0/24` for port 6969 if it
  moved). Elf Arsenal `:6969`, FTP `:2121`, payload loader `:9021`.
- The external HD (exFAT) is `/mnt/usb0` (number may change). Layout (EmuDeck/Bazzite style, shared with the owner's
  Linux side): `Emulation/bios/{ps2,ps1,nds,switch/{keys,firmware}}`, `Emulation/roms/{ps2,psx,gc,wii,wiiu,3ds,nds}`, **Switch lives in `Emulation/eden/{keys,firmware,roms/<Game>/}`** (ProsperoEden's required
  layout: one folder per game with its update/DLC `.nsp` files beside the game), `Emulation/LEIA-ME.txt`. No symlinks on exFAT.
  PS1 games are one folder per game (`.cue/.bin/.chd`); Switch games are one folder per game containing the `.nsp`.
- Existing PS5 emulator apps (not yours; do not modify): PS5SX2 PPSA99203 (PS2), SwanStationPS5 PPSA98510 (PS1),
  Porpoise PPSA99764 (GameCube/Wii), PS5-ECAM PPSA99360 (Wii U/3DS/DS), ProsperoEden PPSA99008 (Switch),
  Elf Arsenal PPSA99000. Their config files, found by reading their configs and binaries (the SwanStation and PS5SX2 formats are inferred from strings in the binaries and were NOT confirmed by running the apps; verify before relying on them):
  - ProsperoEden `/data/prosperoeden/config/prosperoeden.json` key `game_files`; covers `/data/prosperoeden/covers/<hash>.tga|.name`;
  - PS5-ECAM `/data/ps5cemu/ps5cemu.json` `gamesFolder` (Wii U), `n3ds.gamesFolder`, `nds.gamesFolder`, `switch.gamesFolder`; library `/data/ps5cemu/library.json`;
  - SwanStationPS5 `/data/SwanStationPS5/SwanStationPS5.ini` `usb_folder=<folder name searched under /mnt/usbN/>`; covers `covers/boxart/<title>.png`, `covers/default/<serial>.jpg`;
  - PS5SX2 `/data/PCSX2/gs.ini`, section `PS5SX2`, `GameFolders=a;b` (semicolon list); covers `/data/PCSX2/covers/<serial>.jpg`;
  - Porpoise auto-searches `/data/porpoise/games`, `/data/games`, `/data/roms`, `/data/iso` and USB drives; "Add a Game Folder" in its settings.
  How Estante launches each emulator with a chosen game is in `estante-ps5/src/main.cpp`; read it, do not guess.
- PS5-side Estante scans the roots above plus `/mnt/usb0..2/Emulation/roms/*` (see its `*_roots` arrays).

## 6. Lessons from Estante's attempt (so you do not repeat them)

The owner tried to bend Estante toward Boxshelf for a week; these were wrong and he noticed on the TV:
1. **One 2:3 case for every system looked fake.** Boxshelf has one style per system (table above); the 3DS case is
   landscape. Build the per-system style table first.
2. **A square PS5 icon inside a portrait case = blurry bands / tiny art.** Real boxes have art that fills the face.
   PS5/PS4 need either real portrait art (the store's `PORTRAIT_BANNER`, via the PC tool) or a composed cover in the
   system's style (case frame + the `band` strip + the square art), never a small picture on a blur.
3. **Cases were ~10 % of their height thick**; Boxshelf's are about 6–7 % (`depth*ratio`). Thick spines look wrong.
4. The top menu in Boxshelf is not "Home / Library / Settings": it is the **system word + count (left), a glass
   capsule of system tabs (centre), counter and buttons (right)**; L1/R1 change system. The Library/Settings are
   reached from a Menu button.
5. Backdrop: blurred, tinted art of the focused game (cross-fade 0.8 s) + vignette + floor glow + halo. The shelf
   must run to both screen edges (about 7 cases per side) with a tight stack.
6. Per-cover cost matters: the PS5 has little RAM for textures (Estante keeps ~24 covers in memory). Plan a cache.
7. Games the owner has (see §5) are mostly square-icon / GameTDB art; plan the cover pipeline before the UI.

## 7. Roadmap (do them in order; each phase ends with a PC-preview render and a `PROGRESS.md` entry)

- **P0 Foundations.** Repo layout (suggest `ps5/` for the app, keep Boxshelf's sources untouched under their
  folders as reference), title id (`PPSA9xxxx`, pick one not used by Estante PPSA99841 / Trophy Manager PPSA99842 /
  emulators above), build + preview harness running, deploy script, decision gate §4.1. **Done when** an empty
  scene renders in the preview and `make app` produces a folder.
- **P1 Data.** Library from the roots in §5 (use Estante's importer logic), per-system style table, cover pipeline
  (portrait art, GameTDB covers + backs, composed covers per style, cache). **Done when** a unit-tested data layer
  lists the owner's games with the right system and a style for each.
- **P2 The shelf, 1:1.** Scene, `place()`, backdrop, halo, floor, top bar with tabs, info line, hints, input (D-pad,
  L1/R1 systems, A, B, Menu), letter jump, sort/filter panel. **Done when** screenshots match Boxshelf's mock in a browser
  side by side (capture both; list differences).
- **P3 Cases, 1:1.** Per-system anatomy (§3.1): frame, rim, glare, sheen, spine with band, bevels, back, reflections.
- **P4 Details / inspect.** Open case, disc, manual where data exists.
- **P5 Museum.** Port `museum.js` behaviour (rows per system, spine-out cases, picture lights, scenes).
- **P6 Settings, themes, sounds.** The app has **no audio layer yet**: investigate the PS5 audio API
  (Estante's SDK) before promising sounds. Themes from `themefx.js` / `customthemes.js`.
- **P7 Launch + polish.** Launch emulators from the shelf, recent/last played, setup/first run, localisation (pt-BR,
  en, es like Estante's `i18n.hpp`), performance on the console.

Checkpoints for the owner: after P2 and after P3 stop and ask him to look on the console (give him the exact
deploy command and what to look at).

## 8. How to work and be monitored

- Keep `PROGRESS.md` as an append-only log. One entry per work session:
  `## YYYY-MM-DD — phase — one-line title`, then: **Done**, **Files**, **Tests run** (commands + result),
  **Verified on** (unit / PC preview / console), **Screenshots** (paths under `.local/`), **Deviations**,
  **Open questions for the owner**, **Next step**. Update the phase checklist at the top of `PROGRESS.md`.
- Commit locally after each coherent step (no attribution lines). The monitoring assistant reads `PROGRESS.md` and
  `git log --stat`; if they disagree with reality it will say so.
- Prefer tests you can run on the PC (unit tests for the data layer, render-to-PNG for the UI). Look at your own
  screenshots; compare against the original running in a browser with `renderer/mock.js`.
- When blocked on something only the owner can decide, write it under "Open questions", pick the most
  Boxshelf-faithful default, and carry on.

## 9. Environment notes (this PC)

Windows 11 + Git Bash + PowerShell + WSL (Ubuntu, clang-18, make). Python is `py -3` on Windows (Pillow installed).
No `gh` CLI. Git Bash rewrites arguments that start with `/` into Windows paths: use `MSYS_NO_PATHCONV=1` when passing
console paths like `/mnt/usb0`. Use `curl` for Elf Arsenal when Python's `urllib` answers HTTP 400. Console FTP is
anonymous on 2121. Scratch/temp files go in a scratch folder, not in the repo.
