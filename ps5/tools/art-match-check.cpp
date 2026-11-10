// Boxshelf PS5 - prints the libretro match of every game (stdin: "system\ttitle\tregion" lines) with the C++ port,
// for comparison with tools/boxshelf_covers.py. usage: art-match-check <cache dir>  < games.tsv
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cJSON.h"
#include "data/art_match.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    std::map<std::string, std::vector<std::string>> lists;
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::istringstream row(line);
        std::string sys, title, region;
        std::getline(row, sys, '\t');
        std::getline(row, title, '\t');
        std::getline(row, region, '\t');
        if (!lists.count(sys))
        {
            std::ifstream f(std::string(argv[1]) + "/libretro-" + sys + ".json");
            std::stringstream s;
            s << f.rdbuf();
            std::vector<std::string> names;
            if (cJSON *root = cJSON_Parse(s.str().c_str()))
            {
                const cJSON *e = nullptr;
                cJSON_ArrayForEach(e, root) if (cJSON_IsString(e)) names.emplace_back(e->valuestring);
                cJSON_Delete(root);
            }
            lists[sys] = std::move(names);
        }
        std::printf("%s\t%s\t%s\n", sys.c_str(), title.c_str(),
                    boxshelf::data::libretro_match(lists[sys], title, region).c_str());
    }
    return 0;
}
