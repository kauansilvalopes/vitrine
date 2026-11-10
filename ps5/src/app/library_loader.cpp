// Boxshelf PS5 - library loader. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/library_loader.hpp"

#include "app/elf_client.hpp"
#include "data/scan.hpp"
#include "platform/ps5/system.hpp"

#include <pthread.h>

#include <algorithm>

namespace boxshelf::app
{
void LibraryLoader::start()
{
    int expected = 0;
    if (!state_.compare_exchange_strong(expected, 1))
        return;
    pthread_t thread;
    if (pthread_create(&thread, nullptr, &LibraryLoader::run, this) != 0)
    {
        work(); // no thread: do it now
        return;
    }
    pthread_detach(thread);
}

void *LibraryLoader::run(void *self)
{
    static_cast<LibraryLoader *>(self)->work();
    return nullptr;
}

void LibraryLoader::work()
{
    using namespace data;
    std::vector<Game> games;
    std::string body;
    status_ = "Reading the installed games";
    std::vector<std::string> installed;
    if (elf_get("/appdb", body))
    {
        for (auto &g : games_from_appdb(body))
        {
            installed.push_back(g.app);
            games.push_back(std::move(g));
        }
    }
    hui::sys::log("[BOXSHELF] library: appdb %zu titles", installed.size());
    status_ = "Reading the emulators' settings";
    auto roots = default_roots();
    auto add = [&](std::vector<Root> more) { roots.insert(roots.end(), more.begin(), more.end()); };
    if (elf_get(fs_url("/api/fs/download?path=", "/data/ps5cemu/ps5cemu.json"), body))
        add(roots_from_ps5cemu_json(body));
    if (elf_get(fs_url("/api/fs/download?path=", "/data/PCSX2/gs.ini"), body))
        add(roots_from_ps5sx2_ini(body));
    if (elf_get(fs_url("/api/fs/download?path=", "/data/SwanStationPS5/SwanStationPS5.ini"), body))
        add(roots_from_swanstation_ini(body));
    status_ = "Looking through your game folders";
    std::vector<std::string> seen;
    int listed = 0;
    for (const auto &root : roots)
    {
        if (std::find(seen.begin(), seen.end(), root.path) != seen.end())
            continue;
        seen.push_back(root.path);
        std::vector<DirEntry> entries;
        if (!elf_get(fs_url("/api/fs/list?path=", root.path), body) || !parse_fs_list(body, entries))
            continue;
        ++listed;
        for (auto &g : scan_listing(root, entries))
            games.push_back(std::move(g));
    }
    hui::sys::log("[BOXSHELF] library: %d roots listed, %zu games", listed, games.size());
    std::vector<Shelf> shelves;
    for (const auto &s : build_shelves(games))
    {
        const auto app = emulator_app(s.system);
        const bool has_emu = !app.empty() && std::find(installed.begin(), installed.end(), app) != installed.end();
        shelves.push_back(prepare_shelf(s, has_emu));
    }
    // Home stays out until it has recent games and favourites to show (it is built from play history, P7).
    shelves.erase(std::remove_if(shelves.begin(), shelves.end(),
                                 [](const Shelf &s) { return s.system == System::home && s.items.empty(); }),
                  shelves.end());
    result_ = std::move(shelves);
    state_ = 2;
}

bool LibraryLoader::take(std::vector<Shelf> &shelves)
{
    int expected = 2;
    if (!state_.compare_exchange_strong(expected, 3))
        return false;
    shelves = std::move(result_);
    return true;
}

std::string LibraryLoader::status() const
{
    return status_.load();
}
} // namespace boxshelf::app
