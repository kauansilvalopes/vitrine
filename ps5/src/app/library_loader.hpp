// Boxshelf PS5 - builds the library in the background: /appdb, the emulators' configs, every root's listing.
// Read-only: only GET requests. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app/shelf.hpp"

#include <atomic>
#include <string>
#include <vector>

namespace boxshelf::app
{
class LibraryLoader
{
  public:
    // Starts the worker thread (once).
    void start();
    // True once, when the shelves are ready to take.
    bool take(std::vector<Shelf> &shelves);
    // What the worker is doing ("Reading /mnt/usb0/Emulation/roms/ps2"), for the empty screen.
    std::string status() const;

  private:
    static void *run(void *self);
    void work();

    std::atomic<int> state_{0}; // 0 idle, 1 working, 2 done, 3 taken
    std::vector<Shelf> result_;
    std::atomic<const char *> status_{"Looking for your games"};
};
} // namespace boxshelf::app
