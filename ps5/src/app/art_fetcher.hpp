// Boxshelf PS5 - downloads box art on the console, from the sources Boxshelf uses (libretro thumbnails, GameTDB), in
// the background, into the app's own storage. The shelf opens at once with what it has; covers arrive as they land.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "data/art_match.hpp"
#include "data/scan.hpp"

#include <pthread.h>

#include <string>
#include <utility>
#include <vector>

namespace boxshelf::app
{
struct ArtJob
{
    std::string id, system, title, region, disc_id;
    bool front = true; // look for the front
    bool back = false; // look for the back (cut from GameTDB's full cover: GameCube, Wii, Switch, 3DS)
};

// The app's writable storage: /download0 on the console; $BOXSHELF_DOWNLOAD0 (a PC folder) for tests.
std::string storage_root();
// Where a game's downloaded cover lives (it may not exist yet).
std::string downloaded_cover_path(const std::string &game_id);
std::string downloaded_back_path(const std::string &game_id);
// True for the systems whose backs the fetcher can cut from a full cover scan.
bool back_source(std::string_view system_key);
// The work for these games. everything: fetch again even what is already here (Settings: Download all again);
// otherwise only what is missing (the front when there is no Boxshelf cover, the back when there is no back art).
std::vector<ArtJob> art_jobs(const std::vector<data::Game> &games, bool everything);

class ArtFetcher
{
  public:
    ArtFetcher();
    // Starts the worker once with the games that have no cover of Boxshelf's own yet.
    void start(std::vector<ArtJob> jobs);
    // Settings: a new run (games not found this week are asked again). False while a run is still going.
    bool restart(std::vector<ArtJob> jobs);
    bool running();
    // Covers that arrived since the last call: (game id, local path); a back's id ends in "|back".
    std::vector<std::pair<std::string, std::string>> take();
    // "Box art 12 / 60" while it works, "" when idle or done.
    std::string status();

  private:
    static void *run(void *self);
    void work();
    bool fetch_one(const ArtJob &job, const std::vector<std::string> &libretro_names);
    bool fetch_back(const ArtJob &job);
    void launch();
    std::vector<std::string> libretro_names(const std::string &system);
    bool gametdb_ready(const std::string &system); // loads (or downloads, weekly) the title database into tdb_

    pthread_mutex_t lock_;
    std::vector<ArtJob> jobs_;
    std::vector<std::pair<std::string, std::vector<data::TdbGame>>> tdb_; // per system key
    std::vector<std::pair<std::string, std::string>> done_;
    std::string status_;
    bool started_ = false, running_ = false, retry_misses_ = false;
    int found_ = 0;
};
} // namespace boxshelf::app
