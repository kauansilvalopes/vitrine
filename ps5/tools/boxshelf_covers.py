#!/usr/bin/env python3
# Boxshelf PS5 - box art from the same places Boxshelf takes it (main/covers.js), on the PC:
#   1. libretro thumbnails (Named_Boxarts: clean fronts, matched by name; no account)
#   2. the LaunchBox Games Database (Box - Front and its fallbacks; the only source with PS5 boxes; no account)
#   3. GameTDB (GameCube / Wii HD covers by disc id)
# Name matching is a port of Boxshelf's (lbindex.js norm, covers.js lookupNames / matchTitle / libretroMatch).
# Downloads go to .local/boxshelf-covers/<game id>.<ext> (never committed). Uploading them to the console is a
# separate, explicit step: tools/upload-covers.py (it writes to the console: the owner runs or approves it).
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   python3 tools/boxshelf_covers.py [--systems ps5,sw] [--only-missing] [--limit N]
import argparse
import html
import io
import json
import re
import subprocess
import sys
import time
import unicodedata
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

UA = "Boxshelf-PS5/0.1"
LIBRETRO = {"gc": "Nintendo - GameCube", "wii": "Nintendo - Wii", "psp": "Sony - PlayStation Portable",
            "wiiu": "Nintendo - Wii U", "sw": "Nintendo - Switch", "ps3": "Sony - PlayStation 3",
            "ps4": "Sony - PlayStation 4", "ps2": "Sony - PlayStation 2", "x360": "Microsoft - Xbox 360",
            "n3ds": "Nintendo - Nintendo 3DS", "psx": "Sony - PlayStation", "nds": "Nintendo - Nintendo DS"}
LAUNCHBOX = {"gc": "Nintendo GameCube", "wii": "Nintendo Wii", "psp": "Sony PSP", "wiiu": "Nintendo Wii U",
             "sw": "Nintendo Switch", "sw2": "Nintendo Switch 2", "ps3": "Sony Playstation 3",
             "ps4": "Sony Playstation 4", "ps5": "Sony Playstation 5", "ps2": "Sony Playstation 2",
             "x360": "Microsoft Xbox 360", "n3ds": "Nintendo 3DS", "psx": "Sony Playstation", "nds": "Nintendo DS",
             "custom": "Windows"}
LB_ZIP = "https://gamesdb.launchbox-app.com/Metadata.zip"
LB_IMG = "https://images.launchbox-app.com/"
FRONT_TYPES = ["Box - Front", "Box - Front - Reconstructed", "Fanart - Box - Front", "Steam Poster"]
BACK_TYPES = ["Box - Back", "Box - Back - Reconstructed"]
# covers.js: North American backs print English/French/Spanish side by side, so English-only regions come first
BACK_REGION_PREF = ["United Kingdom", "Australia", "Europe", "World", "North America", "United States", "Canada", "", None]
REGION_PREF = ["North America", "United States", "World", "", "Canada", "Europe", "United Kingdom", "Japan"]
TDB_SYS = {"gc": "wii", "wii": "wii", "wiiu": "wiiu"}
TDB_REGIONS = {"E": ["US"], "N": ["US"], "J": ["JA"], "K": ["KO"], "P": ["EN", "FR", "DE", "ES", "IT", "NL"],
               "D": ["DE", "EN"], "F": ["FR", "EN"], "S": ["ES", "EN"], "I": ["IT", "EN"], "H": ["NL", "EN"],
               "U": ["EN"]}

# ---------------------------------------------------------------- names (Boxshelf main/lbindex.js, covers.js)
ROMAN = {"ii": 2, "iii": 3, "iv": 4, "v": 5, "vi": 6, "vii": 7, "viii": 8, "ix": 9, "xi": 11, "xii": 12, "xiii": 13,
         "xiv": 14, "xv": 15, "xvi": 16, "xvii": 17, "xviii": 18, "xix": 19, "xx": 20}
ROMAN_RE = re.compile(r"\b(" + "|".join(sorted(ROMAN, key=len, reverse=True)) + r")\b")


def norm(s):
    s = unicodedata.normalize("NFD", str(s or "").lower())
    s = "".join(c for c in s if not unicodedata.combining(c))
    s = s.replace("&", " and ")
    s = re.sub(r"['’]", "", s)
    s = re.sub(r"[^a-z0-9]+", " ", s)
    s = re.sub(r"\b(the|a|an)\b", " ", s)
    s = ROMAN_RE.sub(lambda m: str(ROMAN[m.group(1)]), s)
    return re.sub(r"\s+", " ", s).strip()


EDITION_RE = re.compile(r"\s*(?:[-–—:+&]\s*)?\b(?:nintendo switch 2 edition|switch 2 edition|ns2 edition|"
                        r"definitive edition|complete edition|deluxe edition|game of the year(?: edition)?|"
                        r"goty(?: edition)?|dlc|all dlc|with dlc)\b.*$", re.I)


