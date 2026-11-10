// Boxshelf PS5 - runs the app's own art fetcher (src/app/art_fetcher.cpp) on the PC, end to end: real downloads,
// the same matching, shrinking and cache files, into $BOXSHELF_DOWNLOAD0 (default .local/download0).
// usage: art-fetch-host <library.json from list-library --json> [systems]   SPDX-License-Identifier: GPL-3.0-or-later
#include "app/art_fetcher.hpp"
#include "cJSON.h"
#include "data/art_match.hpp"

#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    std::ifstream f(argv[1]);
    std::stringstream s;
    s << f.rdbuf();
    std::set<std::string> only;
    if (argc > 2)
    {
        std::stringstream list(argv[2]);
        for (std::string k; std::getline(list, k, ',');)
            only.insert(k);
    }
    std::vector<boxshelf::app::ArtJob> jobs;
    std::set<std::string> seen;
    cJSON *root = cJSON_Parse(s.str().c_str());
    const cJSON *g = nullptr;
    auto str = [&](const char *k) {
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(g, k);
        return cJSON_IsString(v) ? std::string(v->valuestring) : std::string();
    };
    cJSON_ArrayForEach(g, root)
    {
        const std::string sys = str("system"), id = str("id");
        if ((!only.empty() && !only.count(sys)) || seen.count(id))
            continue;
        if (boxshelf::data::libretro_system(sys).empty() && boxshelf::data::gametdb_art(sys).empty() && sys != "gc" &&
            sys != "wii")
            continue;
        seen.insert(id);
        boxshelf::app::ArtJob job{id, sys, str("title"), str("region"), str("disc_id")};
        job.back = boxshelf::app::back_source(sys);
        jobs.push_back(std::move(job));
    }
    cJSON_Delete(root);
    std::printf("%zu jobs\n", jobs.size());
    boxshelf::app::ArtFetcher fetcher;
    fetcher.start(jobs);
    int got = 0;
    for (int idle = 0; idle < 50;)
    {
        usleep(200000);
        for (const auto &[id, path] : fetcher.take())
            std::printf("  got %s -> %s\n", id.c_str(), path.c_str()), ++got;
        idle = fetcher.running() ? 0 : idle + 1;
    }
    std::printf("downloaded %d covers\n", got);
    return 0;
}
