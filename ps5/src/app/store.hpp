// Boxshelf PS5 - where Vitrine keeps what it downloads (covers, LaunchBox and libretro lists) and its settings.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The app's own download data (/download0) is wiped by the console when the app crashes or is updated, and the
// covers had to be fetched again each time. On the console the store is /data/vitrine instead: written directly
// when the app may, otherwise through Elf Arsenal (FTP on 2121 to write, its HTTP API to read and list). If neither
// works it falls back to /download0/boxshelf. On the PC it is $BOXSHELF_DOWNLOAD0/boxshelf.
#pragma once

#include <cstddef>
#include <string>

namespace boxshelf::app
{
// Picks the place, once, before anything reads or writes (safe to call again).
void store_init();
// The folder ("/data/vitrine"); paths below it go through the functions here.
std::string store_root();
// How the store is reached, for the log: "direct", "elf" or "download0".
const char *store_mode();

bool store_read(const std::string &path, std::string &out, std::size_t max = 64u << 20);
// The whole file, replaced safely (written aside, then renamed over the old one).
bool store_write(const std::string &path, const std::string &data);
// true when the file is there; its size and modification time (unix seconds) when asked.
bool store_stat(const std::string &path, long long *size = nullptr, long long *mtime = nullptr);
void store_mkdirs(const std::string &dir);
} // namespace boxshelf::app
