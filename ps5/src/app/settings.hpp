// Boxshelf PS5 - the owner's choices, kept in the app's own folder (/download0/boxshelf/settings.json).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace boxshelf::app
{
struct Settings
{
    std::string language; // "en", "pt", "es"; empty: the console's language
    bool backs = true;    // download the backs of the boxes too (Menu → Box art backs)
    std::string sort = "az";    // app.js ui.sort: "az", "recent" (last played), "most" (most played)
    std::string filter = "all"; // app.js ui.filter: "all", "fav", "played", "unplayed"
    // Games put on another shelf from the menu: game id -> Boxshelf system key ("sw", "n64"...)
    std::vector<std::pair<std::string, std::string>> moves;
    // Favourites (hold Triangle; app.js ui.favorites) and play history (app.js HIST: last start, how many starts)
    std::vector<std::string> favorites;
    struct Played
    {
        std::string id;
        long long last = 0; // unix seconds
        int count = 0;
    };
    std::vector<Played> history;

    bool is_favorite(const std::string &game_id) const;
    bool toggle_favorite(const std::string &game_id); // true when it is a favourite now
    void played(const std::string &game_id, long long when);
    const Played *history_of(const std::string &game_id) const;

    const std::string *moved_to(const std::string &game_id) const;
    void set_move(const std::string &game_id, const std::string &system_key); // empty key: back to its own shelf

    std::string to_json() const;
    static Settings from_json(const std::string &json);
};

std::string settings_path();
Settings load_settings();
bool save_settings(const Settings &settings);
} // namespace boxshelf::app
