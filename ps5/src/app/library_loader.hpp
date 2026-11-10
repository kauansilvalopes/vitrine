// Boxshelf PS5 - builds the library in the background: /appdb, the emulators' configs, every root's listing.
// Read-only: only GET requests. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app/art_fetcher.hpp"
#include "app/shelf.hpp"

#include <atomic>
#include <string>
#include <vector>

namespace boxshelf::app
{
class LibraryLoader
{
  public:
    // Starts the worker thread (once). moves: games the owner put on another shelf (game id -> system key).
    void start(std::vector<std::pair<std::string, std::string>> moves = {});
    // True once, when the shelves are ready to take; jobs: the games whose box art should be downloaded.
    bool take(std::vector<Shelf> &shelves, std::vector<ArtJob> &jobs);
    // What the worker is doing ("Reading /mnt/usb0/Emulation/roms/ps2"), for the empty screen.
    std::string status() const;

  private:
    static void *run(void *self);
    void work();

    std::atomic<int> state_{0}; // 0 idle, 1 working, 2 done, 3 taken
    std::vector<Shelf> result_;
    std::vector<ArtJob> jobs_;
    std::vector<std::pair<std::string, std::string>> moves_;
    std::atomic<const char *> status_{"Looking for your games"};
};
} // namespace boxshelf::app
