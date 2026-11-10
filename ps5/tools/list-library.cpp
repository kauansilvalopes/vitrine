// Boxshelf PS5 - host tool: runs the app's data layer (src/data) over listings saved from the console.
// SPDX-License-Identifier: GPL-3.0-or-later
//
//   list-library --roots <dir>   print every root to list (defaults + those found in the saved emulator configs)
//   list-library <dir>           print the shelves built from <dir>/appdb.json and <dir>/fs/<escaped root>.json
//
// <dir> is filled by tools/fetch-listings.py (read-only HTTP GETs to Elf Arsenal).
#include "data/cover_index.hpp"
#include "data/scan.hpp"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace boxshelf::data;

namespace
{
std::string read(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

// Same escaping as fetch-listings.py: '/' -> '_', everything not [A-Za-z0-9._-] -> '%XX'.
std::string escape(const std::string &path)
{
    std::string out;
    char hex[4];
    for (unsigned char c : path)
    {
        if (c == '/')
            out += '_';
        else if (std::isalnum(c) || c == '.' || c == '-')
            out += static_cast<char>(c);
        else
        {
            std::snprintf(hex, sizeof hex, "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

std::vector<Root> all_roots(const std::string &dir)
{
    auto roots = default_roots();
    for (auto &r : roots_from_ps5cemu_json(read(dir + "/config/ps5cemu.json")))
        roots.push_back(r);
    for (auto &r : roots_from_ps5sx2_ini(read(dir + "/config/gs.ini")))
        roots.push_back(r);
    for (auto &r : roots_from_swanstation_ini(read(dir + "/config/SwanStationPS5.ini")))
        roots.push_back(r);
    return roots;
}
} // namespace

int main(int argc, char **argv)
{
    if (argc == 3 && std::string(argv[1]) == "--roots")
    {
        for (const auto &r : all_roots(argv[2]))
            std::printf("%s\n", r.path.c_str());
        return 0;
    }
    const bool covers_only = argc == 3 && std::string(argv[1]) == "--covers";
    const bool json_only = argc == 3 && std::string(argv[1]) == "--json";
    if (argc != 2 && !covers_only && !json_only)
    {
        std::fprintf(stderr, "usage: %s [--roots|--covers|--json] <listings dir>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[(covers_only || json_only) ? 2 : 1];
    std::vector<Game> games = games_from_appdb(read(dir + "/appdb.json"));
    int listed = 0;
    for (const auto &root : all_roots(dir))
    {
        std::vector<DirEntry> entries;
        if (!parse_fs_list(read(dir + "/fs/" + escape(root.path) + ".json"), entries))
            continue;
        ++listed;
        for (auto &g : scan_listing(root, entries))
            games.push_back(std::move(g));
    }
    if (!covers_only && !json_only)
        std::printf("roots listed: %d\n", listed);

    // Cover index (the same files the console app reads) and the cover folders' listings, to tell which exist.
    CoverIndex index;
    index.add_swan_serials(read(dir + "/config/swan-serials.tsv"));
    index.add_ps2_serials(read(dir + "/config/ps2-serials.txt"));
    index.add_ps2_titles(read(dir + "/config/ps2-titles.tsv"));
    index.add_porpoise_cache(read(dir + "/config/porpoise-disc-cache.tsv"));
    index.add_ecam_library(read(dir + "/config/ecam-library.json"));
    {
        std::vector<DirEntry> eden;
        parse_fs_list(read(dir + "/fs/" + escape("/data/prosperoeden/covers") + ".json"), eden);
        for (const auto &e : eden)
            if (e.name.size() > 5 && e.name.substr(e.name.size() - 5) == ".name")
                index.add_eden_name(e.name.substr(0, e.name.size() - 5), read(dir + "/eden/" + e.name));
    }
    auto exists = [&](const std::string &path)
    {
        const auto slash = path.rfind('/');
        std::vector<DirEntry> entries;
        if (slash == std::string::npos ||
            !parse_fs_list(read(dir + "/fs/" + escape(path.substr(0, slash)) + ".json"), entries))
            return false;
        const std::string leaf = path.substr(slash + 1);
        for (const auto &e : entries)
            if (e.name == leaf)
                return true;
        return false;
    };
    int total = 0, with_cover = 0;
    for (auto &g : games)
        index.resolve(g);
    if (json_only)
    {
        // the library for tools/boxshelf_covers.py: one object per game
        auto esc = [](const std::string &s)
        {
            std::string o;
            for (char c : s)
            {
                if (c == '"' || c == '\\')
                    o += '\\';
                if (static_cast<unsigned char>(c) < 0x20)
                    continue;
                o += c;
            }
            return o;
        };
        std::printf("[\n");
        for (std::size_t i = 0; i < games.size(); ++i)
        {
            const auto &g = games[i];
            std::printf("{\"id\":\"%s\",\"system\":\"%s\",\"title\":\"%s\",\"path\":\"%s\",\"disc_id\":\"%s\",\"region\":\"%s\","
                        "\"app\":\"%s\",\"has_cover\":%s}%s\n",
                        esc(g.id).c_str(), std::string(style(g.system).key).c_str(), esc(g.title).c_str(),
                        esc(g.path).c_str(), esc(g.disc_id).c_str(), esc(g.region).c_str(), esc(g.app).c_str(),
                        (!g.cover_front.empty() && exists(g.cover_front)) ? "true" : "false",
                        i + 1 < games.size() ? "," : "");
        }
        std::printf("]\n");
        return 0;
    }
    if (covers_only)
    {
        for (const auto &g : games)
            for (const auto *p : {&g.cover_front, &g.cover_back, &g.cover_spine, &g.cover_fallback})
                if (!p->empty() && exists(*p))
                    std::printf("%s\t%s\n", std::string(style(g.system).key).c_str(), p->c_str());
        return 0;
    }
    for (const auto &shelf : build_shelves(games))
    {
        const auto &s = style(shelf.system);
        int found = 0;
        for (const auto &g : shelf.games)
            found += (!g.cover_front.empty() && exists(g.cover_front)) || (!g.cover_fallback.empty() && exists(g.cover_fallback));
        total += static_cast<int>(shelf.games.size());
        with_cover += found;
        std::printf("\n== %s (%s) - %zu  covers %d/%zu  case %.3f x depth %.3f, colour #%06x, spine \"%.*s\"\n",
                    std::string(s.word).c_str(), std::string(s.key).c_str(), shelf.games.size(), found,
                    shelf.games.size(), s.ratio, s.depth, s.case_color, static_cast<int>(s.spine_band.label.size()),
                    s.spine_band.label.data());
        for (const auto &g : shelf.games)
        {
            const bool ok = (!g.cover_front.empty() && exists(g.cover_front)) ||
                            (!g.cover_fallback.empty() && exists(g.cover_fallback));
            std::printf("   %s %-48s %s%s%s\n", ok ? "[art]" : "[ - ]", g.title.c_str(), g.path.c_str(),
                        g.folder ? "/" : "", g.cover_square ? "  (square icon)" : "");
        }
    }
    std::printf("\ncovers found: %d of %d games\n", with_cover, total);
    return 0;
}