def lookup_names(title):
    out = []

    def add(t):
        t = re.sub(r"[\s\-–—:+&]+$", "", str(t or "")).strip()
        if t and not any(norm(x) == norm(t) for x in out):
            out.append(t)
    add(title)
    t = str(title or "")
    for _ in range(3):
        u = EDITION_RE.sub("", t)
        if u == t or not u.strip():
            break
        t = u
        add(t)
    return out


BRAND = re.compile(r"^(disney s|disneys|disney pixar|disney|pixar|nickelodeon|nick|mtv|cartoon network|dreamworks|"
                   r"activision|ea sports|ea|sega|capcom|konami|ubisoft|thq|namco)\s+")


def match_title(entries, title, art=lambda e: 1, exact_only=False):
    """entries: [{'n': normalised name, ...}] -> best entry or None (covers.js matchTitle)."""
    if not entries or not title:
        return None
    if re.match(r"^(dvd|disc|game|image|iso|default)$", title, re.I):
        return None
    want = norm(title)
    tight = want.replace(" ", "")
    if not want:
        return None

    def richest(arr):
        return max(arr, key=art) if arr else None
    exact = richest([g for g in entries if g["n"] == want]) or richest([g for g in entries if g["n"].replace(" ", "") == tight])
    if exact or exact_only:
        return exact

    def unbrand(x):
        for _ in range(2):
            x = BRAND.sub("", x)
        return x
    want2 = unbrand(want)
    if want2 != want:
        e2 = richest([g for g in entries if g["n"] == want2 or unbrand(g["n"]) == want2])
        if e2:
            return e2
    no_and = re.sub(r"\s+", " ", re.sub(r"\band\b", " ", want)).strip()
    if no_and != want:
        e3 = richest([g for g in entries if re.sub(r"\s+", " ", re.sub(r"\band\b", " ", g["n"])).strip() == no_and])
        if e3:
            return e3
    W = set(w for w in want2.split(" ") if w)
    want_w = [w for w in want2.split(" ") if w]
    best, best_score = None, 0.0
    for g in entries:
        score = 0.0
        n = g["n"]
        if n.startswith(want2 + " ") or want2.startswith(n + " "):
            extra = n[len(want2):] if len(n) > len(want2) else want2[len(n):]
            if not re.search(r"\b(\d+|[ivx]+)\b", extra):
                score = .9 - abs(len(n) - len(want2)) / 200
        else:
            G = [w for w in n.split(" ") if w]
            if G:
                hit = sum(1 for w in G if w in W)
                j = hit / len(W | set(G))
                if j >= .75:
                    score = j
                elif len(W) >= 3:
                    extra = [w for w in G if w not in W]
                    k = 0
                    for w in G:
                        if k < len(want_w) and w == want_w[k]:
                            k += 1
                    if k == len(want_w) and len(extra) <= 3 and not any(re.match(r"^(\d+|[ivx]+)$", w) for w in extra):
                        score = .8 - len(extra) * .03
        if score and not art(g):
            score -= .05
        if score > best_score:
            best_score, best = score, g
    return best


def match_name(entries, title, art=lambda e: 1):
    names = lookup_names(title)
    hit = match_title(entries, names[0], art)
    for n in names[1:]:
        if hit:
            break
        hit = match_title(entries, n, art, True)
    return hit


def clean_libretro(name):
    """The title part of a No-Intro / libretro name: no (..) / [..] tags."""
    t = re.sub(r"\s*[\(\[][^\)\]]*[\)\]]", " ", name)
    t = re.sub(r"\s+", " ", t).strip()
    m = re.match(r"^(.*), (The|A|An)( - .*)?$", t)  # "Legend of Zelda, The - X" -> "The Legend of Zelda - X"
    if m:
        t = f"{m.group(2)} {m.group(1)}{m.group(3) or ''}"
    return t


# ---------------------------------------------------------------- http


def get(url, timeout=60):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


# ---------------------------------------------------------------- libretro


def libretro_names(sys_id, cache):
    folder = LIBRETRO.get(sys_id)
    if not folder:
        return []
    f = cache / f"libretro-{sys_id}.json"
    if f.exists() and time.time() - f.stat().st_mtime < 7 * 86400:
        return json.loads(f.read_text())
    try:
        page = get(f"https://thumbnails.libretro.com/{urllib.parse.quote(folder)}/Named_Boxarts/").decode("utf-8", "replace")
    except Exception as e:
        print(f"  libretro {sys_id}: {e}")
        return []
    names = [urllib.parse.unquote(html.unescape(m))[:-4] for m in re.findall(r'href="([^"]+?\.png)"', page, re.I)]
    if names:
        f.write_text(json.dumps(names))
    return names


