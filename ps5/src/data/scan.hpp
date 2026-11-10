// Boxshelf PS5 - turning folder listings into games on shelves. SPDX-License-Identifier: GPL-3.0-or-later
//
// Pure functions only: no file system or network access here, so every rule runs in host unit tests. On the
// console the listings come from Elf Arsenal's /api/fs/list (the app is sandboxed); on the PC from a directory.
#pragma once

#include "data/systems.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace boxshelf::data
{
struct DirEntry
{
    std::string name;
    bool dir = false;
};

enum class RootKind : std::uint8_t
{
    files,         // one game per file with a known extension
    folders,       // one game per sub-folder (its name is the title)
    files_folders, // either
};

struct Root
{
    std::string path;
    System system = System::count; // System::count with gc_or_wii: decided per game
    RootKind kind = RootKind::files;
    bool gc_or_wii = false; // Porpoise's own folders mix GameCube and Wii discs
};

struct Game
{
    std::string id;    // stable: system key + FNV-1a of the path
    std::string title; // what the shelf shows
    System system = System::count;
    std::string path;      // file or folder on the console
    std::string app;       // title id of the PS5 app that runs it (emulator), or the game's own title id
    std::string disc_id;   // GameCube/Wii disc id or PS1/PS2 serial when known
    std::string region;    // "USA", "EUR"... when the emulator knows it
    bool folder = false;
    // Box art already on the console (see data/cover_index.hpp); empty when none.
    std::string cover_front, cover_back, cover_spine;
    bool cover_square = false; // an icon, not box art: composed into the system's cover style
    std::string cover_fallback; // tried when cover_front cannot be read
    bool fallback_square = false; // the fallback is a square icon (cropped to fill), not box art
};

// ---- rules ----
// main/systems.js extensions, narrowed to what the PS5 emulator for that system opens.
bool is_game_file(System system, std::string_view name) noexcept;
// Switch: true for a base game file; false for updates and DLC ("[0100...800]", "[UPD]", "Update", "DLC").
bool is_switch_base(std::string_view name) noexcept;
// GameCube or Wii from a disc id's first letter (G, D, P = GameCube; R, S, H, W = Wii); System::count if unknown.
System gc_or_wii_from_disc_id(std::string_view disc_id) noexcept;
// Readable title from a file name (Estante's clean_rom_title: no extension, no (..)/[..] groups, ", The" moved).
// is_file = false for a folder name: nothing after its last dot is taken for an extension.
std::string clean_title(std::string_view name, bool is_file = true);
std::string make_game_id(System system, std::string_view path);
// Estante's same_title(): equal but for punctuation, spacing and case ("Bowsers Fury" == "Bowser's Fury").
bool same_title(std::string_view a, std::string_view b);
// Estante's meaningless_title(): shorter than 3, or 8+ characters of only hex digits.
bool meaningless(std::string_view title);
// Title id of the PS5 emulator app for a system (PS5-PORT.md section 5); empty when none.
std::string_view emulator_app(System system) noexcept;

// ---- roots ----
// The owner's HD layout (/mnt/usbN/Emulation/...) and the emulators' own folders (Estante's *_roots).
std::vector<Root> default_roots();
// Extra roots from the emulators' settings. Formats read from their configs/binaries, not confirmed by running them.
std::vector<Root> roots_from_ps5cemu_json(std::string_view json);  // gamesFolder, n3ds/nds/switch.gamesFolder
std::vector<Root> roots_from_ps5sx2_ini(std::string_view ini);     // [PS5SX2] GameFolders=a;b
std::vector<Root> roots_from_swanstation_ini(std::string_view ini); // usb_folder=<name> under /mnt/usb0..2

// ---- listings ----
// Games found in one root's listing.
std::vector<Game> scan_listing(const Root &root, const std::vector<DirEntry> &entries);
// Elf Arsenal /api/fs/list body: {"entries":[{"name":..,"dir":bool},..]}. Returns false on bad JSON.
bool parse_fs_list(std::string_view json, std::vector<DirEntry> &out);
// Elf Arsenal /appdb body: {"titles":[{"titleId":..,"titleName":..},..]} -> PS5 / PS4 games and apps. Homebrew
// title ids (PPSA9xxxx) go to the Media shelf; this app itself is left out.
std::vector<Game> games_from_appdb(std::string_view json);
// PPSA9xxxx titles that are tools / emulators (-> Media); other PPSA9xxxx are ports (-> Custom).
bool is_homebrew_app(std::string_view title_id) noexcept;
// Retail streaming apps (-> Media).
bool is_streaming_app(std::string_view title_id) noexcept;

// ---- library ----
// The order Boxshelf's scanner gives a shelf: case-insensitive, numbers by value ("Game 2" before "Game 10").
bool title_less(std::string_view a, std::string_view b);
struct Shelf
{
    System system;
    std::vector<Game> games; // sorted by title
};
// Merges games (same path once), groups them per system in shelf order and sorts each shelf by title. A shelf with
// no game is left out, whatever emulators are installed.
std::vector<Shelf> build_shelves(const std::vector<Game> &games);
} // namespace boxshelf::data
