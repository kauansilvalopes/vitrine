// Boxshelf PS5 - host tool: runs the app's data layer (src/data) over listings saved from the console.
// SPDX-License-Identifier: GPL-3.0-or-later
//
//   list-library --roots <dir>   print every root to list (defaults + those found in the saved emulator configs)
//   list-library <dir>           print the shelves built from <dir>/appdb.json and <dir>/fs/<escaped root>.json
//
// <dir> is filled by tools/fetch-listings.py (read-only HTTP GETs to Elf Arsenal).
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
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: %s [--roots] <listings dir>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
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
    std::printf("roots listed: %d\n", listed);
    for (const auto &shelf : build_shelves(games))
    {
        const auto &s = style(shelf.system);
        std::printf("\n== %s (%s) - %zu  case %.3f x depth %.3f, colour #%06x, spine \"%.*s\"\n",
                    std::string(s.word).c_str(), std::string(s.key).c_str(), shelf.games.size(), s.ratio, s.depth,
                    s.case_color, static_cast<int>(s.spine_band.label.size()), s.spine_band.label.data());
        for (const auto &g : shelf.games)
            std::printf("   %-48s %s%s\n", g.title.c_str(), g.path.c_str(), g.folder ? "/" : "");
    }
    return 0;
}
