#!/usr/bin/env python3
# Boxshelf PS5 - sends the covers tools/boxshelf_covers.py downloaded to the console, into Boxshelf's own folder
# /data/boxshelf/covers (FTP, port 2121, anonymous). It WRITES to the console: run it only with the owner's go-ahead.
# It creates /data/boxshelf and /data/boxshelf/covers if needed and only ever adds or replaces files there; nothing
# else on the console is touched. --dry-run lists what would be sent.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 tools/upload-covers.py --host 192.168.2.120 [--dry-run]
import argparse
import ipaddress
import sys
from ftplib import FTP, error_perm
from pathlib import Path

REMOTE = "/data/boxshelf/covers"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", required=True, type=lambda v: str(ipaddress.IPv4Address(v)))
    ap.add_argument("--port", type=int, default=2121)
    ap.add_argument("--src", default=".local/boxshelf-covers")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    files = sorted(p for p in Path(args.src).iterdir() if p.suffix.lower() in (".png", ".jpg") and p.is_file())
    print(f"{len(files)} covers to send to {args.host}:{REMOTE}")
    if args.dry_run:
        for p in files[:20]:
            print("  ", p.name)
        return 0
    with FTP() as ftp:
        ftp.connect(args.host, args.port, timeout=30)
        ftp.login("anonymous", "boxshelf")
        for d in ("/data/boxshelf", REMOTE):
            try:
                ftp.mkd(d)
            except error_perm:
                pass  # already there
        existing = set()
        try:
            existing = {name for name, facts in ftp.mlsd(REMOTE) if facts.get("type") == "file"}
        except error_perm:
            pass
        sent = 0
        for p in files:
            temp = f"{REMOTE}/.{p.name}.upload"
            with p.open("rb") as f:
                ftp.storbinary(f"STOR {temp}", f, blocksize=256 * 1024)
            if p.name in existing:
                try:
                    ftp.sendcmd(f"DELE {REMOTE}/{p.name}")
                except error_perm:
                    pass
            ftp.rename(temp, f"{REMOTE}/{p.name}")
            sent += 1
        print(f"sent {sent} covers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
