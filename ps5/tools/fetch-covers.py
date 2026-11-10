#!/usr/bin/env python3
# Boxshelf PS5 - copies the box art the console already has into .local/console/<console path>, read-only
# (HTTP GET to Elf Arsenal), so the PC preview can show real covers (preview mode real-<system>).
# The copies are the owner's own files for local testing: .local/ is never committed.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 tools/fetch-covers.py --host 192.168.2.120 [--systems gc,psx,sw]
import argparse
import ipaddress
import subprocess
import sys
import urllib.parse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=lambda v: str(ipaddress.IPv4Address(v)))
    parser.add_argument("--listings", default=".local/listings")
    parser.add_argument("--out", default=".local/console")
    parser.add_argument("--tool", default="build/tools/list-library")
    parser.add_argument("--systems", default="", help="comma-separated system keys (default: all)")
    args = parser.parse_args()
    wanted = {s for s in args.systems.split(",") if s}
    listing = subprocess.run([args.tool, "--covers", args.listings], capture_output=True, text=True, check=True)
    done = failed = 0
    for line in listing.stdout.splitlines():
        system, path = line.split("\t", 1)
        if wanted and system not in wanted:
            continue
        target = Path(args.out + path)
        if target.exists():
            continue
        url = f"http://{args.host}:6969/api/fs/download?path=" + urllib.parse.quote(path)
        r = subprocess.run(["curl", "-sf", "--max-time", "30", url], capture_output=True)
        if r.returncode != 0:
            failed += 1
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(r.stdout)
        done += 1
    print(f"covers copied: {done}, failed: {failed}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
