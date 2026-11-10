#!/usr/bin/env bash
# Boxshelf PS5 - host checks. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
python3 - <<'PY'
import json, re
param = json.load(open("sce_sys/param.json", encoding="utf-8"))
title = param["titleId"]
# Title ids already used on the owner's console (PS5-PORT.md section 5); this app must not reuse one.
taken = {"PPSA99841", "PPSA99842", "PPSA99203", "PPSA98510", "PPSA99764", "PPSA99360", "PPSA99008", "PPSA99000"}
assert re.fullmatch(r"PPSA\d{5}", title), title
assert title not in taken, f"{title} belongs to another app"
assert title in param["contentId"] and param["conceptId"] == title[4:]
assert param["localizedParameters"]["en-US"]["titleName"] == "Boxshelf"
print(f"param.json ok: {title}")
PY
bash tools/validate-assets.sh
# Data layer unit tests, with sanitizers.
mkdir -p build/tests
clang-18 -O1 -g -fsanitize=address,undefined -c src/cJSON.c -o build/tests/cjson.o
clang++-18 -std=c++20 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc \
  tests/test_data.cpp src/data/*.cpp build/tests/cjson.o -o build/tests/test_data
build/tests/test_data
clang++-18 -std=c++20 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc \
  tests/test_shelf.cpp src/app/shelf.cpp src/data/*.cpp build/tests/cjson.o -o build/tests/test_shelf
build/tests/test_shelf
# The library tool over a made-up listings folder (same layout fetch-listings.py writes).
make --no-print-directory -s library-tool
fx=build/tests/listings
rm -rf "$fx" && mkdir -p "$fx/fs" "$fx/config"
echo '{"titles":[{"titleId":"PPSA01234","titleName":"Some PS5 Game"},{"titleId":"PPSA99841","titleName":"Estante"}]}' > "$fx/appdb.json"
echo '{"entries":[{"name":"Alpha (USA).iso","dir":false},{"name":"readme.txt","dir":false}]}' > "$fx/fs/_mnt_usb0_Emulation_roms_ps2.json"
echo '{"entries":[{"name":"Cart Racer","dir":true}]}' > "$fx/fs/_mnt_usb0_Emulation_eden_roms.json"
echo '{"entries":[{"name":"Pocket Quest (Europe).3ds","dir":false}]}' > "$fx/fs/_mnt_usb0_Emulation_roms_3ds.json"
echo '{"gamesFolder":"/mnt/usb1/Cemu Games"}' > "$fx/config/ps5cemu.json"
echo '{"entries":[{"name":"Island Party","dir":true}]}' > "$fx/fs/_mnt_usb1_Cemu%20Games.json"
out=$(build/tools/list-library "$fx")
for want in "covers found:" "roots listed: 4" "Alpha" "Cart Racer" "Pocket Quest" "Island Party" "Some PS5 Game" "Estante" \
            "case 1.127" "spine \"3DS\""; do
  grep -qF -- "$want" <<<"$out" || { echo "list-library: missing '$want'"; echo "$out"; exit 1; }
done
echo "list-library ok"
# The preview must build and render the empty scene.
rm -f .local/gl-gc.png
bash tools/preview-boxshelf-gl.sh gc >/dev/null 2>&1
test -s .local/gl-gc.png && echo "preview ok: .local/gl-gc.png"
