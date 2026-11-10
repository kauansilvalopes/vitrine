#!/usr/bin/env bash
# Boxshelf PS5 - renders screens through the GPU path (kit renderer on Mesa's surfaceless EGL) to .local/gl-*.png.
# Adapted from Estante's tools/preview-estante-gl.sh. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
mkdir -p build/preview .local
SDK=.deps/ps5-opengl/current/include
[[ -d $SDK ]] || bash tools/prepare-opengl.sh >/dev/null
clang-18 -O1 -c src/cJSON.c -o build/preview/cjson.o
clang++-18 -std=c++20 -O1 -g -DBOXSHELF_HOST_PREVIEW -DBOXSHELF_GL=1 -DGL_GLEXT_PROTOTYPES=1 \
  -Isrc -I$SDK -ffunction-sections -fdata-sections \
  tools/preview-boxshelf.cpp tools/gl_host_platform.cpp src/app/*.cpp src/data/*.cpp src/gl_canvas.cpp src/gl3d.cpp \
  src/gfx/*.cpp src/core/save_file.cpp src/core/input.cpp build/preview/cjson.o \
  -Wl,--gc-sections -pthread -l:libEGL.so.1 -l:libGL.so.1 -lm -o build/preview/preview-boxshelf-gl
modes=${*:-gc gc-first n3ds sw ps2 psx wii wiiu ps5 anim empty-shelf loading}
for mode in $modes; do
  build/preview/preview-boxshelf-gl ".local/gl-$mode.ppm" "$mode"
done
python3 - <<'PY'
from pathlib import Path
import struct, zlib
def chunk(t, b):
    return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
for p in Path('.local').glob('gl-*.ppm'):
    header, data = p.read_bytes().split(b'\n', 3)[:3], p.read_bytes().split(b'\n', 3)[3]
    w, h = 1920, 1080
    raw = b''.join(b'\0' + data[y * w * 3:(y + 1) * w * 3] for y in range(h))
    p.with_suffix('.png').write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                                      + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
    p.unlink()
    print(f'preview: {p.with_suffix(".png")}')
PY
