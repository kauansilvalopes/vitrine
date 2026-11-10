// Boxshelf PS5 - shelf model. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/shelf.hpp"

#include "app/css.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace boxshelf::app
{
using data::System;

const char *emulator_name(System system)
{
    switch (system)
    {
    case System::ps2:
        return "PS5SX2";
    case System::psx:
        return "SwanStation";
    case System::gc:
    case System::wii:
        return "Porpoise";
    case System::wiiu:
    case System::n3ds:
    case System::nds:
        return "PS5-ECAM";
    case System::sw:
        return "ProsperoEden";
    default:
        return nullptr;
    }
}

namespace
{
Item tile(ItemKind kind, std::string title, std::string_view seed_text, int hue, System system)
{
    Item it;
    it.kind = kind;
    it.title = std::move(title);
    it.seed = title_hash(seed_text);
    it.hue = hue;
    it.system = system;
    return it;
}
} // namespace

Shelf prepare_shelf(const data::Shelf &shelf, bool emulator_installed)
{
    Shelf out;
    out.system = shelf.system;
    out.game_count = static_cast<int>(shelf.games.size());
    const bool console = !data::emulator_app(shelf.system).empty();
    const std::string key(data::style(shelf.system).key);
    // items.unshift order in app.js: emulator, then setup, then random -> [random?, setup?, emulator?, games...]
    if (console && out.game_count > 1)
        out.items.push_back(tile(ItemKind::random, "Surprise me", "random" + key, 300, shelf.system));
    if (console && out.game_count == 0)
        out.items.push_back(tile(ItemKind::setup, "Add games", "setup" + key, 210, shelf.system));
    if (console && emulator_installed)
    {
        const std::string t = std::string("Open ") + emulator_name(shelf.system);
        Item it = tile(ItemKind::emulator, t, t + key, 0, shelf.system);
        it.hue = static_cast<int>(it.seed % 360);
        out.items.push_back(std::move(it));
    }
    for (const auto &g : shelf.games)
    {
        Item it;
        it.kind = ItemKind::game;
        it.title = g.title;
        it.seed = title_hash(g.title);
        it.hue = static_cast<int>(it.seed % 360);
        it.system = g.system;
        it.game = g;
        out.items.push_back(std::move(it));
    }
    return out;
}

void ShelfModel::set(std::vector<Shelf> shelves, int shelf)
{
    shelves_ = std::move(shelves);
    current_ = shelves_.empty() ? 0 : std::max(0, std::min(shelf, shelf_count() - 1));
}

const Item *ShelfModel::item() const
{
    if (shelves_.empty() || cur().items.empty())
        return nullptr;
    return &cur().items[static_cast<std::size_t>(selected())];
}

bool ShelfModel::go(int index)
{
    if (shelves_.empty())
        return false;
    auto &s = shelves_[static_cast<std::size_t>(current_)];
    const int n = static_cast<int>(s.items.size());
    if (n == 0)
        return false;
    index = std::max(0, std::min(n - 1, index));
    if (index == s.selected)
        return false;
    s.selected = index;
    return true;
}

std::string section_of(const Item &item)
{
    if (item.kind != ItemKind::game)
        return "\xe2\x80\xa2"; // •
    const unsigned char c = item.title.empty() ? 0 : static_cast<unsigned char>(item.title[0]);
    if (std::isalpha(c))
        return std::string(1, static_cast<char>(std::toupper(c)));
    return "#";
}

bool ShelfModel::letter(int dir, std::string *label)
{
    if (shelves_.empty())
        return false;
    const auto &items = cur().items;
    std::vector<int> starts;
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
        if (i == 0 || section_of(items[i]) != section_of(items[i - 1]))
            starts.push_back(i);
    if (starts.size() < 2)
        return false;
    const int sel = selected(), n = static_cast<int>(starts.size());
    int k = 0;
    for (int j = 0; j < n; ++j)
        if (starts[j] <= sel)
            k = j;
    const int target = dir > 0 ? starts[(k + 1) % n] : starts[sel > starts[k] ? k : (k - 1 + n) % n];
    const bool moved = go(target);
    if (label)
        *label = section_of(items[static_cast<std::size_t>(selected())]);
    return moved;
}

bool ShelfModel::switch_shelf(int dir)
{
    const int n = shelf_count();
    if (n < 2)
        return false;
    current_ = ((current_ + dir) % n + n) % n;
    return true;
}

float ShelfModel::position() const
{
    if (shelves_.empty())
        return 0.0f;
    const int n = static_cast<int>(cur().items.size());
    return static_cast<float>(selected()) / static_cast<float>(std::max(1, n - 1));
}

Pose place(int index, int selected, float bw, float bd)
{
    const int d = index - selected, ad = std::abs(d), sg = (d > 0) - (d < 0);
    // Selected game forward with clear space on each side; the rest recede in a tight angled stack.
    if (d == 0)
        return {0.0f, bw * 1.05f, 22.0f};
    return {static_cast<float>(sg) * (bw * 1.22f + static_cast<float>(ad - 1) * (bw * 0.22f + bd * 0.9f)),
            -bw * 0.7f - static_cast<float>(ad) * 6.0f, -static_cast<float>(sg) * 60.0f};
}
} // namespace boxshelf::app
