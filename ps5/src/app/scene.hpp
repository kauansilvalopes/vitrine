// Boxshelf PS5 - the shelf screen: backdrop, 3D shelf, top bar, title strip, hints.
// Ported from Boxshelf's renderer/app.js, style.css and setup.css. Everything is laid out in the 1920x1080 virtual
// canvas in CSS pixels; the console and the PC preview run the same code. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app/cover_loader.hpp"
#include "app/css.hpp"
#include "app/launcher.hpp"
#include "app/settings.hpp"
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
    // A cover the art fetcher just downloaded: the game shows it from now on.
    void art_arrived(const std::string &game_id, const std::string &path);
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
    void draw_spectrum_fx(hui::gfx::DrawList &list, double now);
    void draw_floor_and_halo(hui::gfx::DrawList &list, double now);
    void draw_cases(GlCanvas &canvas, double now);
    void draw_top_bar(hui::gfx::DrawList &list, double now, const Clock &clock);
    void draw_info(hui::gfx::DrawList &list, double now);
    void draw_hints(hui::gfx::DrawList &list);
    void draw_letter_flash(hui::gfx::DrawList &list, double now);
    void draw_empty(hui::gfx::DrawList &list);
    std::uint32_t cover_texture(GlCanvas &canvas, const Item &item);
    // covers.js drawBack(): the made-up back of a game with no back art, drawn into r of the current list.
    void draw_made_up_back(hui::gfx::DrawList &list, const Item &item, const hui::gfx::Rect &r);
    void set_flipped(bool on, double now);
    void play(double now);
    // app.js buildHome() / toggleFav() / homeLine(); L3: a random game on the shelf
    void rebuild_home(double now);
    void toggle_favorite(double now);
    void shuffle(double now);
    std::vector<std::string> meta_line(const Item &item) const;
    // app.js arrange(): the shelves as the library has them, sorted and filtered into model_ (Menu → Sort / Filter)
    void arrange(double now);

  public:
    // The owner's saved choices (language, games moved to another shelf); the quick menu changes and saves them.
    void set_settings(Settings settings)
    {
        settings_ = std::move(settings);
    }
    bool menu_open() const
    {
        return qm_open_;
    }
    void open_menu(double now);
    // Settings → Box art: 1 find what is missing, 2 download everything again; 0 when nothing was asked. Once.
    int take_art_request()
    {
        const int r = art_request_;
        art_request_ = 0;
        return r;
    }
    // The art fetcher's progress ("12 / 60", "" when idle), shown in the toast and in the menu.
    void set_art_status(const std::string &status, bool running, double now);
    // All the games on the shelves (for a new box art run).
    std::vector<data::Game> games() const;
    const Settings &settings() const
    {
        return settings_;
    }


  private:
    void toast(const std::string &message, bool working, bool done, double now);
    void draw_toast(hui::gfx::DrawList &list, double now);
    // app.js launch screen (#launch): the game's card, its console, a loading bar, the start's messages
    void draw_launch(GlCanvas &canvas, double now);
    // The quick menu (app.js "The quick menu", quickshelf.css): a glass panel from the right.
    struct MenuRow
    {
        int kind;            // 0 Move to shelf, 1 Language
        std::string label, value;
        std::vector<std::string> chips;
        int current;         // the chip in use
    };
    std::vector<MenuRow> menu_rows() const;
    void menu_input(const hui::InputFrame &in, double now);
    void menu_apply(const MenuRow &row, int chip, double now);
    void draw_menu(GlCanvas &canvas, double now);
    // Details (Square): app.js fillDetails(), with what the PS5 knows about the game
    void open_details(double now);
    // Inspect (R3): app.js inspect mode — the case in your hands, turned with the right stick, opened, its disc taken out
    void inspect_input(const hui::InputFrame &in, double now, float dt);
    void details_input(const hui::InputFrame &in, double now);
    void draw_details(GlCanvas &canvas, double now);
    std::vector<std::pair<std::string, std::string>> info_rows(const Item &item) const;
    std::vector<std::pair<std::string, std::string>> info_facts(const data::Game &game) const;
    // Menu → Sort / Filter: (value, label), built from what the library's info has
    std::vector<std::pair<std::string, std::string>> sort_options() const;
    std::vector<std::pair<std::string, std::string>> filter_options() const;
    float aside(double now) const; // how far the shelf has stepped aside, 0..1
    void update_covers(GlCanvas &canvas);

    ShelfModel model_;
    std::string status_ = "Looking for your games";
    std::vector<Slot> slots_;
    int shown_shelf_ = -1;
    Backdrop bg_now_, bg_prev_;
    double bg_due_ = -1.0; // updateBg's 120 ms timer
    double info_since_ = -10.0; // the title strip's rise
    double landed_at_ = -10.0;
    // themefx.js: the moving layer's colour eases toward the shelf's (hue += (hueT - hue) * min(1, dt * 1.5))
    float fx_hue_ = -1.0f;
    double fx_last_ = -1.0;
    // Triangle: flip() turns the selected case round (.box transition: transform .75s cubic-bezier(.3,.95,.25,1)).
    bool flipped_ = false;
    Glide flip_{0.0f, 0.0f, -1.0, .75f, Bezier{.3f, .95f, .25f, 1.0f}, true};
    double north_down_ = -1.0, menu_down_ = -1.0;
    ShelfModel library_; // every game, in each shelf's own order; model_ is what the sort and filter leave
    std::string starting_id_; // the game being started: it goes into the history once the console confirms it
    // the launch screen: shown from Cross until the start is over (+2.5 s when the game came up); Circle closes it
    bool launching_ = false, launch_failed_ = false;
    double launch_at_ = -10.0, launch_end_ = -1.0;
    Item launch_item_;
    std::string launch_note_;
    Settings settings_;
    bool qm_open_ = false;
    Glide qm_{0.0f, 0.0f, -1.0, .55f, Bezier{.22f, .9f, .25f, 1.0f}, true};
    Glide qm_hl_{0.0f, 0.0f, -1.0, .32f, Bezier{.3f, .9f, .25f, 1.0f}, false}; // the white pill's row
    int qm_row_ = 0, qm_sub_ = -1, qm_chip_ = 0;
    bool det_open_ = false;
    bool isp_on_ = false;
    int isp_state_ = 0;                   // 0 closed, 1 open, 2 the disc out
    float isp_yaw_ = 0.0f, isp_pitch_ = 0.0f, isp_zoom_ = 1.0f, isp_spin_ = 0.0f;
    double isp_idle_at_ = 0.0;
    Glide isp_in_{0.0f, 0.0f, -1.0, .6f, Bezier{.22f, .9f, .25f, 1.0f}, true};
    Glide isp_lid_{0.0f, 0.0f, -1.0, .7f, Bezier{.3f, .9f, .25f, 1.0f}, true};
    Glide isp_out_{0.0f, 0.0f, -1.0, .8f, Bezier{.3f, .9f, .25f, 1.0f}, true};
    Glide det_{0.0f, 0.0f, -1.0, .55f, Bezier{.22f, .9f, .25f, 1.0f}, true};
    int art_request_ = 0;
    std::string art_status_;
    bool art_running_ = false, art_asked_ = false;
    // Cross: Play. The launch runs on its own thread; its messages show in .toast (style.css).
    Launcher launcher_;
    unsigned launch_serial_ = 0;
    std::string toast_;
    bool toast_work_ = false, toast_done_ = false;
    double toast_at_ = -10.0, toast_until_ = -10.0;
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
    std::vector<std::unique_ptr<CoverPixels>> stale_; // replaced pictures, kept until the canvas has forgotten them
    std::vector<std::string> art_missing_;
    std::string wish_;
    int covers_pending_ = 0;
    // Decoded covers kept (0.5-0.8 MB each at 360 px wide, plus the same again as a GPU texture); Estante keeps ~24.
    static constexpr std::size_t kMaxCovers = 32;
};
} // namespace boxshelf::app
