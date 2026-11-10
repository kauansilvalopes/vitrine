// Boxshelf PS5 - the shelf screen: backdrop, 3D shelf, top bar, title strip, hints.
// Ported from Boxshelf's renderer/app.js, style.css and setup.css. Everything is laid out in the 1920x1080 virtual
// canvas in CSS pixels; the console and the PC preview run the same code. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app/cover_loader.hpp"
#include "app/css.hpp"
#include "app/shelf.hpp"
#include "core/held_step.hpp"
#include "core/input.hpp"
#include "gl_canvas.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace boxshelf::app
{
struct Clock
{
    std::string date, time; // already formatted ("ter., 10 de nov.", "13:15")
};

class ShelfScreen
{
  public:
    void set_library(ShelfModel model, double now);
    // What the empty screen says while the library is read ("" once it has been read).
    void set_status(std::string status)
    {
        status_ = std::move(status);
    }
    // One frame of controller input (app.js gamepad loop, PAD_HINTS mapping).
    void input(const hui::InputFrame &in, double now, float dt);
    void draw(GlCanvas &canvas, double now, const Clock &clock);

    // For the preview and tests.
    ShelfModel &model()
    {
        return model_;
    }
    void settle(double now); // jumps every animation to its end
    // Covers asked for and not back yet (the preview waits for them).
    int covers_pending() const
    {
        return covers_pending_;
    }

  private:
    struct Slot
    {
        Glide x, z, ry, fade;
    };
    struct Backdrop
    {
        float shelf_hue = 0.0f; // shelfHue(g)
        float game_hue = 0.0f;
        double since = -10.0; // cross-fade start
    };

    void layout(double now, bool instant);
    void on_moved(double now);
    void build_slots(double now);
    float case_height() const;
    float shelf_hue() const;

    void draw_backdrop(hui::gfx::DrawList &list, double now);
    void draw_floor_and_halo(hui::gfx::DrawList &list, double now);
    void draw_cases(GlCanvas &canvas, double now);
    void draw_top_bar(hui::gfx::DrawList &list, double now, const Clock &clock);
    void draw_info(hui::gfx::DrawList &list, double now);
    void draw_hints(hui::gfx::DrawList &list);
    void draw_letter_flash(hui::gfx::DrawList &list, double now);
    void draw_empty(hui::gfx::DrawList &list);
    std::uint32_t cover_texture(GlCanvas &canvas, const Item &item);
    void update_covers(GlCanvas &canvas);

    ShelfModel model_;
    std::string status_ = "Looking for your games";
    std::vector<Slot> slots_;
    int shown_shelf_ = -1;
    Backdrop bg_now_, bg_prev_;
    double bg_due_ = -1.0; // updateBg's 120 ms timer
    double info_since_ = -10.0; // the title strip's rise
    double landed_at_ = -10.0;
    Glide pill_x_, pill_w_;
    std::string flash_;
    double flash_at_ = -10.0;
    hui::HeldStep letters_{hui::Action::page_prev, hui::Action::page_next};  // L1 / R1
    hui::HeldStep systems_{hui::Action::jump_prev, hui::Action::jump_next};  // L2 / R2
    std::unordered_map<std::string, std::vector<std::uint32_t>> covers_;     // placeholder covers by item key
    std::unordered_map<std::string, int> cover_size_;                        // width per key (height = w / ratio)
    // Real box art: the loader thread, the decoded covers by game id, and those not found.
    CoverLoader loader_;
    std::unordered_map<std::string, std::unique_ptr<CoverPixels>> art_;
    std::vector<std::string> art_new_; // decoded but not uploaded yet (the canvas must not reuse a stale texture)
    std::vector<std::string> art_missing_;
    std::string wish_;
    int covers_pending_ = 0;
    // Decoded covers kept (0.5-0.8 MB each at 360 px wide, plus the same again as a GPU texture); Estante keeps ~24.
    static constexpr std::size_t kMaxCovers = 32;
};
} // namespace boxshelf::app
