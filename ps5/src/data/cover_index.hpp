// Boxshelf PS5 - which cover file on the console belongs to which game. SPDX-License-Identifier: GPL-3.0-or-later
//
// The emulators already keep box art on the console, keyed by what they read from the disc. These are the files
// they leave (all read-only here, through Elf Arsenal):
//   PS1  SwanStation  /data/SwanStationPS5/cache/serials.tsv  path, size, mtime, serial
//                     /data/SwanStationPS5/covers/default/<serial>.jpg
//   PS2  PS5SX2       /data/PCSX2/chd-serials.txt              path, size, mtime, serial
//                     /data/PCSX2/covers/<serial>.jpg, <serial>_spine.jpg, <serial>_back.jpg
//                     titles: Estante's /data/homebrew/PPSA99841/assets/ps2-titles.tsv (serial, title), if present
//   GC/Wii Porpoise   /data/porpoise/disc-cache.tsv  path, size, mtime, 0, disc id, 0, title, format, system, region
//                     /data/porpoise/covers/<id>.png, <id>.back.png, <id>.spine.png
//   Switch Eden       /data/prosperoeden/covers/<hash>.tga + <hash>.name (the title)
//   3DS  PS5-ECAM     /data/ps5cemu/library.json  {"3ds":[{titleId, gameId, name, path}]}
//                     /data/ps5cemu/covers/boxart/3ds/<gameId>.tga (box), covers/3ds/<titleId>.tga (icon)
//   PS5/PS4 system    /user/appmeta/<titleId>/icon0.png (square), pic0.png (1920x1080 key art)
// Paths in the caches can be older homes of the same files (/mnt/ext1/...), so games match on their last path
// component (the file, or the folder for one-folder-per-game systems).
#pragma once

#include "data/scan.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace boxshelf::data
{
class CoverIndex
{
  public:
    void add_swan_serials(std::string_view tsv);
    void add_ps2_serials(std::string_view tsv);
    void add_ps2_titles(std::string_view tsv);
    void add_porpoise_cache(std::string_view tsv);
    void add_eden_name(std::string_view hash, std::string_view title);
    void add_ecam_library(std::string_view json);
    // Boxshelf's own covers (tools/boxshelf_covers.py: libretro / LaunchBox / GameTDB, the sources Boxshelf uses),
    // /data/boxshelf/covers/<game id with ':' as '_'>.png|jpg: they win over the emulators' art.
    void add_boxshelf_covers(const std::vector<std::string> &file_names);

    // Fills game.cover_* / disc_id / region, a better title where the emulator knows one (PS2 serial titles,
    // Porpoise for names that say nothing) and GameCube vs Wii from the disc id. Returns true if a cover was found.
    bool resolve(Game &game) const;

  private:
    bool resolve_emulator(Game &game) const;
    std::unordered_map<std::string, std::string> swan_serial_; // folder name (lower) -> serial
    std::unordered_map<std::string, std::string> ps2_serial_;  // file name (lower) -> serial
    std::unordered_map<std::string, std::string> ps2_title_;   // serial -> title
    struct Disc
    {
        std::string id, title, system, region;
    };
    std::unordered_map<std::string, Disc> porpoise_; // file name (lower) -> disc
    std::vector<std::pair<std::string, std::string>> eden_; // (hash, title)
    struct Ecam
    {
        std::string title_id, game_id, name;
    };
    std::unordered_map<std::string, Ecam> ecam_; // file name (lower) -> entry
    std::unordered_map<std::string, std::string> own_; // game key -> file name in /data/boxshelf/covers
};

// The file name (without extension) a game's own cover has in /data/boxshelf/covers: its id with ':' as '_'.
std::string cover_key(const std::string &game_id);
// Last path component, lower-cased ASCII.
std::string leaf_key(std::string_view path);
// Folder of a one-folder-per-game entry inside a cache path ("/x/games/<Folder>/<file>" -> "<folder>"), lower-cased.
std::string parent_key(std::string_view path);
} // namespace boxshelf::data
