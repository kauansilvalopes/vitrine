// Boxshelf PS5 - downloads box art on the console, from the sources Boxshelf uses (libretro thumbnails, GameTDB), in
// the background, into the app's own storage. The shelf opens at once with what it has; covers arrive as they land.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "data/art_match.hpp"

#include <pthread.h>

#include <string>
#include <utility>
#include <vector>

namespace boxshelf::app
{
struct ArtJob
{
    std::string id, system, title, region, disc_id;
};

// The app's writable storage: /download0 on the console; $BOXSHELF_DOWNLOAD0 (a PC folder) for tests.
std::string storage_root();
// Where a game's downloaded cover lives (it may not exist yet).
std::string downloaded_cover_path(const std::string &game_id);

class ArtFetcher
{
  public:
    ArtFetcher();
    // Starts the worker once with the games that have no cover of Boxshelf's own yet.
    void start(std::vector<ArtJob> jobs);
    // Covers that arrived since the last call: (game id, local path).
    std::vector<std::pair<std::string, std::string>> take();
    // "Box art 12 / 60" while it works, "" when idle or done.
    std::string status();

  private:
    static void *run(void *self);
    void work();
    bool fetch_one(const ArtJob &job, const std::vector<std::string> &libretro_names);
    std::vector<std::string> libretro_names(const std::string &system);
    bool gametdb_ready(const std::string &system); // loads (or downloads, weekly) the title database into tdb_

    pthread_mutex_t lock_;
    std::vector<ArtJob> jobs_;
    std::vector<std::pair<std::string, std::vector<data::TdbGame>>> tdb_; // per system key
    std::vector<std::pair<std::string, std::string>> done_;
    std::string status_;
    bool started_ = false;
};
} // namespace boxshelf::app
