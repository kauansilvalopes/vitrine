// Boxshelf PS5 - the demo shelves (Menu → Demo shelves): well-known games on every platform, for screenshots and
// videos. Only titles live here; their box art is downloaded at run time from the sources Boxshelf uses (libretro,
// GameTDB), like any other game's, and kept in the app's own folder. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "data/scan.hpp"

#include <vector>

namespace boxshelf::app
{
// The demo games (ids "demo-<system>:<hash>", paths under /demo/). The PS5 shelf is not mocked: libretro and GameTDB
// have no PS5 boxes, so the demo keeps the console's own installed titles there.
std::vector<data::Game> demo_games();
// True for a demo game (nothing to start).
bool is_demo(const data::Game &game);
} // namespace boxshelf::app
