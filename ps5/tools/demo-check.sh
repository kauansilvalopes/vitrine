#!/usr/bin/env bash
# Boxshelf PS5 - checks that every demo game (tools/demo_library.cpp) finds its box art on libretro with the app's
# own matcher. Reads the libretro index pages (public, cached a week in .local/art-cache). Prints the misses.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
mkdir -p build/tools .local/art-cache
clang-18 -O1 -c src/cJSON.c -o build/tools/cjson.o
cat > build/tools/demo-list.cpp <<'CPP'
#include "demo_library.hpp"
#include <cstdio>
int main()
{
    for (const auto &g : boxshelf::app::demo_games())
        std::printf("%s\t%s\t%s\n", std::string(boxshelf::data::style(g.system).key).c_str(), g.title.c_str(), g.region.c_str());
}
CPP
clang++-18 -std=c++20 -O1 -w -Isrc -Itools build/tools/demo-list.cpp tools/demo_library.cpp src/data/*.cpp build/tools/cjson.o \
  -o build/tools/demo-list
clang++-18 -std=c++20 -O1 -w -Isrc tools/art-match-check.cpp src/data/*.cpp build/tools/cjson.o -o build/tools/art-match-check
build/tools/demo-list > .local/demo.tsv
python3 - <<'PY'
import sys
sys.path.insert(0, "tools")
from pathlib import Path
import boxshelf_covers as bc
for s in sorted({l.split("\t")[0] for l in open(".local/demo.tsv", encoding="utf-8")}):
    print(s, len(bc.libretro_names(s, Path(".local/art-cache"))), "names")
PY
build/tools/art-match-check .local/art-cache < .local/demo.tsv > .local/demo-match.tsv
total=$(wc -l < .local/demo.tsv)
miss=$(awk -F'\t' '$3==""' .local/demo-match.tsv | wc -l)
echo "demo: $((total - miss)) of $total titles have box art on libretro"
awk -F'\t' '$3=="" {print "  missing: " $1 "  " $2}' .local/demo-match.tsv
