// Boxshelf PS5 - what the shelf screen shows and where each case sits. No drawing here (host-testable).
// Port of renderer/app.js prepare(), place(), go(), letter(), setSystem(). SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "data/scan.hpp"
#include "data/systems.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace boxshelf::app
{
enum class ItemKind : std::uint8_t
{
    game,
    random,   // "Surprise me" (shelves with 2+ games)
    emulator, // "Open <emulator>" (console shelves whose emulator is installed)
    setup,    // "Add games" (console shelf with no games)
};

struct Item
{
    ItemKind kind = ItemKind::game;
    std::string title;
    int hue = 0;     // covers.js: hash(title) % 360 (seed % 360)
    std::uint32_t seed = 0;
    data::System system = data::System::count; // the case style it is drawn with
    data::Game game; // kind == game
};

struct Shelf
{
    data::System system;
    std::vector<Item> items;
    int game_count = 0;
    int selected = 0; // remembered per shelf
};

// The emulator app a console shelf opens, as Boxshelf words it ("Open Dolphin").
const char *emulator_name(data::System system);

// app.js prepare(): the shelf's items ("Add games" on an empty console shelf, then the games; the owner dropped
// Surprise me and the Open <emulator> tiles).
Shelf prepare_shelf(const data::Shelf &shelf, bool emulator_installed);

class ShelfModel
{
  public:
    void set(std::vector<Shelf> shelves, int shelf = 0);
    bool empty() const
    {
        return shelves_.empty();
    }
    int shelf_count() const
    {
        return static_cast<int>(shelves_.size());
    }
    int current() const
    {
        return current_;
    }
    const Shelf &shelf(int i) const
    {
        return shelves_[static_cast<std::size_t>(i)];
    }
    const Shelf &cur() const
    {
        return shelves_[static_cast<std::size_t>(current_)];
    }
    int selected() const
    {
        return cur().selected;
    }
    const Item *item() const;

    // go(i): false when nothing changed.
    bool go(int index);
    bool step(int dir)
    {
        return go(selected() + dir);
    }
    // letter(dir): to the next / previous letter section (wraps); writes the section label.
    bool letter(int dir, std::string *label = nullptr);
    // setSystem(current + dir), wrapping.
    bool switch_shelf(int dir);
    // 0 at the first case, 1 at the last (app.js shelfPos()).
    float position() const;
    // A game's new box art (the art fetcher's download): its picture becomes the fallback. False if not found.
    bool set_cover(const std::string &game_id, const std::string &path);
    // The game item with this id on any shelf, or null.
    const Item *find(const std::string &game_id) const;
    // Puts a game on another shelf (made when missing, in the shelves' order; a shelf left empty goes). The view
    // stays on the shelf it was on. False when the game is not found or is already there.
    bool move_item(const std::string &game_id, data::System to);

  private:
    std::vector<Shelf> shelves_;
    int current_ = 0;
};

// Section label of an item for letter(): the first letter, "#" for digits/symbols, "•" for tiles.
std::string section_of(const Item &item);

// app.js place(): where case i sits when `selected` is selected, in CSS pixels relative to the track
// (left 50 %, top 42 %). bw / bd: case width and depth.
struct Pose
{
    float x, z, ry_deg;
};
Pose place(int index, int selected, float bw, float bd);
// The cases app.js keeps built: |i - sel| <= WIN = 13 + 2 (effects on); those past WIN - 2 fade out (.far).
constexpr int kWindow = 15;
} // namespace boxshelf::app
