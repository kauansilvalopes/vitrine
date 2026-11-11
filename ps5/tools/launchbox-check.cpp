// Boxshelf PS5 - reads a local Metadata.zip through the app's streaming LaunchBox reader (src/app/launchbox.cpp), fed
// in 64 KB pieces as a download would, and matches the library's titles. usage: launchbox-check <zip> < games.tsv
// (lines "system key \t title"). SPDX-License-Identifier: GPL-3.0-or-later
#include "app/launchbox.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    std::vector<std::pair<std::string, std::string>> games;
    std::set<std::string> plats;
    for (std::string line; std::getline(std::cin, line);)
    {
        const auto t = line.find('\t');
        if (t == std::string::npos)
            continue;
        const std::string key = line.substr(0, t);
        if (boxshelf::data::launchbox_platform(key).empty())
            continue;
        games.emplace_back(key, line.substr(t + 1));
        plats.insert(std::string(boxshelf::data::launchbox_platform(key)));
    }
    boxshelf::data::LaunchBoxReader reader({plats.begin(), plats.end()});
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<char> buf(65536);
    std::size_t fed = 0;
    const auto t0 = std::chrono::steady_clock::now();
    while (!reader.done() && f.read(buf.data(), buf.size()), f.gcount() > 0)
    {
        if (!reader.feed(buf.data(), static_cast<std::size_t>(f.gcount())))
        {
            std::printf("broken at %zu bytes\n", fed);
            return 1;
        }
        fed += static_cast<std::size_t>(f.gcount());
        if (reader.done())
            break;
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto all = reader.take();
    std::printf("read %zu of the zip's bytes in %.1f s, done=%d\n", fed, secs, reader.done() ? 1 : 0);
    std::size_t bytes = 0;
    for (const auto &[p, list] : all)
    {
        int fronts = 0, backs = 0;
        for (const auto &g : list)
            fronts += !g.front.empty(), backs += !g.back.empty();
        const std::string tsv = boxshelf::data::lb_to_tsv(list);
        bytes += tsv.size();
        std::printf("%-22s %6zu games, %6d fronts, %6d backs, cache %zu KB, round trip %s\n", p.c_str(), list.size(), fronts,
                    backs, tsv.size() / 1024, boxshelf::data::lb_from_tsv(tsv).size() == list.size() ? "ok" : "FAILED");
    }
    int found = 0, front = 0, back = 0, info = 0;
    for (const auto &[key, title] : games)
    {
        const auto &list = all[std::string(boxshelf::data::launchbox_platform(key))];
        const auto *g = boxshelf::data::launchbox_match(list, title);
        if (!g)
        {
            std::printf("  no match: %s  %s\n", key.c_str(), title.c_str());
            continue;
        }
        ++found, front += !g->front.empty(), back += !g->back.empty(), info += !g->overview.empty();
    }
    std::printf("library: %d of %zu matched (%d fronts, %d backs, %d with an overview); caches %zu KB\n", found, games.size(),
                front, back, info, bytes / 1024);
    return 0;
}
