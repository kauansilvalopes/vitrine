#!/usr/bin/env python3
# Boxshelf PS5 - saves what the data layer needs from the console, read-only (HTTP GET to Elf Arsenal only):
# /appdb, the emulators' config files and a folder listing per root. Nothing is written to the console.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 tools/fetch-listings.py --host 192.168.2.120 [--out .local/listings]
#   then: build/tools/list-library .local/listings
import argparse
import ipaddress
import subprocess
import sys
import urllib.parse
from pathlib import Path

CONFIGS = {
    "ps5cemu.json": "/data/ps5cemu/ps5cemu.json",
    "gs.ini": "/data/PCSX2/gs.ini",
    "SwanStationPS5.ini": "/data/SwanStationPS5/SwanStationPS5.ini",
    "prosperoeden.json": "/data/prosperoeden/config/prosperoeden.json",
    "porpoise-disc-cache.tsv": "/data/porpoise/disc-cache.tsv",
    # cover index (src/data/cover_index.hpp)
    "swan-serials.tsv": "/data/SwanStationPS5/cache/serials.tsv",
    "ps2-serials.txt": "/data/PCSX2/chd-serials.txt",
    "ps2-titles.tsv": "/data/homebrew/PPSA99841/assets/ps2-titles.tsv",
    "ecam-library.json": "/data/ps5cemu/library.json",
}
# Folders whose listing tells which cover files exist.
COVER_DIRS = [
    "/data/SwanStationPS5/covers/default", "/data/PCSX2/covers", "/data/porpoise/covers",
    "/data/prosperoeden/covers", "/data/ps5cemu/covers/boxart/3ds", "/data/ps5cemu/covers/3ds", "/user/appmeta",
]


def escape(path: str) -> str:
    out = []
    for byte in path.encode("utf-8"):
        c = chr(byte)
        if c == "/":
            out.append("_")
        elif c.isascii() and (c.isalnum() or c in ".-"):
            out.append(c)
        else:
            out.append(f"%{byte:02X}")
    return "".join(out)


def get(host: str, path: str) -> bytes | None:
    # curl, not urllib: Elf Arsenal has answered urllib with HTTP 400 (PS5-PORT.md section 9).
    url = f"http://{host}:6969{path}"
    result = subprocess.run(["curl", "-sf", "--max-time", "20", url], capture_output=True)
    return result.stdout if result.returncode == 0 else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=lambda v: str(ipaddress.IPv4Address(v)))
    parser.add_argument("--out", default=".local/listings")
    parser.add_argument("--tool", default="build/tools/list-library")
    args = parser.parse_args()
    out = Path(args.out)
    (out / "fs").mkdir(parents=True, exist_ok=True)
    (out / "config").mkdir(parents=True, exist_ok=True)

    appdb = get(args.host, "/appdb")
    if appdb is None:
        print(f"no answer from http://{args.host}:6969/appdb", file=sys.stderr)
        return 1
    (out / "appdb.json").write_bytes(appdb)
    for name, remote in CONFIGS.items():
        body = get(args.host, "/api/fs/download?path=" + urllib.parse.quote(remote))
        print(f"config {remote}: {'saved' if body is not None else 'not found'}")
        if body is not None:
            (out / "config" / name).write_bytes(body)

    import json
    (out / "eden").mkdir(exist_ok=True)
    for folder in COVER_DIRS:
        body = get(args.host, "/api/fs/list?path=" + urllib.parse.quote(folder))
        print(f"covers {folder}: {'ok' if body is not None else '-'}")
        if body is None:
            continue
        (out / "fs" / (escape(folder) + ".json")).write_bytes(body)
        if folder == "/data/prosperoeden/covers":
            for entry in json.loads(body).get("entries", []):
                name = entry.get("name", "")
                if name.endswith(".name"):
                    text = get(args.host, "/api/fs/download?path=" + urllib.parse.quote(f"{folder}/{name}"))
                    if text is not None:
                        (out / "eden" / name).write_bytes(text)
    for title_id in [t.get("titleId", "") for t in json.loads(appdb).get("titles", [])]:
        if title_id[:4] in ("PPSA", "CUSA"):
            body = get(args.host, "/api/fs/list?path=" + urllib.parse.quote(f"/user/appmeta/{title_id}"))
            if body is not None:
                (out / "fs" / (escape(f"/user/appmeta/{title_id}") + ".json")).write_bytes(body)

    roots = subprocess.run([args.tool, "--roots", str(out)], capture_output=True, text=True, check=True)
    for root in roots.stdout.splitlines():
        body = get(args.host, "/api/fs/list?path=" + urllib.parse.quote(root))
        if body is not None:
            (out / "fs" / (escape(root) + ".json")).write_bytes(body)
        print(f"list {root}: {'ok' if body is not None else '-'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
