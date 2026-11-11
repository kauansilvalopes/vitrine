// Boxshelf PS5 - library loader. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/library_loader.hpp"

#include "app/elf_client.hpp"
#include "data/art_match.hpp"
#include "data/cover_index.hpp"

#include <sys/stat.h>
#include "data/scan.hpp"
#include "platform/ps5/system.hpp"

#include <pthread.h>

#include <algorithm>

namespace boxshelf::app
{
void LibraryLoader::start(std::vector<std::pair<std::string, std::string>> moves)
{
    moves_ = std::move(moves);
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

    // Box art the emulators already keep on the console (data/cover_index.hpp).
    status_ = "Matching box art";
    CoverIndex index;
    auto download = [&](const char *path) { return elf_get(fs_url("/api/fs/download?path=", path), body) ? body : std::string(); };
    index.add_swan_serials(download("/data/SwanStationPS5/cache/serials.tsv"));
    index.add_ps2_serials(download("/data/PCSX2/chd-serials.txt"));
    index.add_ps2_titles(download("/data/homebrew/PPSA99841/assets/ps2-titles.tsv"));
    index.add_porpoise_cache(download("/data/porpoise/disc-cache.tsv"));
    index.add_ecam_library(download("/data/ps5cemu/library.json"));
    {
        std::vector<DirEntry> eden;
        if (elf_get(fs_url("/api/fs/list?path=", "/data/prosperoeden/covers"), body) && parse_fs_list(body, eden))
            for (const auto &e : eden)
                if (e.name.size() > 5 && e.name.compare(e.name.size() - 5, 5, ".name") == 0)
                {
                    const std::string hash = e.name.substr(0, e.name.size() - 5);
                    index.add_eden_name(hash, download(("/data/prosperoeden/covers/" + e.name).c_str()));
                }
    }
    {
        // Boxshelf's own covers, put there by tools/boxshelf_covers.py + upload-covers.py
        std::vector<DirEntry> own;
        if (elf_get(fs_url("/api/fs/list?path=", "/data/boxshelf/covers"), body) && parse_fs_list(body, own))
        {
            std::vector<std::string> names;
            for (const auto &e : own)
                if (!e.dir)
                    names.push_back(e.name);
            index.add_boxshelf_covers(names);
        }
    }
    // Folder names often lost their apostrophes ("Tony Hawks Pro Skater", "Disneys Tarzan"): when the libretro index
    // the art fetcher keeps has the same game under the same words, its proper name is shown instead.
    {
        std::vector<std::pair<std::string, std::vector<std::string>>> indexes;
        int renamed = 0;
        for (auto &g : games)
        {
            const std::string key(style(g.system).key);
            if (data::libretro_system(key).empty())
                continue;
            auto found = std::find_if(indexes.begin(), indexes.end(), [&](const auto &p) { return p.first == key; });
            if (found == indexes.end())
            {
                std::vector<std::string> names;
                if (FILE *f = std::fopen((storage_root() + "/libretro-" + key + ".txt").c_str(), "rb"))
                {
                    char line[512];
                    while (std::fgets(line, sizeof line, f))
                    {
                        std::string n(line);
                        while (!n.empty() && (n.back() == '\n' || n.back() == '\r'))
                            n.pop_back();
                        if (!n.empty())
                            names.push_back(std::move(n));
                    }
                    std::fclose(f);
                }
                indexes.emplace_back(key, std::move(names));
                found = indexes.end() - 1;
            }
            if (found->second.empty())
                continue;
            const std::string hit = data::libretro_match(found->second, g.title, g.region);
            if (hit.empty())
                continue;
            // only the apostrophes are taken: libretro's file names also turn ':' into ' - ' and '&' into '_'
            const std::string proper = data::libretro_title(hit);
            std::string bare;
            for (char c : proper)
                if (c != '\'')
                    bare += c;
            if (proper != g.title && bare == g.title)
                g.title = proper, ++renamed;
        }
        hui::sys::log("[BOXSHELF] library: %d titles given their proper names", renamed);
    }
    // PS5SX2 keeps <serial>.jpg, _back.jpg and _spine.jpg, but a back it could not find is "<serial>_back.missing":
    // only the files that are there are used (a back that isn't comes from LaunchBox instead)
    std::vector<std::string> pcsx2;
    bool pcsx2_listed = false;
    {
        std::vector<DirEntry> list;
        if (elf_get(fs_url("/api/fs/list?path=", "/data/PCSX2/covers"), body, 8u << 20) && parse_fs_list(body, list))
            pcsx2_listed = true;
        for (const auto &e : list)
                if (!e.dir)
                    pcsx2.push_back("/data/PCSX2/covers/" + e.name);
        std::sort(pcsx2.begin(), pcsx2.end());
    }
    auto pcsx2_has = [&](const std::string &path)
    { return !pcsx2_listed || path.rfind("/data/PCSX2/covers/", 0) != 0 || std::binary_search(pcsx2.begin(), pcsx2.end(), path); };
    int covered = 0;
    for (auto &g : games)
    {
        bool has_art = index.resolve(g);
        if (!pcsx2_has(g.cover_back))
            g.cover_back.clear();
        if (!pcsx2_has(g.cover_spine))
            g.cover_spine.clear();
        if (!pcsx2_has(g.cover_front))
            g.cover_front.clear(), has_art = false;
        // the owner's shelf for it (menu: Move to shelf), after its art was found where it came from
        for (const auto &[id, key] : moves_)
            if (id == g.id)
                if (const System to = system_from_key(key); to != System::count && to != System::home)
                    g.system = to;
        const bool own = g.cover_front.rfind("/data/boxshelf/covers/", 0) == 0;
        // box art the app downloaded earlier (art_fetcher), when the PC tool sent none
        struct stat st
        {
        };
        const std::string local = downloaded_cover_path(g.id), back = downloaded_back_path(g.id);
        if (g.cover_back.empty() && stat(back.c_str(), &st) == 0 && st.st_size > 0)
            g.cover_back = back;
        if (!own && stat(local.c_str(), &st) == 0 && st.st_size > 0)
        {
            if (!g.cover_front.empty())
                g.cover_fallback = g.cover_front, g.fallback_square = g.cover_square;
            g.cover_front = local;
            g.cover_square = false;
            has_art = true;
        }
        covered += has_art;
    }
    // what LaunchBox said about each game (fetched earlier), then what is still missing
    apply_info(games);
    jobs_ = art_jobs(games, false);
    hui::sys::log("[BOXSHELF] library: box art matched for %d of %zu games", covered, games.size());
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

bool LibraryLoader::take(std::vector<Shelf> &shelves, std::vector<ArtJob> &jobs)
{
    int expected = 2;
    if (!state_.compare_exchange_strong(expected, 3))
        return false;
    shelves = std::move(result_);
    jobs = std::move(jobs_);
    return true;
}

std::string LibraryLoader::status() const
{
    return status_.load();
}
} // namespace boxshelf::app
