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
}


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

    roots = subprocess.run([args.tool, "--roots", str(out)], capture_output=True, text=True, check=True)
    for root in roots.stdout.splitlines():
        body = get(args.host, "/api/fs/list?path=" + urllib.parse.quote(root))
        if body is not None:
            (out / "fs" / (escape(root) + ".json")).write_bytes(body)
        print(f"list {root}: {'ok' if body is not None else '-'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