def libretro_match(names, game):
    if not names:
        return None
    entries = [{"n": norm(clean_libretro(n)), "name": n} for n in names]
    # prefer the game's region, then USA (libretroMatch)
    hits = []
    for t in lookup_names(game["title"]):
        hits = [e["name"] for e in entries if e["n"] == norm(t)]
        if hits:
            break
    if not hits:
        e = match_name(entries, game["title"])
        return e["name"] if e else None
    reg = (game.get("region") or "USA").lower()
    return next((h for h in hits if f"({reg}" in h.lower()), None) or next((h for h in hits if re.search(r"\(usa", h, re.I)), None) or hits[0]


# ---------------------------------------------------------------- LaunchBox


def tag(block, name):
    m = re.search(f"<{name}>([\\s\\S]*?)</{name}>", block)
    return html.unescape(m.group(1)).strip() if m else ""


def launchbox_index(cache, platforms):
    idx_file = cache / "launchbox-index.json"
    if idx_file.exists() and time.time() - idx_file.stat().st_mtime < 60 * 86400:
        idx = json.loads(idx_file.read_text())
        if set(platforms) <= set(idx["games"]):
            return idx
    zip_file = cache / "Metadata.zip"
    if not zip_file.exists():
        print("  LaunchBox: downloading Metadata.zip (~110 MB, once every 60 days)")
        req = urllib.request.Request(LB_ZIP, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=600) as r, open(str(zip_file) + ".part", "wb") as out:
            while chunk := r.read(1 << 20):
                out.write(chunk)
        Path(str(zip_file) + ".part").rename(zip_file)
    print("  LaunchBox: indexing (a few seconds)")
    games = {p: [] for p in platforms}
    images, ids = {}, set()
    with zipfile.ZipFile(zip_file) as z:
        with z.open("Metadata.xml") as raw:
            text = io.TextIOWrapper(raw, encoding="utf-8")
            buf = ""
            while True:
                chunk = text.read(4 << 20)
                if not chunk:
                    break
                buf += chunk
                pos = 0
                while True:
                    i = buf.find("<Game", pos)
                    if i < 0 or len(buf) - i < 11:
                        pos = max(pos, len(buf) - 16) if i < 0 else i
                        break
                    close = "</Game>" if buf.startswith("<Game>", i) else "</GameImage>" if buf.startswith("<GameImage>", i) else None
                    if not close:
                        pos = i + 5
                        continue
                    end = buf.find(close, i)
                    if end < 0:
                        pos = i
                        break
                    block = buf[i:end]
                    pos = end + len(close)
                    if close == "</Game>":
                        plat = tag(block, "Platform")
                        if plat in games:
                            gid, name = tag(block, "DatabaseID"), tag(block, "Name")
                            if gid and name:
                                ids.add(gid)
                                games[plat].append({"id": gid, "name": name, "n": norm(name)})
                    else:
                        gid = tag(block, "DatabaseID")
                        if gid in ids:
                            t = tag(block, "Type")
                            if re.match(r"^(Box - (Front|Back)( - Reconstructed)?|Box - Spine|Fanart - Box - Front|Steam Poster)$", t):
                                images.setdefault(gid, []).append({"type": t, "region": tag(block, "Region"), "file": tag(block, "FileName")})
                buf = buf[pos:]
    idx = {"games": games, "images": images}
    idx_file.write_text(json.dumps(idx))
    return idx


def launchbox_front(idx, game, types=None, regions=None):
    plat = LAUNCHBOX.get(game["system"])
    entries = idx["games"].get(plat) if plat else None
    if not entries:
        return None
    art = lambda e: len([i for i in idx["images"].get(e["id"], []) if i["type"].startswith("Box - ")])
    g = match_name(entries, game["title"], art)
    if not g and re.search(r"\s(\+|&|and)\s", game["title"], re.I):
        # Boxshelf tries the part before " and " with its close calls too; that took "Indiana Jones and the Staff of
        # Kings" to "Indiana Jones and the Great Circle". Here the shorter name must match exactly.
        g = match_title(entries, re.split(r"\s(?:\+|&|and)\s", game["title"], flags=re.I)[0], art, True)
    if not g:
        return None
    imgs = idx["images"].get(g["id"], [])
    pref = regions or REGION_PREF
    for t in types or FRONT_TYPES:
        c = [i for i in imgs if i["type"] == t and i["file"]]
        c.sort(key=lambda i: pref.index(i["region"]) if i["region"] in pref else 99)
        if c:
            return LB_IMG + urllib.parse.quote(c[0]["file"]), g["name"]
    return None


MAX_SIDE = 1200  # the console decodes up to 2400 px a side and shows a cover ~550 px tall: keep files small


