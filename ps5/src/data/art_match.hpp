// Boxshelf PS5 - finding a game in an art source's list by name, as Boxshelf does it (main/lbindex.js norm,
// main/covers.js lookupNames / matchTitle / libretroMatch). The same rules as tools/boxshelf_covers.py.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace boxshelf::data
{
// lbindex.js norm(): lower case, accents off, "&" -> "and", apostrophes out, punctuation -> spaces, no the/a/an,
// roman numerals ii..xx -> numbers, single spaces.
std::string art_norm(std::string_view s);
// covers.js lookupNames(): the title, then the title without an edition tail ("Definitive Edition", "+ DLC"...).
std::vector<std::string> lookup_names(std::string_view title);

struct ArtEntry
{
    std::string norm; // art_norm(name)
    std::string name; // as the source spells it
    int art = 1;      // how much art it has (tie-breaker; 0 = none)
};
// covers.js matchTitle(): exact, the same without spaces, without a brand prefix, without "and", then close calls.
// Returns the index in entries, or -1.
int match_title(const std::vector<ArtEntry> &entries, std::string_view title, bool exact_only = false);
// covers.js matchName(): matchTitle on the title, then exact matches of its shorter names.
int match_name(const std::vector<ArtEntry> &entries, std::string_view title);

// The title part of a No-Intro / libretro name: tags dropped, ", The" moved to the front.
std::string libretro_title(std::string_view name);
// libretro thumbnails: the Named_Boxarts index page -> the picture names (without ".png").
std::vector<std::string> parse_libretro_index(std::string_view html);
// covers.js libretroMatch(): the best name for a game, preferring its region, then USA. "" when none.
std::string libretro_match(const std::vector<std::string> &names, std::string_view title, std::string_view region);
// The libretro thumbnail folder of a system ("Sony - PlayStation 2"); "" when libretro has none (PS5, apps).
std::string_view libretro_system(std::string_view system_key);
// "Named_Boxarts/<name>.png" URL of a picture.
std::string libretro_url(std::string_view system_key, std::string_view name);

// GameTDB (main/gametdb.js): its title database for Switch / 3DS, boxed games only.
struct TdbGame
{
    std::string norm, id, region; // art_norm(English title), GameTDB id, its region ("NTSC-U", "PAL", "ALL"...)
};
// The <game> entries of switchtdb.xml / 3dstdb.xml whose <type> is one of types ("Switch"; "3DS","New3DS").
std::vector<TdbGame> parse_gametdb(std::string_view xml, const std::vector<std::string> &types);
// GameTDB's art folder of a system ("switch", "3ds"); "" when it has no title database here.
std::string_view gametdb_art(std::string_view system_key);
// Front cover URLs for a game, best first: exact title matches only (gametdb.js fullCovers), HD then normal.
std::vector<std::string> gametdb_front_urls(const std::vector<TdbGame> &db, std::string_view system_key,
                                            std::string_view title);
} // namespace boxshelf::data
