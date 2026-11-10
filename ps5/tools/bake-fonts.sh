#!/usr/bin/env bash
# Boxshelf PS5 (from Estante) - Rebuild the baked SDF font (assets/fonts/inter.huifont) from third_party/inter/Inter-Medium.ttf.
# The baker comes from ps5-homebrew-ui (tools/font-baker); "european" adds accented Latin, Greek and
# Cyrillic so game names written in those scripts draw. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cxx=$(command -v "${HOST_CXX:-clang++}")
mkdir -p "$root/build/host" "$root/assets/fonts"
"$cxx" -std=c++20 -O2 -w "$root/tools/font-baker/bake_font.cpp" -o "$root/build/host/bake_font"
"$root/build/host/bake_font" "$root/third_party/inter/Inter-Medium.ttf" "$root/assets/fonts/inter.huifont" 56 8 2048 european
# Boxshelf's fonts (renderer/fonts, SIL OFL 1.1), TTFs converted from its latin woff2 files with fontTools:
# Lexend 500 (tabs, titles, hints), 600 (spines), 800 (default system word); the per-shelf wordmark faces
# (style.css .wm-<id> .wm-main). Inter stays as the fallback for other scripts.
src="$root/third_party/boxshelf-fonts"
for face in Lexend-500 Lexend-600 Lexend-800; do
  "$root/build/host/bake_font" "$src/$face.ttf" "$root/assets/fonts/${face,,}.huifont" 56 8 1024 european >/dev/null 2>&1
done
for face in ChakraPetch-700 Syne-800 Unbounded-700 Michroma-400 Audiowide-400 DMSerifDisplay-400-italic Bungee-400; do
  "$root/build/host/bake_font" "$src/$face.ttf" "$root/assets/fonts/${face,,}.huifont" 64 8 1024 basic >/dev/null 2>&1
done
cp "$src"/*-OFL.txt "$root/assets/fonts/"
ls -la "$root/assets/fonts"