def shrink(data):
    """Any picture -> a JPEG no larger than MAX_SIDE on its long side (transparency flattened on dark grey)."""
    from PIL import Image  # Pillow
    im = Image.open(io.BytesIO(data))
    im.load()
    if im.mode in ("RGBA", "LA", "P"):
        im = im.convert("RGBA")
        bg = Image.new("RGB", im.size, (22, 26, 32))
        bg.paste(im, mask=im.getchannel("A"))
        im = bg
    else:
        im = im.convert("RGB")
    if max(im.size) > MAX_SIDE:
        im.thumbnail((MAX_SIDE, MAX_SIDE), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, "JPEG", quality=90, optimize=True)
    return buf.getvalue()


def shrink_existing(out):
    for p in sorted(out.iterdir()):
        if p.suffix.lower() not in (".png", ".jpg"):
            continue
        data = p.read_bytes()
        from PIL import Image
        with Image.open(io.BytesIO(data)) as im:
            fine = p.suffix == ".jpg" and im.format == "JPEG" and max(im.size) <= MAX_SIDE
        if not fine:
            p.unlink()
            p.with_suffix(".jpg").write_bytes(shrink(data))


# ---------------------------------------------------------------- main


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--listings", default=".local/listings")
    ap.add_argument("--tool", default="build/tools/list-library")
    ap.add_argument("--out", default=".local/boxshelf-covers")
    ap.add_argument("--cache", default=".local/art-cache")
    ap.add_argument("--systems", default="")
    ap.add_argument("--only-missing", action="store_true", help="only games with no cover on the console")
    ap.add_argument("--no-launchbox", action="store_true")
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()
    out, cache = Path(args.out), Path(args.cache)
    out.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    shrink_existing(out)  # files from an earlier run kept their original size
    games = json.loads(subprocess.run([args.tool, "--json", args.listings], capture_output=True, text=True, check=True).stdout)
    wanted = {s for s in args.systems.split(",") if s}
    games = [g for g in games if (not wanted or g["system"] in wanted) and g["system"] != "media"
             and not (args.only_missing and g["has_cover"])]
    if args.limit:
        games = games[:args.limit]
    lb = None
    if not args.no_launchbox:
        try:
            lb = launchbox_index(cache, sorted({LAUNCHBOX[g["system"]] for g in games if g["system"] in LAUNCHBOX}))
        except Exception as e:
            print(f"  LaunchBox unavailable: {e}")
    names = {}
    report = []
    for g in games:
        key = g["id"].replace(":", "_")
        # the back (LaunchBox "Box - Back"): the app shows it when the case is flipped
        if lb and not (out / f"{key}-back.jpg").exists():
            r = launchbox_front(lb, g, BACK_TYPES, BACK_REGION_PREF)
            if r:
                try:
                    (out / f"{key}-back.jpg").write_bytes(shrink(get(r[0])))
                    report.append((g, "back", f"launchbox: {r[1]}"))
                except Exception as e:
                    report.append((g, "fail", f"back ({e})"))
                time.sleep(0.15)
        if any((out / f"{key}{e}").exists() for e in (".png", ".jpg")):
            report.append((g, "kept", ""))
            continue
        url = how = None
        # 1. libretro (clean front)
        if g["system"] in LIBRETRO:
            names.setdefault(g["system"], libretro_names(g["system"], cache))
            hit = libretro_match(names[g["system"]], g)
            if hit:
                url = f"https://thumbnails.libretro.com/{urllib.parse.quote(LIBRETRO[g['system']])}/Named_Boxarts/{urllib.parse.quote(hit + '.png')}"
                how = f"libretro: {hit}"
        # 2. LaunchBox
        if not url and lb:
            r = launchbox_front(lb, g)
            if r:
                url, how = r[0], f"launchbox: {r[1]}"
        # 3. GameTDB by disc id
        if not url and g["system"] in TDB_SYS and len(g.get("disc_id", "")) == 6:
            for reg in TDB_REGIONS.get(g["disc_id"][3], ["US", "EN"]):
                url = f"https://art.gametdb.com/{TDB_SYS[g['system']]}/coverHQ/{reg}/{g['disc_id']}.png"
                how = f"gametdb: {g['disc_id']} {reg}"
                break
        if not url:
            report.append((g, "none", ""))
            continue
        try:
            data = get(url)
            (out / f"{key}.jpg").write_bytes(shrink(data))
            report.append((g, "ok", how))
        except Exception as e:
            report.append((g, "fail", f"{how} ({e})"))
        time.sleep(0.15)  # be gentle with the free services
    for g, state, how in report:
        print(f"[{state:4}] {g['system']:5} {g['title'][:50]:50} {how}")
    counts = {s: sum(1 for r in report if r[1] == s) for s in ("ok", "back", "kept", "none", "fail")}
    print(f"\ncovers: {counts}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
