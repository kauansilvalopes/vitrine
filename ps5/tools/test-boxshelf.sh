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
# The preview must build and render the empty scene.
bash tools/preview-boxshelf-gl.sh empty >/dev/null 2>&1
test -s .local/gl-empty.png && echo "preview ok: .local/gl-empty.png"
