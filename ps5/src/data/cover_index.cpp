// Boxshelf PS5 - cover index. SPDX-License-Identifier: GPL-3.0-or-later
#include "data/cover_index.hpp"

#include "cJSON.h"

#include <cctype>

namespace boxshelf::data
{
namespace
{
std::string lower(std::string_view s)
{
    std::string out(s);
    for (char &c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Calls fn(fields) for every non-comment line of a tab-separated text.
template <typename Fn> void each_row(std::string_view text, Fn fn)
{
    while (!text.empty())
    {
        const auto end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (line.empty() || line[0] == '#')
            continue;
        std::vector<std::string_view> fields;
        while (true)
        {
            const auto tab = line.find('\t');
            fields.push_back(line.substr(0, tab));
            if (tab == std::string_view::npos)
                break;
            line = line.substr(tab + 1);
        }
        fn(fields);
    }
}

bool serial_like(std::string_view s)
{
    // SLUS-01538, SCES-12345, SLPM-86023 ...
    if (s.size() < 9 || s[4] != '-')
        return false;
    for (int i = 0; i < 4; ++i)
        if (!std::isalpha(static_cast<unsigned char>(s[i])))
            return false;
    for (std::size_t i = 5; i < s.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            return false;
    return true;
}
} // namespace

std::string leaf_key(std::string_view path)
{
    while (!path.empty() && path.back() == '/')
        path.remove_suffix(1);
    const auto slash = path.rfind('/');
    return lower(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

std::string parent_key(std::string_view path)
{
    const auto slash = path.rfind('/');
    if (slash == std::string_view::npos || slash == 0)
        return {};
    return leaf_key(path.substr(0, slash));
}

void CoverIndex::add_swan_serials(std::string_view tsv)
{
    each_row(tsv, [&](const std::vector<std::string_view> &f) {
        if (f.size() >= 4 && serial_like(f[3]))
        {
            swan_serial_[parent_key(f[0])] = std::string(f[3]);
            swan_serial_[leaf_key(f[0])] = std::string(f[3]); // a loose disc file
        }
    });
}

void CoverIndex::add_ps2_serials(std::string_view tsv)
{
    each_row(tsv, [&](const std::vector<std::string_view> &f) {
        if (f.size() >= 4 && serial_like(f[3]))
            ps2_serial_[leaf_key(f[0])] = std::string(f[3]);
    });
}

void CoverIndex::add_ps2_titles(std::string_view tsv)
{
    each_row(tsv, [&](const std::vector<std::string_view> &f) {
        if (f.size() >= 2 && serial_like(f[0]) && !f[1].empty())
            ps2_title_[std::string(f[0])] = std::string(f[1]);
    });
}

void CoverIndex::add_porpoise_cache(std::string_view tsv)
{
    each_row(tsv, [&](const std::vector<std::string_view> &f) {
        // v2: path, size, mtime, 0, disc id, 0, title, format, system, region
        if (f.size() >= 9 && f[4].size() == 6)
            porpoise_[leaf_key(f[0])] = {std::string(f[4]), std::string(f[6]), std::string(f[8]),
                                         f.size() >= 10 ? std::string(f[9]) : std::string()};
    });
}

void CoverIndex::add_eden_name(std::string_view hash, std::string_view title)
{
    while (!title.empty() && (title.back() == '\n' || title.back() == '\r' || title.back() == ' '))
        title.remove_suffix(1);
    if (!hash.empty() && !title.empty())
        eden_.emplace_back(std::string(hash), std::string(title));
}

void CoverIndex::add_ecam_library(std::string_view json)
{
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root)
        return;
    for (const char *key : {"3ds", "wiiu", "nds"})
    {
        const cJSON *e = nullptr;
        cJSON_ArrayForEach(e, cJSON_GetObjectItemCaseSensitive(root, key))
        {
            const cJSON *path = cJSON_GetObjectItemCaseSensitive(e, "path");
            if (!cJSON_IsString(path) || !path->valuestring)
                continue;
            auto str = [&](const char *k) {
                const cJSON *v = cJSON_GetObjectItemCaseSensitive(e, k);
                return cJSON_IsString(v) && v->valuestring ? std::string(v->valuestring) : std::string();
            };
            ecam_[leaf_key(path->valuestring)] = {str("titleId"), str("gameId"), str("name")};
        }
    }
    cJSON_Delete(root);
}

bool CoverIndex::resolve(Game &g) const
{
    const std::string leaf = leaf_key(g.path);
    switch (g.system)
    {
    case System::psx: {
        const auto it = swan_serial_.find(leaf);
        if (it == swan_serial_.end())
            break;
        g.disc_id = it->second;
        g.cover_front = "/data/SwanStationPS5/covers/default/" + it->second + ".jpg";
        break;
    }
    case System::ps2: {
        const auto it = ps2_serial_.find(leaf);
        if (it == ps2_serial_.end())
            break;
        g.disc_id = it->second;
        const std::string base = "/data/PCSX2/covers/" + it->second;
        g.cover_front = base + ".jpg";
        g.cover_spine = base + "_spine.jpg";
        g.cover_back = base + "_back.jpg";
        // The GameDB title only replaces a file name that says little ("2002fifaworldcup"): the owner's own
        // names ("Bee Movie Game") read better than the database's capitals.
        const auto t = ps2_title_.find(it->second);
        if (t != ps2_title_.end() && (g.title.find(' ') == std::string::npos || meaningless(g.title)))
            g.title = clean_title(t->second, false);
        break;
    }
    case System::gc:
    case System::wii: {
        const auto it = porpoise_.find(leaf);
        if (it == porpoise_.end())
            break;
        const Disc &d = it->second;
        g.disc_id = d.id;
        g.region = d.region;
        if (d.system == "GameCube")
            g.system = System::gc;
        else if (d.system == "Wii")
            g.system = System::wii;
        else if (const System s = gc_or_wii_from_disc_id(d.id); s != System::count)
            g.system = s;
        const std::string base = "/data/porpoise/covers/" + d.id;
        g.cover_front = base + ".png";
        g.cover_back = base + ".back.png";
        g.cover_spine = base + ".spine.png";
        if (meaningless(g.title) && !d.title.empty())
            g.title = d.title;
        break;
    }
    case System::sw:
        for (const auto &[hash, title] : eden_)
            if (same_title(title, g.title))
            {
                g.cover_front = "/data/prosperoeden/covers/" + hash + ".tga";
                g.cover_square = true; // ProsperoEden keeps 256x256 icons
                g.title = title;       // its real title ("Bowser's Fury"), the folder has a file-system safe one
                break;
            }
        break;
    case System::n3ds:
    case System::wiiu:
    case System::nds: {
        const auto it = ecam_.find(leaf);
        if (it == ecam_.end())
            break;
        const Ecam &e = it->second;
        // the box art PS5-ECAM downloaded, else the game's own icon (not every game has box art there)
        const std::string icon = e.title_id.empty() ? std::string() : "/data/ps5cemu/covers/3ds/" + e.title_id + ".tga";
        if (!e.game_id.empty() && g.system == System::n3ds)
        {
            g.cover_front = "/data/ps5cemu/covers/boxart/3ds/" + e.game_id + ".tga";
            g.cover_fallback = icon;
        }
        else if (!icon.empty() && g.system == System::n3ds)
        {
            g.cover_front = icon;
            g.cover_square = true;
        }
        g.disc_id = e.title_id;
        break;
    }
    case System::ps5:
    case System::ps4:
    case System::custom:
    case System::media:
        if (!g.app.empty())
        {
            g.cover_front = "/user/appmeta/" + g.app + "/icon0.png";
            g.cover_square = true;
        }
        break;
    default:
        break;
    }
    return !g.cover_front.empty();
}
} // namespace boxshelf::data
