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
    // Games put on another shelf from the menu: game id -> Boxshelf system key ("sw", "n64"...)
    std::vector<std::pair<std::string, std::string>> moves;

    const std::string *moved_to(const std::string &game_id) const;
    void set_move(const std::string &game_id, const std::string &system_key); // empty key: back to its own shelf

    std::string to_json() const;
    static Settings from_json(const std::string &json);
};

std::string settings_path();
Settings load_settings();
bool save_settings(const Settings &settings);
} // namespace boxshelf::app
