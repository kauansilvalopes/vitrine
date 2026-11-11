// Boxshelf PS5 - the shelf screen. SPDX-License-Identifier: GPL-3.0-or-later
//
// Every number below comes from Boxshelf's renderer (style.css / setup.css / app.js); the comment next to it says
// which rule. CSS pixels are virtual pixels here (the canvas is 1920x1080, the size Boxshelf was measured at).
#include "app/scene.hpp"

#include "app/art_fetcher.hpp"
#include "app/i18n.hpp"
#include "app/ui_fonts.hpp"
#include "data/scan.hpp"
#include "data/covers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <ctime>

namespace boxshelf::app
{
namespace
{
using hui::gfx::Align;
using hui::gfx::Color;
using hui::gfx::DrawList;
using hui::gfx::Rect;

constexpr float W = 1920.0f, H = 1080.0f;
constexpr float kTrackX = W * 0.5f, kTrackY = H * 0.42f; // #track{left:50%;top:42%}
constexpr float kPerspective = 1800.0f;                  // #scene{perspective:1800px;perspective-origin:50% 42%}
constexpr float kPi = 3.14159265f;

const Color kInk = rgb(0xf3f5fb);                       // --ink
const Color kInkDim = rgb(0xf3f5fb, 0.62f);             // --ink-dim
const Color kLine = Color{1, 1, 1, 0.16f};              // --line

Color transparent(Color c)
{
    return c.with_alpha(0.0f);
}

// CSS color-mix(in srgb, a p, b): premultiplied blend.
Color color_mix(Color a, float p, Color b)
{
    const float aa = a.a * p, ba = b.a * (1.0f - p), alpha = aa + ba;
    if (alpha <= 0.0f)
        return {0, 0, 0, 0};
    return {(a.r * aa + b.r * ba) / alpha, (a.g * aa + b.g * ba) / alpha, (a.b * aa + b.b * ba) / alpha, alpha};
}

// ---- text -------------------------------------------------------------------------------------------------------

// Baseline for a line box starting at `top` (CSS: half-leading above, then the ascent).
float baseline(const UiFont &f, float size, float top, float line_height = 0.0f)
{
    const float asc = f.font->ascent(size), desc = f.font->descent(size);
    const float lh = line_height > 0.0f ? line_height : f.font->line_height(size);
    return top + (lh - (asc + desc)) * 0.5f + asc;
}

float text(DrawList &list, Face face, std::string_view s, float x, float top, float size, Color color,
           Align align = Align::left, float letter_spacing_em = 0.0f, float line_height = 0.0f)
{
    const UiFont f = app::face(face);
    if (!f)
        return 0.0f;
    return list.text(*f.font, f.texture, s, x, baseline(f, size, top, line_height), size, color, align,
                     letter_spacing_em * size);
}

float measure(Face face, std::string_view s, float size, float letter_spacing_em = 0.0f)
{
    const UiFont f = app::face(face);
    return f ? f.font->measure(s, size, letter_spacing_em * size) : 0.0f;
}

std::string upper(std::string_view s)
{
    // CSS text-transform: uppercase, for ASCII and the Latin-1 letters Portuguese and Spanish use (UTF-8 C3 A0..BE ->
    // C3 80..9E; ÷ and ÿ stay)
    std::string out(s);
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const auto c = static_cast<unsigned char>(out[i]);
        if (c >= 'a' && c <= 'z')
            out[i] = static_cast<char>(c - 'a' + 'A');
        else if (c == 0xc3 && i + 1 < out.size())
        {
            const auto n = static_cast<unsigned char>(out[i + 1]);
            if (n >= 0xa0 && n <= 0xbe && n != 0xb7)
                out[i + 1] = static_cast<char>(n - 0x20);
            ++i;
        }
    }
    return out;
}

// ---- per-shelf wordmarks (style.css "original per-shelf wordmarks") --------------------------------------------

struct Wordmark
{
    Face face;
    float size;    // clamp(min, vw, max) at 1920 wide: always the max here
    float spacing; // letter-spacing, em
    Color top, bottom, glow, tag;
    bool upper = true;
};

Wordmark wordmark(data::System s)
{
    using data::System;
    // .wm default: linear-gradient(180deg,#fff 15%,#cfd6e6 100%), glow rgba(255,255,255,.25), tag #c8cfdd;
    // .sys-word: 800, clamp(22px,2.6vw,38px), .sys-word.wm resets letter-spacing to normal.
    Wordmark w{Face::lexend800, 38.0f, 0.0f, rgb(0xffffff), rgb(0xcfd6e6), rgb(0xffffff, 0.25f), rgb(0xc8cfdd)};
    switch (s)
    {
    case System::home:
        return {Face::lexend800, 34, 0.10f, rgb(0xffffff), rgb(0xc9b8ff), Color{201 / 255.f, 184 / 255.f, 1, .4f}, rgb(0xbdb3e6)};
    case System::gc:
        return {Face::chakra700, 34, 0.14f, rgb(0xe2dbff), rgb(0x8b75ff), Color{139 / 255.f, 117 / 255.f, 1, .45f}, rgb(0xb7aaf2)};
    case System::wii:
        return {Face::syne800, 38, 0.24f, rgb(0xffffff), rgb(0x9cd2ff), Color{120 / 255.f, 190 / 255.f, 1, .5f}, rgb(0xbfe2ff)};
    case System::wiiu:
        return {Face::syne800, 38, 0.14f, rgb(0xffffff), rgb(0x5cc8ff), Color{60 / 255.f, 180 / 255.f, 1, .5f}, rgb(0xa8ddff)};
    case System::sw: // 95deg gradient: drawn top to bottom here
        return {Face::unbounded700, 36, 0.03f, rgb(0xff9a8a), rgb(0xff3d55), Color{1, 61 / 255.f, 85 / 255.f, .45f}, rgb(0xffb3b8)};
    case System::sw2:
        return {Face::unbounded700, 36, 0.03f, rgb(0xffb0a6), rgb(0xff2a3b), Color{1, 42 / 255.f, 59 / 255.f, .45f}, rgb(0xffc2c7)};
    case System::psp:
        return {Face::michroma400, 34, 0.32f, rgb(0xf7f8fc), rgb(0x8d95a8), Color{200 / 255.f, 210 / 255.f, 235 / 255.f, .3f}, rgb(0xaab1c2)};
    case System::ps2:
        return {Face::michroma400, 34, 0.20f, rgb(0xeef1ff), rgb(0x8fa6ff), Color{143 / 255.f, 166 / 255.f, 1, .4f}, rgb(0xb4c0f5)};
    case System::ps3:
        return {Face::michroma400, 34, 0.22f, rgb(0xf2f6ff), rgb(0x7ab4ff), Color{122 / 255.f, 180 / 255.f, 1, .4f}, rgb(0xb0cdf5)};
    case System::ps4:
        return {Face::audiowide400, 34, 0.18f, rgb(0xeaf1ff), rgb(0x6aa6ff), Color{106 / 255.f, 166 / 255.f, 1, .4f}, rgb(0xa9c6f5)};
    case System::x360:
        return {Face::bungee400, 34, 0.06f, rgb(0xeaffea), rgb(0x5fd35f), Color{95 / 255.f, 211 / 255.f, 95 / 255.f, .4f}, rgb(0xa9e6a9)};
    case System::n3ds: // Unbounded 800 asked; 700 is the weight Boxshelf ships
        return {Face::unbounded700, 36, 0.02f, rgb(0xff8a95), rgb(0xe60012), Color{230 / 255.f, 0, 18 / 255.f, .42f}, rgb(0xffb3ba)};
    case System::psx:
        w.top = rgb(0xf2f2f5), w.bottom = rgb(0x9aa3b5), w.glow = Color{160 / 255.f, 170 / 255.f, 190 / 255.f, .35f};
        w.tag = rgb(0xc3c9d6);
        return w;
    case System::n64: // .wm-n64
        w.top = rgb(0xdff6e8), w.bottom = rgb(0x18a85c), w.glow = Color{24 / 255.f, 168 / 255.f, 92 / 255.f, .4f};
        w.tag = rgb(0x9ed9b8);
        return w;
    case System::nds:
        w.top = rgb(0xffffff), w.bottom = rgb(0xa7b1bf), w.glow = Color{167 / 255.f, 177 / 255.f, 191 / 255.f, .35f};
        w.tag = rgb(0xcdd3dc);
        return w;
    case System::custom: // .wm-custom: the PC lettering in a warm tint
        w.top = rgb(0xfff0e0), w.bottom = rgb(0xe6955a), w.glow = Color{230 / 255.f, 149 / 255.f, 90 / 255.f, .4f};
        w.tag = rgb(0xecc29c);
        return w;
    case System::media:
        return {Face::dmserif_italic, 44, 0.0f, rgb(0xefe6ff), rgb(0xa98bff), Color{169 / 255.f, 139 / 255.f, 1, .45f}, rgb(0xc9b8ff), false};
    default: // PS5 and anything else: the plain .wm look
        return w;
    }
}

// The shelf's name on a tab (CONSOLES[].name; renderer shows `s.name`).
const char *tab_name(data::System s)
{
    using data::System;
    switch (s)
    {
    case System::home: return tr("Home");
    case System::gc: return "GameCube";
    case System::wii: return "Wii";
    case System::wiiu: return "Wii U";
    case System::sw: return "Switch";
    case System::sw2: return "Switch 2";
    case System::psp: return "PSP";
    case System::ps2: return "PS2";
    case System::ps3: return "PS3";
    case System::ps4: return "PS4";
    case System::ps5: return "PS5";
    case System::x360: return "Xbox 360";
    case System::n3ds: return "3DS";
    case System::psx: return "PlayStation";
    case System::nds: return "DS";
    case System::n64: return "N64";
    case System::custom: return tr("Others");
    case System::media: return tr("Media");
    default: return "";
    }
}

// The game's meta line (app.js metaFor()).
std::vector<std::string> meta_for(const Item &it, data::System shelf)
{
    switch (it.kind)
    {
    case ItemKind::emulator:
        return {tr("Emulator"), tr("Open settings")};
    case ItemKind::setup:
        return {trf("No %s games yet", tab_name(shelf)), trf("Cross opens %s settings to add a games folder", tab_name(shelf))};
    case ItemKind::random:
        return {tr("Shuffle"), tr("Lands on a random game")};
    default:
        break;
    }
    std::vector<std::string> out;
    const auto &g = it.game;
    // [region, ext, size, discs, manual, achievements]: what the PS5 knows today is the file's extension.
    const auto dot = g.path.rfind('.');
    if (!g.folder && dot != std::string::npos && g.path.size() - dot <= 6)
        out.push_back(upper(g.path.substr(dot + 1)));
    if (g.system == data::System::ps5 || g.system == data::System::ps4 || g.system == data::System::media)
        out.push_back(g.app);
    return out;
}
} // namespace

// =================================================================================================================

void ShelfScreen::set_library(ShelfModel model, double now)
{
    library_ = std::move(model);
    model_ = library_;
    arrange(now);
    shown_shelf_ = -1;
    layout(now, true);
    settle(now);
}

float ShelfScreen::case_height() const
{
    const auto &s = data::style(model_.empty() ? data::System::media : model_.cur().system);
    return data::case_size(s, W, H).h; // measure(): min(vh*.4, vw*.22/ratio, 620)
}

float ShelfScreen::shelf_hue() const
{
    // app.js shelfHue(): the system's hue walked +-16 degrees along the shelf. No shelves: st() is STYLE.media.
    if (model_.empty())
        return static_cast<float>(data::style(data::System::media).hue);
    const auto &s = data::style(model_.cur().system);
    return static_cast<float>(s.hue) + (model_.position() * 2.0f - 1.0f) * 16.0f;
}

void ShelfScreen::build_slots(double now)
{
    slots_.assign(model_.empty() ? 0 : model_.cur().items.size(), Slot{});
    shown_shelf_ = model_.empty() ? -1 : model_.current();
    for (auto &s : slots_)
        s.fade.duration = 0.4f; // .slot{transition: ..., opacity .4s}
    layout(now, true);
}

void ShelfScreen::layout(double now, bool instant)
{
    if (model_.empty())
        return;
    if (shown_shelf_ != model_.current())
    {
        build_slots(now);
        return;
    }
    const auto &st = data::style(model_.cur().system);
    const auto size = data::case_size(st, W, H);
    const int sel = model_.selected();
    for (int i = 0; i < static_cast<int>(slots_.size()); ++i)
    {
        const Pose p = place(i, sel, size.w, size.d);
        auto &s = slots_[static_cast<std::size_t>(i)];
        s.x.set(p.x, now, instant);
        s.z.set(p.z, now, instant);
        s.ry.set(p.ry_deg, now, instant);
        // .slot.far{opacity:0} past WIN-2
        s.fade.set(std::abs(i - sel) > kWindow - 2 ? 0.0f : 1.0f, now, instant);
    }
}

void ShelfScreen::settle(double now)
{
    if (!model_.empty() && shown_shelf_ != model_.current())
        build_slots(now);
    layout(now, true);
    for (auto &s : slots_)
        for (Glide *g : {&s.x, &s.z, &s.ry, &s.fade})
            g->set(g->to, now, true);
    // the pill takes its first place when the bar is first drawn; afterwards it is snapped here
    if (pill_x_.placed)
    {
        pill_x_.set(pill_x_.to, now, true);
        pill_w_.set(pill_w_.to, now, true);
    }
    bg_due_ = -1.0;
    const Item *it = model_.item();
    bg_now_ = {shelf_hue(), it ? static_cast<float>(it->hue) : 0.0f, -10.0};
    bg_prev_ = bg_now_;
    info_since_ = -10.0;
    landed_at_ = now - 10.0;
}

void ShelfScreen::on_moved(double now)
{
    layout(now, false);
    bg_due_ = now + 0.12; // updateBg(): setTimeout(..., 120)
    info_since_ = now;    // .info rise 450 ms
    landed_at_ = now;     // landSel(): .landed after 380 ms (sheen)
    // a new selection starts face up (its box gets a fresh rotateY(0))
    flipped_ = false;
    flip_.set(0.0f, now, true);
}

void ShelfScreen::play(double now)
{
    const Item *it = model_.item();
    if (!it || it->kind != ItemKind::game)
        return;
    if (launcher_.busy())
        return toast(tr("Still starting the last game"), false, false, now);
    const auto &g = it->game;
    LaunchRequest r;
    r.title = it->title;
    const auto sys = it->system;
    // installed titles (games, apps, ports) open by their own id, whichever shelf they were moved to
    if (g.path.rfind("/appdb/", 0) == 0 || sys == data::System::ps5 || sys == data::System::ps4 ||
        sys == data::System::custom || sys == data::System::media)
        r.app = g.app;
    else
    {
        r.app = std::string(data::emulator_app(sys));
        r.target = g.path;
        r.ps1_folder = sys == data::System::psx && g.folder;
        r.switch_folder = sys == data::System::sw && g.folder;
    }
    if (r.app.size() != 9)
        return toast(trf("No emulator for %s on this console", tab_name(sys)), false, false, now);
    if (launcher_.start(std::move(r)))
    {
        starting_id_ = g.id; // app.js HIST: recorded once the game is up (draw_toast)
        launching_ = true, launch_failed_ = false;
        launch_at_ = now, launch_end_ = -1.0;
        launch_item_ = *it;
        launch_note_.clear();
        det_open_ = false, det_.set(0.0f, now);
        if (isp_on_)
            isp_on_ = false, isp_state_ = 0, isp_in_.set(0.0f, now), isp_lid_.set(0.0f, now), isp_out_.set(0.0f, now);
    }
}

// .toast: a white pill sliding down from the top; a working one has a turning ring and stays, others leave.
void ShelfScreen::toast(const std::string &message, bool working, bool done, double now)
{
    if (toast_.empty() || now > toast_until_ + .45)
        toast_at_ = now;
    toast_ = message;
    toast_work_ = working;
    toast_done_ = done;
    toast_until_ = working ? now + 3600.0 : now + 2.6;
}

void ShelfScreen::draw_toast(DrawList &list, double now)
{
    const auto st = launcher_.status();
    if (st.serial != launch_serial_)
    {
        launch_serial_ = st.serial;
        if (launching_)
        {
            launch_note_ = st.message;
            if (!st.working)
            {
                launch_failed_ = !st.ok;
                launch_end_ = st.ok ? now + 2.5 : -1.0; // up: the screen steps aside; failed: it stays until Circle
            }
        }
        else
            toast(st.message, st.working, st.ok && !st.working, now);
        if (!st.working && !starting_id_.empty())
        {
            // the start is over: a confirmed one goes into the history (Home: recently played)
            if (st.ok)
            {
                settings_.played(starting_id_, static_cast<long long>(std::time(nullptr)));
                save_settings(settings_);
                if (settings_.filter == "played" || settings_.filter == "unplayed" || settings_.sort != "az")
                    arrange(now);
                else
                    rebuild_home(now);
            }
            starting_id_.clear();
        }
    }
    if (toast_.empty())
        return;
    // transform .45s cubic-bezier(.22,.9,.25,1) from translateY(calc(-100% - 60px)) to 0
    static const Bezier ease{.22f, .9f, .25f, 1.0f};
    const float in = ease(static_cast<float>((now - toast_at_) / .45));
    const float out = now > toast_until_ ? ease(static_cast<float>((now - toast_until_) / .45)) : 0.0f;
    if (out >= 1.0f)
    {
        toast_.clear();
        return;
    }
    // padding 9px 16px, 13px 600, border-radius 999px, background rgba(255,255,255,.95), color #10121a
    const float fs = 13.0f, ring = toast_work_ ? 11.0f + 9.0f : 0.0f;
    const float tw = measure(Face::lexend600, toast_, fs) + ring;
    const float h = 9.0f * 2 + 17.0f, w = tw + 32.0f;
    const float top = 22.0f, hidden = -(h + 60.0f);
    const float y = top + hidden * (1.0f - in) + (hidden - top) * out;
    const float x = (W - w) * .5f;
    list.rounded_rect({x, y, w, h}, h * .5f, Color{1, 1, 1, .95f});
    float tx = x + 16.0f;
    if (toast_work_)
    {
        // .toast.work::before: 11px ring, 2px border rgba(16,18,26,.22), top #10121a, turning every .8 s
        const float cx = tx + 5.5f, cy = y + h * .5f;
        list.ring(cx, cy, 5.5f, 2.0f, Color{16 / 255.f, 18 / 255.f, 26 / 255.f, .22f});
        const float a = static_cast<float>(std::fmod(now / .8, 1.0)) * 6.2831853f;
        list.arc(cx, cy, 5.5f, 2.0f, a, 1.5708f, rgb(0x10121a), false);
        tx += ring;
    }
    text(list, Face::lexend600, toast_, tx, y + 9.0f, fs, toast_done_ ? rgb(0x0f3d24) : rgb(0x10121a), Align::left, 0.0f,
         17.0f);
}

void ShelfScreen::set_art_status(const std::string &status, bool running, double now)
{
    const bool was = art_running_;
    art_running_ = running;
    if (running && status != art_status_ && !status.empty())
        toast(trf("Box art %s", status), true, false, now);
    else if (was && !running && art_asked_)
        toast(tr("Box art up to date"), false, true, now);
    if (was && !running)
    {
        art_asked_ = false;
        if (!toast_.empty() && toast_work_ && !launcher_.busy())
            toast_until_ = now; // the progress pill leaves
    }
    art_status_ = status;
}

std::vector<data::Game> ShelfScreen::games() const
{
    std::vector<data::Game> out;
    for (const Item *it : library_.all_games())
        out.push_back(it->game);
    return out;
}

void ShelfScreen::rebuild_home(double now)
{
    // app.js buildHome(): the 12 most recently played games, then the favourites (A to Z), each with its own case
    std::vector<const Item *> all = library_.all_games(); // Home ignores the filter, as in app.js
    std::vector<Item> items;
    std::vector<std::pair<long long, const Item *>> played;
    for (const Item *it : all)
        if (const auto *h = settings_.history_of(it->game.id); h && h->count > 0)
            played.emplace_back(h->last, it);
    std::stable_sort(played.begin(), played.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    std::vector<std::string> seen;
    for (std::size_t i = 0; i < played.size() && i < 12; ++i)
    {
        items.push_back(*played[i].second);
        seen.push_back(played[i].second->game.id);
    }
    std::vector<const Item *> favs;
    for (const Item *it : all)
        if (settings_.is_favorite(it->game.id) && std::find(seen.begin(), seen.end(), it->game.id) == seen.end())
            favs.push_back(it);
    std::stable_sort(favs.begin(), favs.end(), [](const Item *a, const Item *b) { return data::title_less(a->title, b->title); });
    for (const Item *it : favs)
        items.push_back(*it);
    const bool on_home = !model_.empty() && model_.cur().system == data::System::home;
    const int before = model_.current();
    model_.set_home(std::move(items));
    if (on_home || model_.empty() || model_.cur().system == data::System::home)
        shown_shelf_ = -1; // Home's cases change: build them again
    else if (model_.current() != before && shown_shelf_ == before)
        shown_shelf_ = model_.current(); // the same shelf, at a new index
    (void)now;
}

std::vector<std::pair<std::string, std::string>> ShelfScreen::sort_options() const
{
    // app.js SORTS (play time is counted in starts here)
    return {{"az", "A\xe2\x80\x93Z"},           {"year", tr("Release year")}, {"recent", tr("Last played")},
            {"most", tr("Most played")}, {"rating", tr("Score")},       {"genre", tr("Genre")}};
}

std::vector<std::pair<std::string, std::string>> ShelfScreen::filter_options() const
{
    // app.js filterList(): the fixed ones, 2+ players and co-op when any game is, each decade, the 12 commonest genres
    std::vector<std::pair<std::string, std::string>> list = {
        {"all", tr("All games")}, {"fav", tr("Favorites")}, {"played", tr("Played")}, {"unplayed", tr("Not played yet")}};
    std::vector<int> decades;
    std::map<std::string, int> genres;
    bool multi = false, coop = false;
    for (const Item *it : library_.all_games())
    {
        const auto &in = it->game.info;
        if (const int y = std::atoi(in.year.c_str()); y > 1900)
            if (std::find(decades.begin(), decades.end(), y / 10 * 10) == decades.end())
                decades.push_back(y / 10 * 10);
        multi = multi || std::atoi(in.players.c_str()) >= 2;
        coop = coop || in.coop;
        std::size_t start = 0;
        while (!in.genres.empty() && start <= in.genres.size())
        {
            auto semi = in.genres.find(';', start);
            if (semi == std::string::npos)
                semi = in.genres.size();
            std::string one = in.genres.substr(start, semi - start);
            while (!one.empty() && one.front() == ' ')
                one.erase(0, 1);
            if (!one.empty())
                ++genres[one];
            start = semi + 1;
        }
    }
    if (multi)
        list.push_back({"multi", tr("2+ players")});
    if (coop)
        list.push_back({"coop", tr("Co-op")});
    std::sort(decades.begin(), decades.end());
    for (int d : decades)
        list.push_back({"dec:" + std::to_string(d), std::to_string(d) + "s"});
    std::vector<std::pair<int, std::string>> top;
    for (const auto &[n, c] : genres)
        top.push_back({c, n});
    std::stable_sort(top.begin(), top.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    if (top.size() > 12)
        top.resize(12);
    std::sort(top.begin(), top.end(), [](const auto &a, const auto &b) { return a.second < b.second; });
    for (const auto &[c, n] : top)
        list.push_back({"genre:" + n, n});
    return list;
}

void ShelfScreen::arrange(double now)
{
    const data::System keep_sys = model_.empty() ? data::System::count : model_.cur().system;
    const Item *was = model_.item();
    const std::string keep_id = was && was->kind == ItemKind::game ? was->game.id : std::string();
    const std::string &f = settings_.filter, &order = settings_.sort;
    auto keep = [&](const Item &it)
    {
        if (it.kind != ItemKind::game || f == "all")
            return true;
        const auto *h = settings_.history_of(it.game.id);
        const bool played = h && h->count > 0;
        const auto &in = it.game.info;
        if (f == "multi")
            return std::atoi(in.players.c_str()) >= 2;
        if (f == "coop")
            return in.coop;
        if (f.rfind("dec:", 0) == 0)
        {
            const int y = std::atoi(in.year.c_str()), d = std::atoi(f.c_str() + 4);
            return y >= d && y < d + 10;
        }
        if (f.rfind("genre:", 0) == 0)
        {
            const std::string want = f.substr(6);
            std::size_t start = 0;
            while (start <= in.genres.size())
            {
                auto semi = in.genres.find(';', start);
                if (semi == std::string::npos)
                    semi = in.genres.size();
                std::string one = in.genres.substr(start, semi - start);
                while (!one.empty() && one.front() == ' ')
                    one.erase(0, 1);
                if (one == want)
                    return true;
                start = semi + 1;
            }
            return false;
        }
        return f == "fav" ? settings_.is_favorite(it.game.id) : f == "played" ? played : f == "unplayed" ? !played : true;
    };
    // app.js arrange(): played ones by the last start / most starts; year and rating with the unknown last; genre A-Z
    auto key = [&](const Item &it) -> double
    {
        const auto &in = it.game.info;
        if (order == "year")
            return in.year.empty() ? -1e9 : -std::atof(in.year.c_str()); // ascending, as by(yearOf)
        if (order == "rating")
            return in.rating.empty() ? -1e9 : std::atof(in.rating.c_str());
        const auto *h = settings_.history_of(it.game.id);
        if (!h || h->count == 0)
            return -1;
        return order == "recent" ? static_cast<double>(h->last) : h->count;
    };
    std::vector<Shelf> out;
    for (int i = 0; i < library_.shelf_count(); ++i)
    {
        const Shelf &src = library_.shelf(i);
        if (src.system == data::System::home)
            continue;
        Shelf s;
        s.system = src.system;
        for (const auto &it : src.items)
            if (keep(it))
                s.items.push_back(it);
        if (order == "recent" || order == "most" || order == "year" || order == "rating")
            std::stable_sort(s.items.begin(), s.items.end(), [&](const Item &a, const Item &b) { return key(a) > key(b); });
        else if (order == "genre") // the first genre A-Z, the unknown last
            std::stable_sort(s.items.begin(), s.items.end(), [](const Item &a, const Item &b)
                             {
                                 const std::string x = a.game.info.genres.substr(0, a.game.info.genres.find(';'));
                                 const std::string y = b.game.info.genres.substr(0, b.game.info.genres.find(';'));
                                 if (x.empty() != y.empty())
                                     return y.empty();
                                 return x < y;
                             });
        s.game_count = static_cast<int>(std::count_if(s.items.begin(), s.items.end(),
                                                      [](const Item &x) { return x.kind == ItemKind::game; }));
        if (s.game_count > 0) // a shelf the filter empties goes, like a shelf with no games
            out.push_back(std::move(s));
    }
    int current = 0;
    for (std::size_t i = 0; i < out.size(); ++i)
        if (out[i].system == keep_sys)
            current = static_cast<int>(i);
    model_.set(std::move(out), current);
    rebuild_home(now);
    if (!model_.empty() && !keep_id.empty())
        for (std::size_t i = 0; i < model_.cur().items.size(); ++i)
            if (model_.cur().items[i].kind == ItemKind::game && model_.cur().items[i].game.id == keep_id)
                model_.go(static_cast<int>(i));
    shown_shelf_ = -1;
    flipped_ = false;
    flip_.set(0.0f, now, true);
}

void ShelfScreen::toggle_favorite(double now)
{
    const Item *it = model_.item();
    if (!it || it->kind != ItemKind::game)
        return;
    const std::string id = it->game.id, title = it->title;
    const bool on = settings_.toggle_favorite(id);
    if (!save_settings(settings_))
        toast(tr("Could not save the settings"), false, false, now);
    else
        toast(on ? trf("%s added to favorites", title) : trf("Removed %s from favorites", title), false, on, now);
    if (settings_.filter == "fav")
        arrange(now);
    else
        rebuild_home(now);
}

void ShelfScreen::shuffle(double now)
{
    // L3: lands on a random game of this shelf (the "Surprise me" tile's job, without the tile)
    const int n = static_cast<int>(model_.empty() ? 0 : model_.cur().items.size());
    if (n < 2)
        return;
    static unsigned seed = static_cast<unsigned>(std::time(nullptr));
    seed = seed * 1103515245u + 12345u;
    int pick = static_cast<int>((seed >> 8) % static_cast<unsigned>(n - 1));
    if (pick >= model_.selected())
        ++pick;
    if (model_.go(pick))
        on_moved(now);
}

std::vector<std::string> ShelfScreen::meta_line(const Item &it) const
{
    if (model_.empty() || model_.cur().system != data::System::home || it.kind != ItemKind::game)
        return meta_for(it, model_.empty() ? data::System::count : model_.cur().system);
    // app.js homeLine(): its console, "played <day>", favourite
    std::vector<std::string> parts{tab_name(it.system)};
    if (const auto *h = settings_.history_of(it.game.id); h && h->count > 0)
    {
        const long long days = (static_cast<long long>(std::time(nullptr)) - h->last) / 86400;
        parts.push_back(days <= 0   ? std::string(tr("played today"))
                        : days == 1 ? std::string(tr("played yesterday"))
                                    : trf("played %s days ago", std::to_string(days)));
    }
    if (settings_.is_favorite(it.game.id))
        parts.push_back(tr("Favorite"));
    return parts;
}

void ShelfScreen::draw_launch(GlCanvas &canvas, double now)
{
    if (!launching_)
        return;
    if (launch_end_ >= 0.0 && now > launch_end_ + .6)
    {
        launching_ = false;
        return;
    }
    DrawList &list = canvas.list();
    const Item &it = launch_item_;
    const auto &st = data::style(it.system);
    // .launch: opacity .6s in (and out when it steps aside)
    float o = std::clamp(static_cast<float>((now - launch_at_) / .6), 0.0f, 1.0f);
    if (launch_end_ >= 0.0 && now > launch_end_)
        o *= 1.0f - std::clamp(static_cast<float>((now - launch_end_) / .6), 0.0f, 1.0f);
    list.push_opacity(o);
    // background: --l-t1, with .l-bg::after (radial vignette to t1, and t0 30% -> t1 75% from the top)
    const Color t0 = rgb(st.tint_top), t1 = rgb(st.tint_bottom), glow = rgb(st.glow);
    list.rounded_rect({0, 0, W, H}, 0, t1);
    list.gradient_rect({0, 0, W, H}, 0, t0.with_alpha(.30f), t1.with_alpha(.75f));
    list.radial_gradient({0, 0, W, H}, W * .5f, H * .4f, W * .55f, H * .5f, t0.with_alpha(.35f), t1.with_alpha(.88f), 0.0f, 1.0f);
    // the card: min(38vh, 380px) tall, padding 5 in the case colour, radius 12, a glow; lCardIn .7s then lFloat
    const float t = static_cast<float>(now - launch_at_);
    static const Bezier in_ease{.2f, .8f, .2f, 1.0f};
    const float ci = in_ease(std::clamp(t / .7f, 0.0f, 1.0f));
    const float flt = t > .7f ? -8.0f * (.5f - .5f * std::cos((t - .7f) / 4.8f * kPi)) : 0.0f;
    const float ch = 380.0f, cw = (ch - 10.0f) * st.ratio + 10.0f;
    const float cs = .94f + .06f * ci, cy0 = 160.0f + 18.0f * (1.0f - ci) + flt;
    const float cx = W * .5f - cw * cs * .5f;
    const Rect card{cx, cy0, cw * cs, ch * cs};
    list.push_opacity(ci);
    list.shadow({card.x, card.y + 34, card.w, card.h}, 12, 80, Color{0, 0, 0, .55f});
    list.glow(card, 12, 70, glow.with_alpha(.30f));
    list.rounded_rect(card, 12, rgb(st.case_color));
    if (const std::uint32_t cover = cover_texture(canvas, it))
        list.image(cover, {card.x + 5 * cs, card.y + 5 * cs, card.w - 10 * cs, card.h - 10 * cs}, hui::gfx::kFullUv,
                   Color{1, 1, 1, 1}, 8.0f);
    // .l-glare: a 40 %-wide light crossing the card once (1.1s after .55s)
    {
        const float g = std::clamp((t - .55f) / 1.1f, 0.0f, 1.0f);
        if (g > 0.0f && g < 1.0f)
        {
            list.push_clip(card);
            const float gx = card.x - card.w * .6f + (card.w * 2.2f) * static_cast<float>(Bezier{.3f, .6f, .3f, 1.0f}(g));
            list.linear_gradient({gx, card.y, card.w * .2f, card.h}, 100.0f, Color{1, 1, 1, 0}, Color{1, 1, 1, .35f}, 0.0f, 1.0f);
            list.linear_gradient({gx + card.w * .2f, card.y, card.w * .2f, card.h}, 100.0f, Color{1, 1, 1, .35f}, Color{1, 1, 1, 0},
                                 0.0f, 1.0f);
            list.pop_clip();
        }
    }
    list.pop_opacity();
    // .l-word (the console's wordmark, 28px), .l-title 40px 600, .l-bar 220 x 3 filling over 1.8s, .l-emu, .l-note
    float y = cy0 + ch + 30.0f;
    {
        const Wordmark wm = wordmark(it.system);
        const std::string word = wm.upper ? upper(tr(st.word.data())) : std::string(tr(st.word.data()));
        const float size = std::min(28.0f, wm.size);
        const float up = std::clamp((t - .2f) / .6f, 0.0f, 1.0f);
        list.push_opacity(up);
        const std::size_t first = list.mark();
        text(list, wm.face, word, W * .5f, y + 10.0f * (1.0f - up), size, kInk, Align::center, wm.spacing, size);
        list.recolor(first, y, y + size, wm.top, wm.bottom);
        list.pop_opacity();
        y += size + 10.0f;
    }
    text(list, Face::lexend600, it.title, W * .5f, y, 40.0f, kInk, Align::center, 0.0f, 48.0f);
    y += 48.0f + 24.0f;
    {
        static const Bezier load{.5f, 0.0f, .2f, 1.0f};
        const float f = load(std::clamp(t / 1.8f, 0.0f, 1.0f));
        list.rounded_rect({W * .5f - 110.0f, y, 220.0f, 3.0f}, 1.5f, Color{1, 1, 1, .12f});
        list.rounded_rect({W * .5f - 110.0f, y, 220.0f * f, 3.0f}, 1.5f, kInk);
        y += 3.0f + 18.0f;
    }
    const bool app = it.game.path.rfind("/appdb/", 0) == 0;
    text(list, Face::lexend500, upper(app ? std::string(tab_name(it.system)) : std::string(emulator_name(it.system))),
         W * .5f, y, 11.0f, kInkDim, Align::center, .26f, 14.0f);
    y += 14.0f + 26.0f;
    if (!launch_note_.empty())
    {
        text(list, Face::lexend500, launch_note_, W * .5f, y, 13.0f, launch_failed_ ? rgb(0xff8a8a) : kInkDim, Align::center,
             0.0f, 18.0f);
        if (launch_failed_)
        {
            // the Circle drawn as a shape (the fonts have no such glyph), then the words
            const std::string back = tr("to go back");
            const float tw = measure(Face::lexend500, back, 12.0f) + 18.0f, bx = W * .5f - tw * .5f;
            list.ring(bx + 6.0f, y + 32.0f, 6.0f, 1.6f, rgb(0xff7a7a));
            text(list, Face::lexend500, back, bx + 18.0f, y + 24.0f, 12.0f, kInkDim, Align::left, 0.0f, 16.0f);
        }
    }
    list.pop_opacity();
}

void ShelfScreen::set_flipped(bool on, double now)
{
    const Item *it = model_.item();
    if (!it || it->kind != ItemKind::game)
        on = false;
    if (on == flipped_)
        return;
    flipped_ = on;
    flip_.set(on ? 180.0f : 0.0f, now);
    landed_at_ = now - 10.0; // flip() takes .landed off: no sheen on the turned case
}

void ShelfScreen::input(const hui::InputFrame &in, double now, float dt)
{
    if (launching_)
    {
        if (in.is_pressed(hui::Action::back) && (launch_failed_ || launch_end_ >= 0.0))
            launch_end_ = now;
        return;
    }
    if (qm_open_)
        return menu_input(in, now);
    if (det_open_)
        return details_input(in, now);
    if (isp_on_)
        return inspect_input(in, now, dt);
    // R3: Inspect (PAD_HINTS)
    if (in.is_pressed(hui::Action::r3) && model_.item() && model_.item()->kind == ItemKind::game)
    {
        isp_on_ = true, isp_state_ = 0, isp_yaw_ = 0.0f, isp_pitch_ = 0.0f, isp_zoom_ = 1.0f;
        isp_in_.set(1.0f, now), isp_lid_.set(0.0f, now, true), isp_out_.set(0.0f, now, true);
        set_flipped(false, now);
        return;
    }
    // Square: Details (PAD_HINTS)
    if (in.is_pressed(hui::Action::west))
        return open_details(now);
    // Options: a tap opens the menu, a hold opens it on Sort (PAD_HINTS: Menu, hold Sort)
    if (in.is_pressed(hui::Action::menu))
        menu_down_ = now;
    if (menu_down_ >= 0.0 && in.is_held(hui::Action::menu) && now - menu_down_ >= .5)
    {
        menu_down_ = -1.0;
        open_menu(now);
        const auto rows = menu_rows();
        for (std::size_t r = 0; r < rows.size(); ++r)
            if (rows[r].kind == 5)
            {
                qm_row_ = qm_sub_ = static_cast<int>(r);
                qm_chip_ = std::max(0, rows[r].current);
                qm_hl_.set(static_cast<float>(r), now, true);
            }
        return;
    }
    if ((in.released & hui::action_bit(hui::Action::menu)) && menu_down_ >= 0.0)
    {
        menu_down_ = -1.0;
        return open_menu(now);
    }
    if (model_.empty())
        return;
    // D-pad / left stick: one case per step (go(sel +- 1)); the tracker repeats a held direction.
    if (in.nav == hui::Direction::left && model_.step(-1))
        on_moved(now);
    else if (in.nav == hui::Direction::right && model_.step(1))
        on_moved(now);
    // L1 / R1: letter sections (HINTS: Letter = L1 R1).
    if (const int d = letters_.step(in, dt))
    {
        std::string label;
        if (model_.letter(d, &label))
        {
            flash_ = label;
            flash_at_ = now;
            on_moved(now);
        }
    }
    // Triangle: a tap flips (PAD_HINTS: Flip, hold for favourite); Circle turns a flipped case back.
    if (in.is_pressed(hui::Action::north))
        north_down_ = now;
    if (north_down_ >= 0.0 && in.is_held(hui::Action::north) && now - north_down_ >= .5)
    {
        north_down_ = -1.0; // held: Favorite (PAD_HINTS: Flip, hold ♥)
        toggle_favorite(now);
    }
    if ((in.released & hui::action_bit(hui::Action::north)) && north_down_ >= 0.0)
    {
        if (now - north_down_ < .5)
            set_flipped(!flipped_, now);
        north_down_ = -1.0;
    }
    if (in.is_pressed(hui::Action::l3))
        shuffle(now);
    if (in.is_pressed(hui::Action::back) && flipped_)
        set_flipped(false, now);
    // Cross: Play (PAD_HINTS)
    if (in.is_pressed(hui::Action::confirm))
        play(now);
    // L2 / R2: System (PAD_HINTS: System = L2 R2); Up / Down do the same on the keyboard.
    int sys = systems_.step(in, dt);
    if (in.nav == hui::Direction::up)
        sys = -1;
    else if (in.nav == hui::Direction::down)
        sys = 1;
    if (sys && model_.switch_shelf(sys))
    {
        build_slots(now);
        on_moved(now);
        bg_due_ = now; // a new shelf repaints the wash at once
    }
}

// ---- backdrop ---------------------------------------------------------------------------------------------------

void ShelfScreen::draw_backdrop(DrawList &list, double now)
{
    if (model_.empty())
        bg_now_ = bg_prev_ = {shelf_hue(), 0.0f, -10.0};
    if (bg_due_ >= 0.0 && now >= bg_due_)
    {
        bg_due_ = -1.0;
        const Item *it = model_.item();
        Backdrop next{shelf_hue(), it ? static_cast<float>(it->hue) : 0.0f, now};
        if (std::fabs(next.shelf_hue - bg_now_.shelf_hue) > 0.01f || next.game_hue != bg_now_.game_hue)
        {
            bg_prev_ = bg_now_;
            bg_now_ = next;
        }
    }
    // body{background:#07070c}
    list.rounded_rect({0, 0, W, H}, 0, rgb(0x07070c));
    // Theme "Spectrum" tint (app.js THEMES.spectrum.tint), on two layers cross-fading over .8 s (.bg-layer).
    auto wash = [&](const Backdrop &b, float opacity)
    {
        if (opacity <= 0.0f)
            return;
        list.push_opacity(opacity);
        const float sh = b.shelf_hue;
        const Rect all{0, 0, W, H};
        // linear-gradient(160deg, hsl(sh,55,22), hsl(sh+4,50,7))
        list.linear_gradient(all, 160.0f, hsl(sh, 55, 22), hsl(sh + 4, 50, 7));
        // radial-gradient(ellipse 75% 65% at 28% 22%, hsl(sh,72,46,.62), transparent 70%)
        list.radial_gradient(all, W * .28f, H * .22f, W * .75f, H * .65f, hsl(sh, 72, 46, .62f),
                             transparent(hsl(sh, 72, 46)), 0.0f, 0.70f);
        // radial-gradient(ellipse 60% 60% at 82% 72%, hsl(sh+10,70,34,.5), transparent 70%)
        list.radial_gradient(all, W * .82f, H * .72f, W * .60f, H * .60f, hsl(sh + 10, 70, 34, .5f),
                             transparent(hsl(sh + 10, 70, 34)), 0.0f, 0.70f);
        // radial-gradient(ellipse 40% 40% at 55% 55%, hsl(g.hue,60,42,.14), transparent 70%)
        list.radial_gradient(all, W * .55f, H * .55f, W * .40f, H * .40f, hsl(b.game_hue, 60, 42, .14f),
                             transparent(hsl(b.game_hue, 60, 42)), 0.0f, 0.70f);
        list.pop_opacity();
    };
    const float t = static_cast<float>(std::clamp((now - bg_now_.since) / 0.8, 0.0, 1.0)); // opacity .8s ease
    const float e = t * t * (3.0f - 2.0f * t);
    wash(bg_prev_, 1.0f);
    wash(bg_now_, e);
    draw_spectrum_fx(list, now);
    // .vignette: radial-gradient(ellipse 80% 70% at 50% 42%, transparent 35%, rgba(0,0,0,.5) 100%),
    //            linear-gradient(to bottom, rgba(0,0,0,.25), transparent 20%, transparent 62%, rgba(0,0,0,.5))
    const Color black = rgb(0x000000);
    list.radial_gradient({0, 0, W, H}, W * .5f, H * .42f, W * .8f, H * .7f, black.with_alpha(0), black.with_alpha(.5f),
                         0.35f, 1.0f);
    list.gradient_rect({0, 0, W, H * .2f}, 0, black.with_alpha(.25f), black.with_alpha(0));
    list.gradient_rect({0, H * .62f, W, H * .38f}, 0, black.with_alpha(0), black.with_alpha(.5f));
}

// themefx.js spectrum(): three slow ribbons of light and drifting dust in the console's colour, over the wash and
// under the vignette. Canvas 'lighter' compositing becomes plain blending here (the alphas are small).
void ShelfScreen::draw_spectrum_fx(DrawList &list, double now)
{
    const float target = bg_now_.shelf_hue;
    const float dt = fx_last_ < 0.0 ? 0.0f : static_cast<float>(std::clamp(now - fx_last_, 0.0, 0.25));
    fx_last_ = now;
    if (fx_hue_ < 0.0f)
        fx_hue_ = target;
    fx_hue_ += (target - fx_hue_) * std::min(1.0f, dt * 1.5f);
    const float hue = fx_hue_, t = static_cast<float>(std::fmod(now, 10000.0));
    constexpr int kStep = 8; // the path takes a point every 8 px
    for (int k = 0; k < 3; ++k)
    {
        const float yb = H * (.2f + static_cast<float>(k) * .14f), hh = hue + static_cast<float>(k - 1) * 28.0f;
        std::vector<float> ys;
        for (int x = 0; x <= static_cast<int>(W); x += kStep)
        {
            const float u = static_cast<float>(x) / W;
            ys.push_back(yb + std::sin(u * 5.0f + t * (.16f + static_cast<float>(k) * .05f) + static_cast<float>(k) * 2.0f) * H * .06f +
                         std::sin(u * 11.0f - t * .21f + static_cast<float>(k)) * H * .02f);
        }
        // the two wide strokes as bands (top edge then bottom edge), the hairline as segments
        auto band = [&](float width, Color c)
        {
            std::vector<float> xy;
            xy.reserve(ys.size() * 4);
            for (std::size_t i = 0; i < ys.size(); ++i)
                xy.insert(xy.end(), {static_cast<float>(i * kStep), ys[i] - width * .5f});
            for (std::size_t i = ys.size(); i-- > 0;)
                xy.insert(xy.end(), {static_cast<float>(i * kStep), ys[i] + width * .5f});
            list.polygon(xy.data(), static_cast<int>(xy.size() / 2), c);
        };
        band(H * .09f, hsl(hh, 90, 62, .06f));
        band(H * .012f, hsl(hh, 95, 75, .12f));
        const Color hair = hsl(hh + 10, 100, 88, .18f);
        const float lw = std::max(1.0f, H * .0025f);
        for (std::size_t i = 1; i < ys.size(); ++i)
            list.line(static_cast<float>((i - 1) * kStep), ys[i - 1], static_cast<float>(i * kStep), ys[i], lw, hair);
    }
    // dust: 80 motes rising at .004-.016 screens a second, twinkling; a mote that leaves the top comes back at the
    // bottom somewhere else (seeded per lap, so every frame agrees without keeping state)
    auto rnd = [](std::uint32_t a)
    {
        a += 0x6D2B79F5u;
        std::uint32_t r = a;
        r = (r ^ (r >> 15)) * (r | 1u);
        r ^= r + (r ^ (r >> 7)) * (r | 61u);
        return static_cast<float>(r ^ (r >> 14)) / 4294967296.0f;
    };
    for (std::uint32_t i = 0; i < 80; ++i)
    {
        const float y0 = rnd(i * 7 + 1), sz = .4f + rnd(i * 7 + 2) * 1.2f, v = .004f + rnd(i * 7 + 3) * .012f,
                    p = rnd(i * 7 + 4) * 6.28f;
        const float risen = (1.02f - y0) + v * t; // as if it had started at the bottom (1.02); a lap is 1.04 screens
        const float lap = std::floor(risen / 1.04f);
        const float y = 1.02f - (risen - lap * 1.04f);
        const float x = lap == 0.0f ? rnd(i * 7 + 5) : rnd(i * 7 + 5 + static_cast<std::uint32_t>(lap) * 131u);
        const float a = .25f + .25f * std::sin(t * 1.3f + p);
        list.circle(x * W + std::sin(t * .4f + p) * 6.0f, y * H, sz * 2.2f, hsl(hue + 20, 90, 80, a * .7f));
    }
}

void ShelfScreen::draw_floor_and_halo(DrawList &list, double now)
{
    if (model_.empty())
        return;
    const float bh = case_height();
    // landSel(): glow = hsl(halo(shelfHue), 95%, 62%, .62); Spectrum's halo is the shelf hue itself.
    const Color glow = hsl(bg_now_.shelf_hue, 95, 62, .62f);
    // .floor (setup.css): left 50%, top 42% + bh/2 - 10, width min(1400px,140vw), height 160;
    // radial(ellipse 18% 26% at 50% 16%, glow, transparent 75%), radial(ellipse 55% 34% at 50% 26%, rgba(0,0,0,.55), transparent 72%)
    const float fw = std::min(1400.0f, W * 1.4f), fh = 160.0f, fx = W * .5f - fw * .5f, fy = kTrackY + bh * .5f - 10.0f;
    const Rect floor{fx, fy, fw, fh};
    list.radial_gradient(floor, fx + fw * .5f, fy + fh * .26f, fw * .55f, fh * .34f, rgb(0, .55f), rgb(0, 0), 0, .72f);
    list.radial_gradient(floor, fx + fw * .5f, fy + fh * .16f, fw * .18f, fh * .26f, glow, transparent(glow), 0, .75f);
    // .sel-halo: centred on the track, bw*2.6 x bh*1.9 (setup.css; its blur filter is off there),
    // radial-gradient(closest-side, glow, transparent), "breathe" 3.4 s ease-in-out: opacity 1 -> .55 -> 1.
    const auto size = data::case_size(data::style(model_.cur().system), W, H);
    const float hw = size.w * 2.6f, hh = size.h * 1.9f;
    const float phase = static_cast<float>(std::fmod(now, 3.4) / 3.4);
    const float wave = 0.5f - 0.5f * std::cos(phase * 2.0f * kPi); // 0 -> 1 -> 0
    const float opacity = 1.0f - 0.45f * wave;
    list.radial_gradient({kTrackX - hw * .5f, kTrackY - hh * .5f, hw, hh}, kTrackX, kTrackY, hw * .5f, hh * .5f,
                         glow.with_alpha(glow.a * opacity), transparent(glow), 0.0f, 1.0f);
}

// ---- cases ------------------------------------------------------------------------------------------------------

void ShelfScreen::art_arrived(const std::string &game_id, const std::string &path)
{
    if (game_id == "|info")
    {
        // new info from LaunchBox: every copy of each game takes it (the shelves keep their order)
        std::map<std::string, data::Game::Info> by_id;
        for (auto &[id, in] : read_info())
            by_id[id] = std::move(in);
        auto apply = [&](data::Game &g)
        {
            if (const auto it = by_id.find(g.id); it != by_id.end())
                g.info = it->second;
        };
        library_.for_each_game(apply);
        model_.for_each_game(apply);
        return;
    }
    const auto bar = game_id.rfind("|back");
    if (bar != std::string::npos && bar + 5 == game_id.size())
    {
        // a back: the case shows it from its next turn
        library_.set_back(game_id.substr(0, bar), path);
        if (model_.set_back(game_id.substr(0, bar), path))
        {
            const auto it = art_.find(game_id);
            if (it != art_.end())
            {
                stale_.push_back(std::move(it->second));
                art_.erase(it);
            }
            art_missing_.erase(std::remove(art_missing_.begin(), art_missing_.end(), game_id), art_missing_.end());
            loader_.forget(game_id);
            wish_.clear();
        }
        return;
    }
    library_.set_cover(game_id, path);
    if (model_.set_cover(game_id, path))
    {
        // the old picture (or the placeholder) goes; the next frame asks for the new one
        const auto it = art_.find(game_id);
        if (it != art_.end())
        {
            stale_.push_back(std::move(it->second));
            art_.erase(it);
        }
        art_missing_.erase(std::remove(art_missing_.begin(), art_missing_.end(), game_id), art_missing_.end());
        loader_.forget(game_id);
        wish_.clear();
    }
}

void ShelfScreen::update_covers(GlCanvas &canvas)
{
    for (auto &old : stale_)
        canvas.invalidate(old->pixels.data());
    stale_.clear();
    // Finished decodes come in; failed ones are remembered so the placeholder stays.
    for (auto &c : loader_.take())
    {
        if (const Item *now = model_.find(c->key); now && now->game.cover_front != c->path)
        {
            loader_.forget(c->key); // a newer cover arrived while this one was decoding
            continue;
        }
        if (c->failed || c->pixels.empty())
        {
            art_missing_.push_back(c->key);
            continue;
        }
        art_new_.push_back(c->key);
        art_[c->key] = std::move(c);
    }
    // The wish list: real covers of the cases in view, nearest the selection first.
    const auto &shelf = model_.cur();
    const int sel = model_.selected(), n = static_cast<int>(shelf.items.size());
    std::vector<CoverRequest> wish;
    std::string sig;
    for (int d = 0; d <= kWindow - 2; ++d)
        for (int i : {sel - d, sel + d})
        {
            if (i < 0 || i >= n || (d == 0 && i != sel) || (d > 0 && i == sel))
                continue;
            const Item &it = shelf.items[static_cast<std::size_t>(i)];
            const auto &g = it.game;
            if (it.kind != ItemKind::game || g.cover_front.empty() || art_.count(g.id) ||
                std::find(art_missing_.begin(), art_missing_.end(), g.id) != art_missing_.end())
                continue;
            const auto spec = data::cover_spec(data::style(it.system), 360);
            wish.push_back({g.id, g.cover_front, it.system, g.cover_square, spec.width, spec.height, g.cover_fallback,
                            g.fallback_square});
            sig += g.id;
            if (d == 0)
                break;
        }
    // The selected game's own back (emulator caches keep one for PS2, GameCube, Wii), once it is being flipped.
    if (flipped_ && sel >= 0 && sel < n)
    {
        const Item &it = shelf.items[static_cast<std::size_t>(sel)];
        const auto &g = it.game;
        const std::string key = g.id + "|back";
        if (it.kind == ItemKind::game && !g.cover_back.empty() && !art_.count(key) &&
            std::find(art_missing_.begin(), art_missing_.end(), key) == art_missing_.end())
        {
            const auto spec = data::cover_spec(data::style(it.system), 360);
            wish.push_back({key, g.cover_back, it.system, false, spec.width, spec.height, std::string(), false});
            sig += key;
        }
    }
    covers_pending_ = static_cast<int>(wish.size());
    if (sig != wish_)
    {
        wish_ = sig;
        loader_.want(std::move(wish));
    }
    // Memory: keep the covers nearest the selection.
    if (art_.size() > kMaxCovers)
    {
        std::vector<std::pair<int, std::string>> far;
        for (const auto &[key, px] : art_)
        {
            int dist = 1 << 20;
            for (int i = 0; i < n; ++i)
                if (shelf.items[static_cast<std::size_t>(i)].game.id == key)
                    dist = std::abs(i - sel);
            far.emplace_back(dist, key);
        }
        std::sort(far.begin(), far.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
        for (std::size_t k = 0; art_.size() > kMaxCovers && k < far.size(); ++k)
        {
            canvas.invalidate(art_[far[k].second]->pixels.data());
            art_.erase(far[k].second);
            loader_.forget(far[k].second);
        }
    }
}

std::uint32_t ShelfScreen::cover_texture(GlCanvas &canvas, const Item &item)
{
    if (item.kind == ItemKind::game && !item.game.id.empty())
    {
        const auto it = art_.find(item.game.id);
        if (it != art_.end())
        {
            auto &c = *it->second;
            const auto fresh = std::find(art_new_.begin(), art_new_.end(), item.game.id);
            if (fresh != art_new_.end())
            {
                // a new buffer may sit where a dropped one was: make the canvas upload it again
                canvas.invalidate(c.pixels.data());
                art_new_.erase(fresh);
            }
            return canvas.texture_for(c.pixels.data(), static_cast<unsigned>(c.width), static_cast<unsigned>(c.height));
        }
    }
    // Placeholder front until the cover pipeline lands (P3): the style's band over a wash in the game's hue, at the
    // system's own proportions (covers.js drawFront geometry). Kept per item; the canvas caches the texture.
    const auto &st = data::style(item.system);
    const std::string key = item.game.id.empty() ? std::string(st.key) + "|" + item.title : item.game.id;
    auto it = covers_.find(key);
    if (it == covers_.end())
    {
        const auto spec = data::cover_spec(st, 140);
        int ix, iy, iw, ih;
        frame_inner(item.system, spec.width, spec.height, ix, iy, iw, ih);
        std::vector<std::uint32_t> inner(static_cast<std::size_t>(iw * ih));
        const Color top = hsl(static_cast<float>(item.hue), 55, 46), bottom = hsl(static_cast<float>(item.hue) + 40, 60, 16);
        const Color band = rgb(st.band.bg);
        const int band_h = st.has_band ? static_cast<int>(std::lround(ih * 0.085)) : 0;
        for (int y = 0; y < ih; ++y)
        {
            const float t = static_cast<float>(y) / static_cast<float>(ih);
            Color c = hui::gfx::mix(top, bottom, t);
            if (y < band_h)
                c = band;
            const auto r = static_cast<std::uint32_t>(c.r * 255), g = static_cast<std::uint32_t>(c.g * 255),
                       b = static_cast<std::uint32_t>(c.b * 255);
            for (int x = 0; x < iw; ++x)
                inner[static_cast<std::size_t>(y * iw + x)] = 0xff000000u | (b << 16) | (g << 8) | r;
        }
        std::vector<std::uint32_t> px(static_cast<std::size_t>(spec.width * spec.height));
        frame_cover(item.system, inner.data(), iw, ih, px.data(), spec.width, spec.height);
        cover_size_[key] = spec.width;
        it = covers_.emplace(key, std::move(px)).first;
    }
    const int w = cover_size_[key];
    const int h = static_cast<int>(it->second.size()) / w;
    return canvas.texture_for(it->second.data(), static_cast<unsigned>(w), static_cast<unsigned>(h));
}

namespace
{
// covers.js wrapLines(): greedy word wrap at max_w.
std::vector<std::string> wrap_lines(Face face, const std::string &s, float size, float max_w)
{
    std::vector<std::string> lines;
    std::string line, word;
    auto flush_word = [&]
    {
        if (word.empty())
            return;
        const std::string attempt = line.empty() ? word : line + " " + word;
        if (!line.empty() && measure(face, attempt, size) > max_w)
        {
            lines.push_back(line);
            line = word;
        }
        else
            line = attempt;
        word.clear();
    };
    for (char c : s)
    {
        if (c == ' ')
            flush_word();
        else
            word += c;
    }
    flush_word();
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

float text_at_baseline(DrawList &list, Face face, std::string_view s, float x, float base, float size, Color color)
{
    const UiFont f = app::face(face);
    return f ? list.text(*f.font, f.texture, s, x, base, size, color, Align::left) : 0.0f;
}

// covers.js rng(): mulberry32
struct Rng
{
    std::uint32_t a;
    float operator()()
    {
        a += 0x6D2B79F5u;
        std::uint32_t t = a;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return static_cast<float>(t ^ (t >> 14)) / 4294967296.0f;
    }
};
} // namespace

void ShelfScreen::draw_made_up_back(DrawList &list, const Item &item, const Rect &r)
{
    const auto &st = data::style(item.system);
    // .f-back{background:var(--case); padding:var(--edge); padding-right:var(--edge-in)}
    list.rounded_rect(r, 0, rgb(st.case_color));
    const float e = std::round(r.w * st.edge);
    const Rect c{r.x + e, r.y + e, r.w - e - (st.wrap ? 0.0f : e), r.h - 2.0f * e};
    list.push_clip(c);
    const float W = c.w, Hh = c.h, k = W / 420.0f; // drawBack() draws on a 420-wide canvas
    const float hue = static_cast<float>(item.hue);
    list.gradient_rect(c, 0, hsl(hue + 200, 40, 16), hsl(hue + 230, 45, 7));
    // band(): 8.5 % tall, the label at 5 %, 700 bandH*.42, middle
    float band_h = 0.0f;
    if (st.has_band)
    {
        band_h = std::round(Hh * .085f);
        list.rounded_rect({c.x, c.y, W, band_h}, 0, rgb(st.band.bg));
        const float fs = band_h * .42f;
        const UiFont f = app::face(Face::lexend800);
        const float base = f ? c.y + band_h * .5f + (f.font->ascent(fs) - f.font->descent(fs)) * .5f : c.y + band_h * .6f;
        text_at_baseline(list, Face::lexend800, st.band.label, c.x + W * .05f, base, fs, rgb(st.band.fg));
    }
    // the title: 22px in one of the cover fonts (Lilita One is not carried: Lexend 800 stands in), two lines at most
    static constexpr Face kTitleFaces[3] = {Face::lexend800, Face::bungee400, Face::audiowide400};
    const Face tf = kTitleFaces[item.seed % 3];
    const auto title = wrap_lines(tf, upper(item.title), 22.0f * k, W * .88f);
    for (std::size_t i = 0; i < title.size() && i < 2; ++i)
        text_at_baseline(list, tf, title[i], c.x + W * .06f, c.y + band_h + (40.0f + 26.0f * static_cast<float>(i)) * k,
                         22.0f * k, rgb(0xffffff));
    // four "screenshots": drawScene() reduced to its sky, moon and hills (the other scene kinds are not ported)
    Rng rnd{item.seed ^ 0x9e3779b9u};
    const float shot_w = W * .42f, shot_h = shot_w * .6f, top = band_h + 90.0f * k;
    const float pos[4][2] = {{W * .06f, top}, {W * .52f, top}, {W * .06f, top + shot_h + 10 * k}, {W * .52f, top + shot_h + 10 * k}};
    for (int i = 0; i < 4; ++i)
    {
        const float sx = c.x + pos[i][0], sy = c.y + pos[i][1], H = hue + 35.0f * static_cast<float>(i);
        const Rect shot{sx, sy, shot_w, shot_h};
        list.push_clip(shot);
        list.gradient_rect(shot, 0, hsl(H, 75, 62), hsl(H + 40, 70, 32));
        list.circle(sx + shot_w * (.2f + rnd() * .6f), sy + shot_h * .3f, shot_w * .12f, hsl(H + 60, 95, 80).with_alpha(.9f));
        for (int layer = 0; layer < 3; ++layer)
        {
            const float base = sy + shot_h * (.62f + .12f * static_cast<float>(layer));
            std::vector<float> xy{sx, sy + shot_h, sx, base};
            float px = sx;
            while (px < sx + shot_w)
            {
                const float pw = shot_w * (.12f + rnd() * .18f);
                xy.insert(xy.end(), {px + pw * .5f, base - shot_h * (.08f + rnd() * .18f), px + pw, base});
                px += pw;
            }
            xy.insert(xy.end(), {sx + shot_w, sy + shot_h});
            list.polygon(xy.data(), static_cast<int>(xy.size() / 2),
                         hsl(H + 90 + 18 * static_cast<float>(layer), 55, 48 - 8 * static_cast<float>(layer)));
        }
        list.pop_clip();
        // strokeRect, 2px rgba(255,255,255,.7)
        const Color edge{1, 1, 1, .7f};
        const float t = 2.0f * k;
        list.rounded_rect({sx - t * .5f, sy - t * .5f, shot_w + t, t}, 0, edge);
        list.rounded_rect({sx - t * .5f, sy + shot_h - t * .5f, shot_w + t, t}, 0, edge);
        list.rounded_rect({sx - t * .5f, sy + t * .5f, t, shot_h - t}, 0, edge);
        list.rounded_rect({sx + shot_w - t * .5f, sy + t * .5f, t, shot_h - t}, 0, edge);
    }
    // the blurb (6 lines) and the info rows (app.js infoRows(g).slice(0, 5)), 13px Lexend: key at 6 % in white .55,
    // value at 34 %
    std::vector<std::pair<std::string, std::string>> rows = info_rows(item);
    if (rows.size() > 5)
        rows.resize(5);
    float y = top + shot_h * 2.0f + 44.0f * k;
    const float fs = 13.0f * k;
    if (!item.game.info.overview.empty())
    {
        std::string flat = item.game.info.overview;
        for (char &ch : flat)
            if (ch == '\n')
                ch = ' ';
        auto blurb = wrap_lines(Face::lexend500, flat, fs, W * .88f);
        if (blurb.size() > 6)
            blurb.resize(6), blurb.back() += "\xe2\x80\xa6";
        for (const auto &l : blurb)
        {
            text_at_baseline(list, Face::lexend500, l, c.x + W * .06f, c.y + y, fs, Color{1, 1, 1, .9f});
            y += 17.0f * k;
        }
        y += 10.0f * k;
    }
    for (const auto &[key, value] : rows)
    {
        if (y > Hh - 40.0f * k)
            break;
        text_at_baseline(list, Face::lexend500, key, c.x + W * .06f, c.y + y, fs, Color{1, 1, 1, .55f});
        text_at_baseline(list, Face::lexend500, value, c.x + W * .34f, c.y + y, fs, rgb(0xffffff));
        y += 22.0f * k;
    }
    list.pop_clip();
}

void ShelfScreen::draw_cases(GlCanvas &canvas, double now)
{
    if (model_.empty())
        return;
    const auto &shelf = model_.cur();
    const auto &st = data::style(shelf.system);
    const auto size = data::case_size(st, W, H);
    const float bw = size.w, bh = size.h, bd = size.d;
    const int sel = model_.selected();
    // #app.qm-open #scene{transform:translateX(-17vw) ...}: the cases and the camera move together
    const float dx = -.17f * W * aside(now);
    canvas.set_camera(kPerspective, kTrackX + dx, kTrackY);
    update_covers(canvas);

    // Back to front: the farthest cases first, the selected one last.
    std::vector<int> order;
    for (int i = 0; i < static_cast<int>(slots_.size()); ++i)
        if (std::abs(i - sel) <= kWindow)
            order.push_back(i);
    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return std::abs(a - sel) > std::abs(b - sel); });

    // The sides: plain plastic where no spine shows (top, bottom), ~20 % under the case colour.
    const Color side0 = rgb(st.case_side);
    const Color side{side0.r * .8f, side0.g * .8f, side0.b * .8f, 1.0f};

    std::vector<int> drawn;
    for (int i : order)
        if (slots_[static_cast<std::size_t>(i)].fade.at(now) > 0.01f)
            drawn.push_back(i);

    // ---- spines (style.css .f-left / setup.css .f-right.spine2): one atlas row per case, band tiles at the right
    // A row is R pixels across the spine's depth and L along its height, so the row keeps the face's proportions.
    constexpr float R = 48.0f, kBandX = 1096.0f;
    const float AW = static_cast<float>(GlCanvas::spine_width), AH = static_cast<float>(GlCanvas::spine_height);
    const float L = std::min(1088.0f, R * bh / std::max(bd, 1.0f));
    const float band_h = std::round(L * .09f); // .spine-band height: 9%
    const float pad = 8.0f * R / std::max(bd, 1.0f); // .spine-title padding: 8px
    // Each case's own look. On Home every game keeps its console's box (app.js: --bw = --ch * its ratio): the shelf's
    // case height, its own width, depth, colours and spine band.
    struct Look
    {
        const data::CaseStyle *st;
        float w, h, d, L, band_h, pad;
    };
    auto look = [&](int i) -> Look
    {
        const Item &it = shelf.items[static_cast<std::size_t>(i)];
        if (shelf.system != data::System::home || it.system == data::System::count)
            return {&st, bw, bh, bd, L, band_h, pad};
        const auto &s = data::style(it.system);
        const float w = bh * s.ratio, d = bh * s.ratio * s.depth;
        const float l = std::min(1088.0f, R * bh / std::max(d, 1.0f));
        return {&s, w, bh, d, l, std::round(l * .09f), 8.0f * R / std::max(d, 1.0f)};
    };
    struct BandTile
    {
        std::string label;
        std::uint32_t bg, fg;
        float y, h;
    };
    std::vector<BandTile> tiles;
    std::vector<float> rows(slots_.size(), -1.0f);
    std::vector<float> tile_of(slots_.size(), 0.0f);
    const UiFont title_face = app::face(Face::lexend600);
    canvas.begin_spine();
    {
        DrawList &sl = canvas.list();
        float next_row = 0.0f;
        for (int i : drawn)
        {
            if (next_row + R > AH)
                break;
            const Item &item = shelf.items[static_cast<std::size_t>(i)];
            const Look lk = look(i);
            const auto &cst = *lk.st;
            const float L = lk.L, band_h = lk.band_h, pad = lk.pad;
            const Color plastic = rgb(cst.case_color), ink = rgb(cst.spine_ink);
            const Color band_bg = rgb(cst.spine_band.bg), band_fg = rgb(cst.spine_band.fg);
            const float y0 = next_row;
            next_row += R;
            rows[static_cast<std::size_t>(i)] = y0;
            sl.push_clip({0, y0, L, R});
            sl.rounded_rect({0, y0, L, R}, 0, plastic);
            // .spine-title: 600, bd*.46, --spine-ink, nowrap, ellipsis, max-height 88 %, top to bottom after the band
            const float size = R * .46f, room = L * .88f - pad * 2.0f;
            std::string t = item.title;
            if (title_face && measure(Face::lexend600, t, size) > room)
            {
                const std::string dots = "\xe2\x80\xa6";
                while (!t.empty() && measure(Face::lexend600, t + dots, size) > room)
                {
                    t.pop_back();
                    while (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xc0) == 0x80)
                        t.pop_back();
                }
                while (!t.empty() && t.back() == ' ')
                    t.pop_back();
                t += dots;
            }
            text(sl, Face::lexend600, t, band_h + pad, y0, size, ink, Align::left, 0.0f, R);
            sl.pop_clip();
            // .spine-band label: the system's ("GCN"), "EMU" on an emulator tile, "ADD" on the setup tile
            const std::string label = item.kind == ItemKind::emulator ? "EMU"
                                      : item.kind == ItemKind::setup  ? "ADD"
                                                                      : std::string(cst.spine_band.label);
            auto found = std::find_if(tiles.begin(), tiles.end(), [&](const BandTile &b)
                                      { return b.label == label && b.bg == cst.spine_band.bg && b.fg == cst.spine_band.fg && b.h == band_h; });
            if (found == tiles.end())
            {
                const float ty = tiles.empty() ? 0.0f : tiles.back().y + tiles.back().h + 4.0f;
                if (ty + band_h <= AH)
                {
                    tiles.push_back({label, cst.spine_band.bg, cst.spine_band.fg, ty, band_h});
                    found = tiles.end() - 1;
                    // display grid, place-items center; 700 (Lexend 800 is the nearest baked weight), .02em
                    sl.push_clip({kBandX, ty, R, band_h});
                    sl.rounded_rect({kBandX, ty, R, band_h}, 0, band_bg);
                    const float ls = R * .3f;
                    text(sl, Face::lexend800, label, kBandX + R * .5f, ty, ls, band_fg, Align::center, .02f, band_h);
                    sl.pop_clip();
                }
            }
            tile_of[static_cast<std::size_t>(i)] = found == tiles.end() ? -1.0f : found->y;
        }
    }
    canvas.end_spine();

    // .sheen::after: 42 % of the face wide, from left -70 %, translateX(488 %) over .85 s cubic-bezier(.3,.6,.3,1),
    // 380 ms after the selection lands.
    static const Bezier sheen_ease{.3f, .6f, .3f, 1.0f};
    const float since = static_cast<float>(now - landed_at_) - .38f;
    const float sheen = since >= 0.0f && since <= .85f ? -.49f + 2.05f * sheen_ease(since / .85f) : -9.0f;

    // The selected case's back, once it turns: its own art, or covers.js drawBack() rendered into the back target.
    const float turn = flip_.at(now);
    std::uint32_t back_tex = 0;
    bool back_drawn = false;
    float back_uv[4] = {0, 0, 1, 1};
    if (sel >= 0 && sel < static_cast<int>(shelf.items.size()) && std::fabs(turn) > .5f)
    {
        const Item &item = shelf.items[static_cast<std::size_t>(sel)];
        const auto real = art_.find(item.game.id + "|back");
        if (real != art_.end())
        {
            auto &c = *real->second;
            const auto fresh = std::find(art_new_.begin(), art_new_.end(), real->first);
            if (fresh != art_new_.end())
            {
                canvas.invalidate(c.pixels.data());
                art_new_.erase(fresh);
            }
            back_tex = canvas.texture_for(c.pixels.data(), static_cast<unsigned>(c.width), static_cast<unsigned>(c.height));
        }
        else if (canvas.back_texture())
        {
            // fit the case's proportions inside the 640x960 target; the face shows only that part
            const Look lk = look(sel);
            const float tw = static_cast<float>(GlCanvas::back_width), th = static_cast<float>(GlCanvas::back_height);
            float w = tw, h = tw * lk.h / lk.w;
            if (h > th)
                h = th, w = th * lk.w / lk.h;
            canvas.begin_back();
            draw_made_up_back(canvas.list(), item, {0, 0, w, h});
            canvas.end_back();
            back_tex = canvas.back_texture();
            back_drawn = true;
            back_uv[0] = 0.0f, back_uv[1] = 1.0f - h / th, back_uv[2] = w / tw, back_uv[3] = 1.0f;
        }
    }

    for (int i : drawn)
    {
        const auto &slot = slots_[static_cast<std::size_t>(i)];
        float alpha = slot.fade.at(now);
        const Item &item = shelf.items[static_cast<std::size_t>(i)];
        const float x = slot.x.at(now), z = slot.z.at(now), ry = slot.ry.at(now);
        const bool is_sel = i == sel;
        const Look lk = look(i);
        const float L = lk.L, band_h = lk.band_h;
        CaseOp op;
        op.x = kTrackX + dx + x;
        op.y = kTrackY;
        op.z = z;
        op.width = lk.w;
        op.height = lk.h;
        op.depth = lk.d;
        op.favorite = item.kind == ItemKind::game && settings_.is_favorite(item.game.id);
        op.yaw = (ry + (is_sel ? turn : 0.0f)) * kPi / 180.0f;
        // Inspect: the case leaves the shelf for your hands (inspect3d.js takes over from the box)
        const float held = isp_in_.at(now);
        if (is_sel && held > 0.001f)
            alpha *= std::max(0.0f, 1.0f - held * 3.0f);
        op.front = cover_texture(canvas, item);
        if (is_sel && back_tex)
        {
            op.back = back_tex;
            op.flip_back = back_drawn;
            for (int c = 0; c < 4; ++c)
                op.back_uv[c] = back_uv[c];
        }
        const Color own_side = rgb(lk.st->case_side);
        if (lk.st == &st)
            op.side_r = side.r, op.side_g = side.g, op.side_b = side.b;
        else
            op.side_r = own_side.r * .8f, op.side_g = own_side.g * .8f, op.side_b = own_side.b * .8f;
        op.alpha = alpha;
        op.shade = is_sel ? 1.0f : 0.66f; // .slot:not(.sel) .dim{opacity:.34}
        // The front is rounded only away from the spine you see; cases right of the selection (.rside) mirror it.
        op.corner = lk.st->rc;
        op.corner_side = i > sel ? -1.0f : 1.0f;
        // .f-front::after film and .glare: full on the selected case, opacity .55 on the others
        op.film = is_sel ? 1.0f : .55f;
        op.glare = is_sel ? 1.0f : .55f;
        // .glare: a layer 1.2 faces wide (left -10 %), translateX((-ry/110 - x/(bw*7)) * 83.25 %)
        op.glare_pos = .5f + (-ry / 110.0f - x / (bw * 7.0f)) * 83.25f * .012f;
        op.sheen_pos = is_sel && !flipped_ ? sheen : -9.0f;
        const float y0 = rows[static_cast<std::size_t>(i)];
        if (y0 >= 0.0f && canvas.spine_texture())
        {
            op.spine = canvas.spine_texture();
            op.spine_both = true;
            op.spine_uv[0] = 0.0f, op.spine_uv[1] = 1.0f - (y0 + R) / AH;
            op.spine_uv[2] = L / AW, op.spine_uv[3] = 1.0f - y0 / AH;
            const float ty = tile_of[static_cast<std::size_t>(i)];
            if (ty >= 0.0f)
            {
                op.spine_band[0] = kBandX / AW, op.spine_band[1] = 1.0f - ty / AH;
                op.spine_band[2] = (kBandX + R) / AW, op.spine_band[3] = 1.0f - (ty + band_h) / AH;
                op.spine_band_h = .09f;
            }
            // setup.css .f-left::after (and .f-right.spine2::after, the same from the back edge); brighter when selected
            const float shade[4] = {.35f, .06f, .25f, .45f}, shade_sel[4] = {.15f, .14f, .2f, .45f};
            for (int c = 0; c < 4; ++c)
                op.spine_shade[c] = is_sel ? shade_sel[c] : shade[c];
        }
        // Floor: the selected case's real reflection (setup.css box-reflect: below 3px, .3 fading over 42 %); none while
        // the case is in your hands (Inspect)
        if (is_sel && held < .02f)
        {
            CaseOp r = op;
            r.reflection = true;
            r.floor_y = kTrackY + bh * .5f + 1.5f;
            r.fade_length = bh * .42f;
            canvas.case3d(r);
        }
        canvas.case3d(op);
    }
    // ---- Inspect: the shelf dims (.inspect.gl::before: rgba(3,4,8,.72)) and the case is drawn in its own pass
    const float held = isp_in_.at(now);
    if (held > 0.001f && sel >= 0 && sel < static_cast<int>(shelf.items.size()))
    {
        const Item &item = shelf.items[static_cast<std::size_t>(sel)];
        const Look lk = look(sel);
        const auto &cst = *lk.st;
        canvas.list().rounded_rect({0, 0, W, H}, 0, Color{3 / 255.f, 4 / 255.f, 8 / 255.f, .72f * held});
        InspectOp iop;
        iop.height = 517.0f; // inspect3d.js: a case 1 unit tall, 3.9 away, 30 degree field: 48 % of the screen
        iop.ratio = cst.ratio;
        iop.depth = cst.depth;
        iop.corner = cst.rc;
        static const std::pair<data::System, float> sizes[] = {
            {data::System::gc, .62f},  {data::System::wii, .86f},  {data::System::wiiu, .84f}, {data::System::psp, .66f},
            {data::System::ps3, .78f}, {data::System::ps4, .78f},  {data::System::ps5, .78f},  {data::System::x360, .8f},
            {data::System::ps2, .8f},  {data::System::sw, .5f},    {data::System::n3ds, .5f},  {data::System::psx, .8f}};
        iop.media_size = .78f;
        for (const auto &[sys, size] : sizes)
            if (sys == cst.id)
                iop.media_size = size;
        iop.disc_lift = cst.id == data::System::gc ? .1f : 0.0f;
        iop.edge = cst.edge + .01f;
        iop.cart = cst.id == data::System::sw || cst.id == data::System::sw2 || cst.id == data::System::n3ds;
        iop.front = cover_texture(canvas, item);
        const auto real = art_.find(item.game.id + "|back");
        if (real != art_.end())
            iop.back = canvas.texture_for(real->second->pixels.data(), static_cast<unsigned>(real->second->width),
                                          static_cast<unsigned>(real->second->height));
        const Color cc = rgb(cst.case_color), cs = rgb(cst.case_side);
        iop.case_r = cc.r, iop.case_g = cc.g, iop.case_b = cc.b;
        iop.side_r = cs.r, iop.side_g = cs.g, iop.side_b = cs.b;
        // the inside: inspect3d.js lifts black plastic toward #2c2d33 so the moulding shows
        Color in = cs;
        if (in.r * .3f + in.g * .59f + in.b * .11f < .12f)
            in = hui::gfx::mix(in, rgb(0x2c2d33), .75f);
        iop.inner_r = in.r, iop.inner_g = in.g, iop.inner_b = in.b;
        const float y0 = rows[static_cast<std::size_t>(sel)];
        if (y0 >= 0.0f && canvas.spine_texture())
        {
            iop.spine = canvas.spine_texture();
            iop.spine_uv[0] = 0.0f, iop.spine_uv[1] = 1.0f - (y0 + R) / AH;
            iop.spine_uv[2] = lk.L / AW, iop.spine_uv[3] = 1.0f - y0 / AH;
            const float ty = tile_of[static_cast<std::size_t>(sel)];
            if (ty >= 0.0f)
            {
                iop.spine_band[0] = kBandX / AW, iop.spine_band[1] = 1.0f - ty / AH;
                iop.spine_band[2] = (kBandX + R) / AW, iop.spine_band[3] = 1.0f - (ty + lk.band_h) / AH;
                iop.spine_band_h = .09f;
            }
            const float shade[4] = {.15f, .14f, .2f, .45f};
            for (int c = 0; c < 4; ++c)
                iop.spine_shade[c] = shade[c];
        }
        iop.rx = isp_pitch_, iop.ry = isp_yaw_;
        iop.lid = isp_lid_.at(now);
        iop.out = isp_out_.at(now);
        iop.spin = isp_spin_;
        iop.zoom = isp_zoom_;
        // .isp-stage entrance: scale(.84) translateY(24px), opacity 0 -> none
        iop.scale = .84f + .16f * held;
        iop.offset_y = 24.0f * (1.0f - held);
        canvas.inspect3d(iop);
        if (const std::uint32_t tex = canvas.inspect_texture())
            canvas.list().image(tex, {0, 0, W, H}, hui::gfx::kCanvasUv, Color{1, 1, 1, held});
    }
}

// ---- top bar ----------------------------------------------------------------------------------------------------

void ShelfScreen::draw_top_bar(DrawList &list, double now, const Clock &clock)
{
    // .hud-top: padding 26px 32px; brand left, tabs centre, clock / counter / Menu right.
    const float top = 26.0f, left = 32.0f, right = W - 32.0f;
    if (!model_.empty())
    {
        const auto sys = model_.cur().system;
        const Wordmark wm = wordmark(sys);
        const auto &st = data::style(sys);
        // .wm-main: line-height 1, gradient text, drop-shadow(0 2px 16px glow)
        const std::string word = wm.upper ? upper(tr(st.word.data())) : std::string(tr(st.word.data()));
        const float ww = measure(wm.face, word, wm.size, wm.spacing);
        list.shadow({left, top + 2, ww, wm.size}, wm.size * .3f, 16.0f, wm.glow.with_alpha(wm.glow.a * .6f));
        const std::size_t first = list.mark();
        text(list, wm.face, word, left, top, wm.size, kInk, Align::left, wm.spacing, wm.size);
        list.recolor(first, top, top + wm.size, wm.top, wm.bottom);
        // .wm-tag: 600 9px, letter-spacing .38em, uppercase; gap 7px
        float y = top + wm.size + 7.0f;
        text(list, Face::lexend600, upper(tr(st.word_sub.data())), left, y, 9.0f, wm.tag, Align::left, .38f, 9.0f);
        // .sys-count: margin-top 8px; 11px 500, .24em, uppercase, --ink-dim
        y += 9.0f + 8.0f;
        char count[48];
        const int n = model_.cur().game_count;
        std::snprintf(count, sizeof count, "%d %s", n, n == 1 ? tr("GAME") : tr("GAMES"));
        text(list, Face::lexend500, count, left, y, 11.0f, kInkDim, Align::left, .24f);
    }

    // .tabs: glass capsule, padding 4, gap 4; .tab 6px 14px, 12px 500; the active one on a #ebebf0 pill.
    if (model_.shelf_count() > 0)
    {
        const float pad = 4.0f, gap = 4.0f, tab_h = 12.0f * 1.25f + 12.0f;
        std::vector<float> widths;
        float total = pad * 2;
        for (int i = 0; i < model_.shelf_count(); ++i)
        {
            widths.push_back(measure(Face::lexend500, tab_name(model_.shelf(i).system), 12.0f) + 28.0f);
            total += widths.back() + (i ? gap : 0.0f);
        }
        const float x0 = W * .5f - total * .5f, h = tab_h + pad * 2 + 2; // 1px border each side
        // setup.css: background color-mix(in srgb, --sel-glow 30%, rgba(8,10,20,.4)),
        // border-color color-mix(in srgb, --sel-glow 45%, rgba(255,255,255,.1)); no blur.
        const Color glow = hsl(bg_now_.shelf_hue, 95, 62, .62f);
        list.bordered_rect({x0, top, total, h}, h * .5f, color_mix(glow, .3f, Color{8 / 255.f, 10 / 255.f, 20 / 255.f, .4f}),
                           1.0f, color_mix(glow, .45f, Color{1, 1, 1, .1f}));
        float x = x0 + pad;
        float sel_x = x, sel_w = widths.empty() ? 0 : widths[0];
        for (int i = 0; i < model_.shelf_count(); ++i)
        {
            if (i == model_.current())
                sel_x = x, sel_w = widths[static_cast<std::size_t>(i)];
            x += widths[static_cast<std::size_t>(i)] + gap;
        }
        pill_x_.duration = pill_w_.duration = .45f;
        pill_x_.ease = pill_w_.ease = kPill;
        pill_x_.set(sel_x, now);
        pill_w_.set(sel_w, now);
        list.rounded_rect({pill_x_.at(now), top + pad + 1, pill_w_.at(now), tab_h}, tab_h * .5f, rgb(0xebebf0));
        x = x0 + pad;
        for (int i = 0; i < model_.shelf_count(); ++i)
        {
            const float w = widths[static_cast<std::size_t>(i)];
            const bool on = i == model_.current();
            text(list, Face::lexend500, tab_name(model_.shelf(i).system), x + w * .5f, top + pad + 1 + 6, 12.0f,
                 on ? rgb(0x10121a) : kInkDim, Align::center, 0.0f, 15.0f);
            x += w + gap;
        }
    }

    // .right: column, items right-aligned, gap 10px. Clock row: date (13px 500 .08em dim) + time (b: 700 15px ink).
    // (the quick menu's panel covers this corner: it fades away as the panel comes in)
    list.push_opacity(1.0f - aside(now));
    {
        // The row is as tall as its 34px icon buttons (sound, controller), as measured on the reference capture.
        float y = top;
        const float row = 36.0f;
        const float tw = measure(Face::lexend800, clock.time, 15.0f, .08f);
        text(list, Face::lexend800, clock.time, right, y + (row - 21.0f) * .5f, 15.0f, kInk, Align::right, .08f, 21.0f);
        const float dw = measure(Face::lexend500, clock.date, 13.0f, .08f);
        text(list, Face::lexend500, clock.date, right - tw - 8.0f, y + (row - 18.0f) * .5f + 1.0f, 13.0f, kInkDim,
             Align::right, .08f, 18.0f);
        // controller (lit while a pad is connected) and sound icons, 12px apart (.clock-row gap)
        {
            const float cx = right - tw - 8.0f - dw - 12.0f - 12.0f, cy = y + row * .5f;
            list.bordered_rect({cx - 11.0f, cy - 6.0f, 22.0f, 12.0f}, 6.0f, Color{0, 0, 0, 0}, 1.4f, kInkDim);
            list.circle(cx - 5.0f, cy, 1.4f, kInkDim);
            list.circle(cx + 5.0f, cy, 1.4f, kInkDim);
            // (Boxshelf's sound button comes back with the sounds: the app has no audio yet)
        }
        y += row + 10.0f;
        // .counter: 14px 500 tabular, .06em; "<b>sel+1</b> / n"
        if (!model_.empty() && !model_.cur().items.empty())
        {
            char tail[24], head[16];
            std::snprintf(head, sizeof head, "%d", model_.selected() + 1);
            std::snprintf(tail, sizeof tail, " / %d", static_cast<int>(model_.cur().items.size()));
            const float wt = measure(Face::lexend500, tail, 14.0f, .06f);
            text(list, Face::lexend500, tail, right, y, 14.0f, kInk, Align::right, .06f, 18.0f);
            text(list, Face::lexend800, head, right - wt, y, 14.0f, kInk, Align::right, .06f, 18.0f);
        }
        y += 18.0f + 10.0f;
        // .mini Menu: 11px dim, padding 4px 8px, radius 999, 1px line border; a hamburger glyph then "Menu"
        const float mw = measure(Face::lexend500, tr("Menu"), 11.0f) + 14.0f + 6.0f + 16.0f, mh = 22.0f;
        list.bordered_rect({right - mw, y, mw, mh}, mh * .5f, Color{0, 0, 0, 0}, 1.0f, kLine);
        const float gx = right - mw + 8.0f + 1.0f, gy = y + mh * .5f;
        for (float dy : {-3.5f, 0.0f, 3.5f})
            list.line(gx, gy + dy, gx + 11.0f, gy + dy, 1.4f, kInkDim);
        text(list, Face::lexend500, tr("Menu"), right - 8.0f, y + 4.0f, 11.0f, kInkDim, Align::right, 0.0f, 14.0f);
    }
    list.pop_opacity();
}

// ---- title strip ------------------------------------------------------------------------------------------------

void ShelfScreen::draw_info(DrawList &list, double now)
{
    // .info: top calc(42% + bh/2 + 4.5vh), centred. .g-title 44px 500 (clamp(22px,3vw,44px)), text-shadow;
    // .g-meta: margin-top 6px, 14px dim, .04em, spans joined by a "·" with .6em each side at .7 opacity.
    const float bh = case_height();
    float y = kTrackY + bh * .5f + H * .08f; // setup.css: .info{top:calc(42% + var(--bh)/2 + 8vh)}
    // .info.swap: rise 450 ms cubic-bezier(.22,.9,.25,1) from translateY(8px), opacity 0
    const float t = kSlide(static_cast<float>(std::clamp((now - info_since_) / 0.45, 0.0, 1.0)));
    list.push_opacity(t);
    y += 8.0f * (1.0f - t);
    const Item *it = model_.item();
    const std::string title = it ? it->title : tr("No games here yet");
    list.shadow({W * .5f - measure(Face::lexend500, title, 44.0f) * .5f, y + 10, measure(Face::lexend500, title, 44.0f), 34},
                12, 20, Color{0, 0, 0, .4f});
    text(list, Face::lexend500, title, W * .5f, y, 44.0f, kInk, Align::center, -.005f);
    y += app::face(Face::lexend500).font->line_height(44.0f) + 6.0f;
    if (it)
    {
        const auto parts = meta_line(*it);
        const float dot_gap = 14.0f * .6f;
        float total = 0;
        for (std::size_t k = 0; k < parts.size(); ++k)
            total += measure(Face::lexend500, parts[k], 14.0f, .04f) + (k ? dot_gap * 2 + measure(Face::lexend500, "\xc2\xb7", 14.0f) : 0);
        float x = W * .5f - total * .5f;
        for (std::size_t k = 0; k < parts.size(); ++k)
        {
            if (k)
            {
                x += dot_gap;
                x += text(list, Face::lexend500, "\xc2\xb7", x, y, 14.0f, kInkDim.with_alpha(kInkDim.a * .7f));
                x += dot_gap;
            }
            x += text(list, Face::lexend500, parts[k], x, y, 14.0f, kInkDim, Align::left, .04f);
        }
    }
    list.pop_opacity();
}

// ---- hints ------------------------------------------------------------------------------------------------------

void ShelfScreen::draw_hints(DrawList &list)
{
    // PAD_HINTS with PlayStation glyphs (app.js glyph(): SHAPES for the face buttons, keycaps for the rest).
    struct Hint
    {
        const char *keys[2];
        const char *label;
        const char *hold;
    };
    const Item *it = model_.item();
    const char *play = !it || it->kind == ItemKind::setup ? tr("Set up") : tr("Play");
    // Only what works today (owner, 2026-11-10): Settings / Museum come back with their feature.
    std::vector<Hint> hints;
    if (isp_on_) // app.js inspect hints: Turn, Zoom, then the next step, Back
        hints = {{{"R", nullptr}, tr("Turn"), nullptr},
                 {{"L2", "R2"}, tr("Zoom"), nullptr},
                 {{"x", nullptr}, isp_state_ == 0 ? tr("Open case") : isp_state_ == 1 ? tr("Take disc out") : tr("Play"), nullptr},
                 {{"o", nullptr}, isp_state_ == 0 ? tr("Put back") : isp_state_ == 1 ? tr("Close case") : tr("Put disc back"), nullptr}};
    else
        hints = {{{"L2", "R2"}, tr("System"), nullptr},     {{"tri", nullptr}, tr("Flip"), "\xe2\x99\xa5"},
                 {{"sq", nullptr}, tr("Details"), nullptr},  {{"x", nullptr}, play, nullptr},
                 {{"R3", nullptr}, tr("Inspect"), nullptr},  {{"L3", nullptr}, tr("Shuffle"), nullptr},
                 {{"menu", nullptr}, tr("Menu"), tr("Sort")}};
    const float font = 12.0f, key_h = 18.0f, hint_h = 28.0f, pad_x = 10.0f, gap_in = 7.0f, gap = 6.0f;
    auto key_width = [&](const char *k)
    {
        const std::string_view s(k);
        if (s == "tri" || s == "sq" || s == "x" || s == "o")
            return 19.0f;
        if (s == "share" || s == "menu")
            return 18.0f;
        return std::max(22.0f, measure(Face::lexend600, k, 10.0f) + 10.0f + 2.0f);
    };
    auto hold_width = [&](const char *h)
    {
        if (!h)
            return 0.0f;
        const float label = std::string_view(h) == "\xe2\x99\xa5" ? 12.0f : measure(Face::lexend500, h, font);
        return 4.0f + measure(Face::lexend600, tr("HOLD"), 9.5f, .06f) + 10.0f + 4.0f + label;
    };
    std::vector<float> widths;
    float total = 8.0f * 2;
    for (const auto &h : hints)
    {
        float w = pad_x * 2;
        for (const char *k : h.keys)
            if (k)
                w += key_width(k) + 3.0f;
        w -= 3.0f;
        w += gap_in + measure(Face::lexend500, h.label, font);
        w += hold_width(h.hold);
        widths.push_back(w);
        total += w + gap;
    }
    total -= gap;
    const float bar_h = hint_h + 12.0f + 2.0f;
    const float x0 = W * .5f - total * .5f, y0 = H - 24.0f - bar_h;
    // setup.css: .hints{backdrop-filter:none; background:rgba(10,12,22,.78)}
    list.bordered_rect({x0, y0, total, bar_h}, bar_h * .5f, Color{10 / 255.f, 12 / 255.f, 22 / 255.f, .78f}, 1.0f, kLine);
    float x = x0 + 8.0f + 1.0f;
    const float cy = y0 + bar_h * .5f;
    for (std::size_t n = 0; n < hints.size(); ++n)
    {
        const auto &h = hints[n];
        float hx = x + pad_x;
        for (const char *k : h.keys)
        {
            if (!k)
                continue;
            const std::string_view s(k);
            const float kw = key_width(k);
            if (s == "tri") // --tri:#3fd0b6
                list.triangle({hx + 3.5f, cy - 6.0f, 12.0f, 11.0f}, rgb(0x3fd0b6), 1.6f);
            else if (s == "sq") // --sq:#ee8fd6
                list.bordered_rect({hx + 4.0f, cy - 5.5f, 11.0f, 11.0f}, 1.0f, Color{0, 0, 0, 0}, 1.6f, rgb(0xee8fd6));
            else if (s == "x") // --cross:#86a8ff
            {
                list.line(hx + 4.5f, cy - 5.0f, hx + 14.5f, cy + 5.0f, 1.8f, rgb(0x86a8ff));
                list.line(hx + 14.5f, cy - 5.0f, hx + 4.5f, cy + 5.0f, 1.8f, rgb(0x86a8ff));
            }
            else if (s == "o") // --circ:#ff7a7a
                list.ring(hx + 9.5f, cy, 6.0f, 1.6f, rgb(0xff7a7a));
            else if (s == "menu") // three bars
            {
                for (float dy : {-4.0f, 0.0f, 4.0f})
                    list.line(hx + 3.0f, cy + dy, hx + 15.0f, cy + dy, 1.6f, kInk);
            }
            else if (s == "share") // two overlapping rounded squares (the Create button's mark)
            {
                list.bordered_rect({hx + 2.0f, cy - 6.0f, 10.0f, 10.0f}, 2.0f, Color{0, 0, 0, 0}, 1.4f, kInk);
                list.bordered_rect({hx + 6.0f, cy - 2.0f, 10.0f, 10.0f}, 2.0f, Color{0, 0, 0, 0}, 1.4f, kInk);
            }
            else // .g keycap: 1px rgba(255,255,255,.55) border, radius 5, 10px 600
            {
                list.bordered_rect({hx, cy - key_h * .5f, kw, key_h}, 5.0f, Color{0, 0, 0, 0}, 1.0f, Color{1, 1, 1, .55f});
                text(list, Face::lexend600, k, hx + kw * .5f, cy - key_h * .5f + 1.0f, 10.0f, kInk, Align::center, 0, 16.0f);
            }
            hx += kw + 3.0f;
        }
        hx += gap_in - 3.0f;
        hx += text(list, Face::lexend500, h.label, hx, cy - 8.0f, font, kInk, Align::left, 0, 16.0f);
        if (h.hold)
        {
            // .hint .hold: margin-left -1px, gap 4px, opacity .82; <i>hold</i>: 9.5px 600 .06em upper, pill rgba(255,255,255,.1)
            hx += 4.0f;
            const float iw = measure(Face::lexend600, tr("HOLD"), 9.5f, .06f) + 10.0f;
            list.rounded_rect({hx, cy - 7.5f, iw, 15.0f}, 4.0f, Color{1, 1, 1, .1f * .82f});
            text(list, Face::lexend600, tr("HOLD"), hx + 5.0f, cy - 7.0f, 9.5f, Color{1, 1, 1, .7f * .82f}, Align::left, .06f, 14.0f);
            hx += iw + 4.0f;
            if (std::string_view(h.hold) == "\xe2\x99\xa5")
            {
                // "♥": no baked face has it (Boxshelf gets it from the system font): two lobes and a point
                const Color heart = kInk.with_alpha(.82f);
                list.circle(hx + 3.2f, cy - 1.6f, 3.2f, heart);
                list.circle(hx + 8.8f, cy - 1.6f, 3.2f, heart);
                list.triangle({hx + .2f, cy - 1.0f, 11.6f, 6.6f}, heart, 0.0f, 3.14159265f);
            }
            else
                text(list, Face::lexend500, h.hold, hx, cy - 8.0f, font, kInk.with_alpha(.82f), Align::left, 0, 16.0f);
        }
        x += widths[n] + gap;
    }
}

void ShelfScreen::draw_letter_flash(DrawList &list, double now)
{
    // .letter-flash: centred on the track, min(22vh,180px) 700; 900 ms ease-out: 0 -> .95 (at 15 %, scale .8 -> 1)
    // -> 0 (scale 1.08)
    const float t = static_cast<float>((now - flash_at_) / 0.9);
    if (t < 0.0f || t >= 1.0f || flash_.empty())
        return;
    const float e = 1.0f - (1.0f - t) * (1.0f - t); // ease-out
    float opacity, scale;
    if (e < 0.15f)
        opacity = .95f * e / .15f, scale = .8f + .2f * e / .15f;
    else
        opacity = .95f * (1.0f - (e - .15f) / .85f), scale = 1.0f + .08f * (e - .15f) / .85f;
    const float size = std::min(H * .22f, 180.0f) * scale;
    list.push_opacity(opacity);
    text(list, Face::lexend800, flash_, W * .5f, kTrackY - size * .5f, size, kInk, Align::center, 0, size);
    list.pop_opacity();
}

void ShelfScreen::draw_empty(DrawList &list)
{
    // index.html #empty: "Nothing on the shelf yet" / "Connect your emulators and game folders" (+ Run setup, which
    // the PS5 build has no setup screen for yet). While the library is read, the kicker says what is being read.
    const bool loading = !status_.empty();
    const float cy = kTrackY;
    if (!loading && settings_.filter != "all" && !library_.empty())
    {
        // the filter left no game: the shelves come back with Menu → Filter → All games
        text(list, Face::lexend600, tr("NOTHING MATCHES THE FILTER"), W * .5f, cy - 52.0f, 11.0f, kInkDim, Align::center,
             .24f, 14.0f);
        text(list, Face::lexend600, tr("No games here yet"), W * .5f, cy - 28.0f, 34.0f, kInk, Align::center, 0.0f, 42.0f);
        text(list, Face::lexend500, tr("Menu > Filter > All games shows them again."), W * .5f,
             cy + 24.0f, 16.0f, kInkDim, Align::center, 0.0f, 22.0f);
        return;
    }
    text(list, Face::lexend600, loading ? upper(tr(status_.c_str())) : tr("NOTHING ON THE SHELF YET"), W * .5f,
         cy - 52.0f, 11.0f, kInkDim, Align::center, .24f, 14.0f);
    text(list, Face::lexend600, loading ? tr("Reading your library") : tr("Connect your emulators and game folders"),
         W * .5f, cy - 28.0f, 34.0f, kInk, Align::center, 0.0f, 42.0f);
    text(list, Face::lexend500,
         loading ? tr("Installed games, then the folders your emulators use")
                 : tr("Put your games in Emulation/roms/<system> on your drive and open Vitrine again."),
         W * .5f, cy + 24.0f, 16.0f, kInkDim, Align::center, 0.0f, 22.0f);
}

// ---- the quick menu -------------------------------------------------------------------------------------------

float ShelfScreen::aside(double now) const
{
    // the menu and Details move the shelf aside the same way (#app.details-open #scene, #app.qm-open #scene)
    return std::clamp(std::max(qm_.at(now), det_.at(now)), 0.0f, 1.0f);
}

void ShelfScreen::open_menu(double now)
{
    qm_open_ = true;
    qm_.set(1.0f, now);
    qm_row_ = 0, qm_sub_ = -1;
    qm_hl_.set(0.0f, now, true);
    set_flipped(false, now);
}

namespace
{
// The shelves a game can be put on (Move to shelf), in the shelves' order.
constexpr data::System kMoveTargets[] = {data::System::gc,   data::System::wii,  data::System::wiiu,
                                         data::System::sw,   data::System::ps2,  data::System::ps4,
                                         data::System::ps5,  data::System::n3ds, data::System::psx,
                                         data::System::nds,  data::System::n64,  data::System::custom,
                                         data::System::media};
constexpr Lang kLangs[] = {Lang::en, Lang::pt, Lang::es};
} // namespace

std::vector<ShelfScreen::MenuRow> ShelfScreen::menu_rows() const
{
    std::vector<MenuRow> rows;
    const Item *it = model_.item();
    if (it && it->kind == ItemKind::game)
    {
        MenuRow r{0, tr("Move to shelf"), tab_name(it->system), {}, 0};
        for (std::size_t i = 0; i < std::size(kMoveTargets); ++i)
        {
            r.chips.push_back(tab_name(kMoveTargets[i]));
            if (kMoveTargets[i] == it->system)
                r.current = static_cast<int>(i);
        }
        rows.push_back(std::move(r));
    }
    {
        MenuRow a{2, tr("Box art"), art_running_ ? trf("Downloading %s", art_status_) : std::string(tr("Up to date")),
                  {tr("Find missing"), tr("Download all again")}, -1};
        rows.push_back(std::move(a));
    }
    {
        MenuRow so{5, tr("Sort"), "", {}, 0}, fi{6, tr("Filter"), "", {}, 0};
        const auto sorts = sort_options(), filters = filter_options();
        for (std::size_t i = 0; i < sorts.size(); ++i)
        {
            so.chips.push_back(sorts[i].second);
            if (settings_.sort == sorts[i].first)
                so.current = static_cast<int>(i), so.value = sorts[i].second;
        }
        for (std::size_t i = 0; i < filters.size(); ++i)
        {
            fi.chips.push_back(filters[i].second);
            if (settings_.filter == filters[i].first)
                fi.current = static_cast<int>(i), fi.value = filters[i].second;
        }
        rows.push_back(std::move(so));
        rows.push_back(std::move(fi));
    }
    rows.push_back({3, tr("Box backs"), settings_.backs ? tr("Download") : tr("Off"), {tr("Download"), tr("Off")},
                    settings_.backs ? 0 : 1});
    MenuRow l{1, tr("Language"), language_name(language()), {}, 0};
    for (std::size_t i = 0; i < std::size(kLangs); ++i)
    {
        l.chips.push_back(language_name(kLangs[i]));
        if (kLangs[i] == language())
            l.current = static_cast<int>(i);
    }
    rows.push_back(std::move(l));
    return rows;
}

void ShelfScreen::menu_apply(const MenuRow &row, int chip, double now)
{
    if (row.kind == 5 || row.kind == 6)
    {
        const auto options = row.kind == 5 ? sort_options() : filter_options();
        if (chip < 0 || chip >= static_cast<int>(options.size()))
            return;
        std::string &v = row.kind == 5 ? settings_.sort : settings_.filter;
        const std::string pick = options[static_cast<std::size_t>(chip)].first;
        if (v == pick)
            return;
        v = pick;
        if (!save_settings(settings_))
            toast(tr("Could not save the settings"), false, false, now);
        arrange(now);
        return;
    }
    if (row.kind == 3)
    {
        const bool on = chip == 0;
        if (settings_.backs == on)
            return;
        settings_.backs = on;
        if (!save_settings(settings_))
            toast(tr("Could not save the settings"), false, false, now);
        else if (on && !art_running_)
            art_request_ = 1, art_asked_ = true; // fetch the backs now
        return;
    }
    if (row.kind == 2)
    {
        if (art_running_)
            return toast(tr("Box art is already downloading"), false, false, now);
        art_request_ = chip == 0 ? 1 : 2;
        art_asked_ = true;
        toast(chip == 0 ? tr("Looking for the missing box art") : tr("Downloading all the box art again"), true, false, now);
        return;
    }
    if (row.kind == 1)
    {
        set_language(kLangs[chip]);
        settings_.language = std::string(language_code(kLangs[chip]));
        if (!save_settings(settings_))
            toast(tr("Could not save the settings"), false, false, now);
        build_slots(now); // tab pill widths change with the words
        return;
    }
    const Item *it = model_.item();
    if (!it || it->kind != ItemKind::game)
        return;
    const data::System to = kMoveTargets[chip];
    const std::string id = it->game.id;
    const std::string from = id.substr(0, id.find(':')); // the shelf it was found on
    if (!library_.move_item(id, to))
        return;
    arrange(now); // the shelves (and Home) again, from the library
    settings_.set_move(id, std::string(data::style(to).key) == from ? std::string() : std::string(data::style(to).key));
    if (!save_settings(settings_))
        toast(tr("Could not save the settings"), false, false, now);
    else
        toast(trf("Moved to %s", tab_name(to)), false, true, now);
    build_slots(now);
    on_moved(now);
}

void ShelfScreen::menu_input(const hui::InputFrame &in, double now)
{
    const auto rows = menu_rows();
    const int n = static_cast<int>(rows.size());
    auto close = [&]
    {
        qm_open_ = false;
        qm_sub_ = -1;
        qm_.set(0.0f, now);
    };
    if (in.is_pressed(hui::Action::menu))
        return close();
    if (qm_sub_ >= 0 && qm_sub_ < n)
    {
        const auto &row = rows[static_cast<std::size_t>(qm_sub_)];
        const int chips = static_cast<int>(row.chips.size());
        if (in.nav == hui::Direction::left)
            qm_chip_ = (qm_chip_ + chips - 1) % chips;
        else if (in.nav == hui::Direction::right)
            qm_chip_ = (qm_chip_ + 1) % chips;
        else if (in.is_pressed(hui::Action::confirm))
        {
            menu_apply(row, qm_chip_, now);
            qm_sub_ = -1;
            if (row.kind == 0)
                close();
        }
        else if (in.is_pressed(hui::Action::back) || in.nav == hui::Direction::up || in.nav == hui::Direction::down)
            qm_sub_ = -1;
        return;
    }
    if (in.is_pressed(hui::Action::back))
        return close();
    if (n == 0)
        return;
    qm_row_ = std::clamp(qm_row_, 0, n - 1);
    if (in.nav == hui::Direction::up && qm_row_ > 0)
        --qm_row_;
    else if (in.nav == hui::Direction::down && qm_row_ < n - 1)
        ++qm_row_;
    else if (in.is_pressed(hui::Action::confirm) || in.nav == hui::Direction::right)
    {
        qm_sub_ = qm_row_;
        qm_chip_ = std::max(0, rows[static_cast<std::size_t>(qm_row_)].current);
    }
    qm_hl_.set(static_cast<float>(qm_row_), now);
}

void ShelfScreen::draw_menu(GlCanvas &canvas, double now)
{
    const float t = qm_.at(now);
    if (t <= 0.001f && !qm_open_)
        return;
    DrawList &list = canvas.list();
    // .qm: width 460, from the right (translateX(104%) -> 0); rgba(8,10,18,.74) over a 26px blur (no blur pass here:
    // a little more opaque instead), 1px --line on the left; padding 34 / 28
    const float pw = 460.0f, px = W - pw * t + (1.0f - t) * pw * .04f;
    list.rounded_rect({px, 0, pw, H}, 0, Color{8 / 255.f, 10 / 255.f, 18 / 255.f, .95f});
    list.rounded_rect({px, 0, 1, H}, 0, kLine);
    const float inner = px + 28.0f;
    float y = 34.0f;
    // the game you're on: .qm-cov 50x68 r5, kicker 11px .22em, title 23px 600, meta 13px dim
    if (const Item *it = model_.item())
    {
        const std::uint32_t cover = cover_texture(canvas, *it);
        list.shadow({inner + 2, y + 6, 50, 68}, 5, 16, Color{0, 0, 0, .8f});
        if (cover)
            list.image(cover, {inner + 2, y, 50, 68}, hui::gfx::kFullUv, Color{1, 1, 1, 1}, 5.0f);
        const float tx = inner + 2 + 50 + 16, tw = px + pw - 28 - tx;
        text(list, Face::lexend500, upper(tab_name(it->system)), tx, y + 6, 11.0f, kInkDim, Align::left, .22f, 14.0f);
        std::string title = it->title;
        while (title.size() > 1 && measure(Face::lexend600, title, 23.0f) > tw)
        {
            title.pop_back();
            while (!title.empty() && (static_cast<unsigned char>(title.back()) & 0xc0) == 0x80)
                title.pop_back();
            if (measure(Face::lexend600, title + "\xe2\x80\xa6", 23.0f) <= tw)
            {
                title += "\xe2\x80\xa6";
                break;
            }
        }
        text(list, Face::lexend600, title, tx, y + 6 + 14 + 5, 23.0f, kInk, Align::left, 0.0f, 26.0f);
        std::string meta;
        for (const auto &m : meta_line(*it))
            meta += (meta.empty() ? "" : " \xc2\xb7 ") + m;
        text(list, Face::lexend500, meta, tx, y + 6 + 14 + 5 + 26 + 4, 13.0f, kInkDim, Align::left, 0.0f, 16.0f);
        y += 68.0f + 16.0f;
    }
    // the list: rows 50 tall, r 25, gap 2; the white pill (#ebebf0) glides to the row you're on
    const auto rows = menu_rows();
    const float rx = px + 28 - 14 + 10, rw = pw - 56 + 28 - 20;
    // chips wrap inside the row (left 50, right 8): how many lines a row's chips take
    auto chip_lines = [&](const MenuRow &r)
    {
        float cx = 0.0f;
        int lines = 1;
        for (const auto &c : r.chips)
        {
            const float w = measure(Face::lexend500, c, 13.0f) + 28.0f;
            if (cx + w > rw - 58.0f && cx > 0.0f)
                cx = 0.0f, ++lines;
            cx += w + 6.0f;
        }
        return lines;
    };
    y += 8.0f;
    std::vector<float> row_y;
    float yy = y;
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        row_y.push_back(yy);
        yy += 50.0f + 2.0f;
        if (static_cast<int>(i) == qm_sub_)
        {
            yy += 6.0f + 11.0f + 7.0f + 3.0f + static_cast<float>(chip_lines(rows[i])) * 36.0f + 12.0f;
        }
    }
    if (!rows.empty())
    {
        const float hl = std::clamp(qm_hl_.at(now), 0.0f, static_cast<float>(rows.size() - 1));
        const int lo = static_cast<int>(std::floor(hl)), hi = std::min(lo + 1, static_cast<int>(rows.size()) - 1);
        const float hy = row_y[static_cast<std::size_t>(lo)] +
                         (row_y[static_cast<std::size_t>(hi)] - row_y[static_cast<std::size_t>(lo)]) * (hl - static_cast<float>(lo));
        const bool soft = qm_sub_ >= 0;
        list.rounded_rect({rx, hy, rw, 50}, 25, soft ? Color{1, 1, 1, .07f} : rgb(0xebebf0));
    }
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const auto &r = rows[i];
        const float ry = row_y[i];
        const bool on = static_cast<int>(i) == qm_row_ && qm_sub_ < 0;
        const Color ink = on ? rgb(0x10121a) : kInk, dim = on ? Color{16 / 255.f, 18 / 255.f, 26 / 255.f, .62f} : kInkDim;
        text(list, Face::lexend500, r.label, rx + 18, ry + 16, 15.0f, ink, Align::left, 0.0f, 18.0f);
        // .qm-chev "›" 18px, turned 90° when open (drawn as a "v" then)
        const Color chev = on ? Color{16 / 255.f, 18 / 255.f, 26 / 255.f, .5f} : Color{243 / 255.f, 245 / 255.f, 251 / 255.f, .38f};
        const float cx = rx + rw - 18 - 5, cy = ry + 25;
        if (static_cast<int>(i) == qm_sub_)
        {
            list.line(cx - 4, cy - 2, cx, cy + 2, 1.6f, chev);
            list.line(cx, cy + 2, cx + 4, cy - 2, 1.6f, chev);
        }
        else
        {
            list.line(cx - 2, cy - 4, cx + 2, cy, 1.6f, chev);
            list.line(cx + 2, cy, cx - 2, cy + 4, 1.6f, chev);
        }
        text(list, Face::lexend500, r.value, cx - 12, ry + 17, 13.0f, dim, Align::right, 0.0f, 16.0f);
        if (static_cast<int>(i) != qm_sub_)
            continue;
        // .qm-subin: padding 6px 8px 12px 50px; .qm-ll 11px .16em uppercase; .qm-opts chips 7px 14px, gap 6
        float sx = rx + 50, sy = ry + 50 + 6;
        text(list, Face::lexend500, upper(r.label), sx + 2, sy, 11.0f, Color{243 / 255.f, 245 / 255.f, 251 / 255.f, .42f},
             Align::left, .16f, 14.0f);
        sy += 11.0f + 7.0f + 3.0f;
        float cxp = sx;
        for (std::size_t c = 0; c < r.chips.size(); ++c)
        {
            const float w = measure(Face::lexend500, r.chips[c], 13.0f) + 28.0f, h = 30.0f;
            if (cxp + w > rx + rw - 8 && cxp > sx)
                cxp = sx, sy += h + 6.0f;
            const bool focus = static_cast<int>(c) == qm_chip_, cur = static_cast<int>(c) == r.current;
            if (focus)
                list.rounded_rect({cxp, sy, w, h}, h * .5f, rgb(0xebebf0));
            else if (cur) // .qm-opt.cur: --ink, a --line border and a dot under the word
            {
                list.bordered_rect({cxp, sy, w, h}, h * .5f, Color{1, 1, 1, .06f}, 1.0f, kLine);
                list.circle(cxp + w * .5f, sy + h - 5.0f, 2.0f, Color{243 / 255.f, 245 / 255.f, 251 / 255.f, .8f});
            }
            else
                list.rounded_rect({cxp, sy, w, h}, h * .5f, Color{1, 1, 1, .06f});
            text(list, Face::lexend500, r.chips[c], cxp + 14, sy + 7, 13.0f, focus ? rgb(0x10121a) : cur ? kInk : kInkDim,
                 Align::left, 0.0f, 16.0f);
            cxp += w + 6.0f;
        }
    }
    // the panel's own hints at the bottom: Cross and Circle drawn as shapes, as in the hint bar (no such glyphs)
    const float hy = H - 34.0f - 8.0f;
    float hx = px + 28.0f;
    list.line(hx, hy - 5, hx + 10, hy + 5, 1.8f, rgb(0x86a8ff));
    list.line(hx + 10, hy - 5, hx, hy + 5, 1.8f, rgb(0x86a8ff));
    hx += 16.0f;
    hx += text(list, Face::lexend500, tr("Select"), hx, hy - 8, 12.0f, kInkDim, Align::left, 0.0f, 16.0f) + 22.0f;
    list.ring(hx + 6, hy, 6.0f, 1.6f, rgb(0xff7a7a));
    hx += 18.0f;
    text(list, Face::lexend500, qm_sub_ >= 0 ? tr("Back") : tr("Close"), hx, hy - 8, 12.0f, kInkDim, Align::left, 0.0f,
         16.0f);
}

// ---- Details (Square) ---------------------------------------------------------------------------------------

std::vector<std::pair<std::string, std::string>> ShelfScreen::info_facts(const data::Game &g) const
{
    // app.js infoRows(): Released, Developer, Publisher (when another), Genre, Players (co-op), Rating, Community score
    std::vector<std::pair<std::string, std::string>> rows;
    const auto &i = g.info;
    if (!i.year.empty())
        rows.push_back({tr("Released"), i.year});
    if (!i.developer.empty())
        rows.push_back({tr("Developer"), i.developer});
    if (!i.publisher.empty() && i.publisher != i.developer)
        rows.push_back({tr("Publisher"), i.publisher});
    if (!i.genres.empty())
    {
        std::string gs = i.genres;
        for (std::size_t at = gs.find(';'); at != std::string::npos; at = gs.find(';', at + 2))
            gs.replace(at, 1, ", ");
        rows.push_back({tr("Genre"), gs});
    }
    if (!i.players.empty())
        rows.push_back({tr("Players"), i.players + (i.coop ? std::string(" ") + tr("(co-op)") : std::string())});
    if (!i.esrb.empty())
        rows.push_back({tr("Rating"), i.esrb.substr(0, i.esrb.find(" - "))});
    if (!i.rating.empty())
    {
        char r[16];
        std::snprintf(r, sizeof r, "%.1f / 5", std::atof(i.rating.c_str()));
        rows.push_back({tr("Community score"), r});
    }
    return rows;
}

std::vector<std::pair<std::string, std::string>> ShelfScreen::info_rows(const Item &item) const
{
    // app.js infoRows(), with what the PS5 knows: the made-up back shows the same facts
    std::vector<std::pair<std::string, std::string>> rows;
    const auto &g = item.game;
    if (g.path.rfind("/appdb/", 0) == 0)
    {
        const std::string from = g.id.substr(0, g.id.find(':'));
        rows.push_back({tr("Type"), from == "media"    ? std::string(tr("Streaming app"))
                                    : from == "custom" ? std::string(tr("Homebrew"))
                                                       : trf("%s game", from == "ps4" ? "PS4" : "PS5")});
        rows.push_back({tr("Title ID"), g.app.empty() ? std::string(tr("Unknown")) : g.app});
        for (auto &r : info_facts(g))
            rows.push_back(std::move(r));
        return rows;
    }
    for (auto &r : info_facts(g))
        rows.push_back(std::move(r));
    rows.push_back({tr("Region"), g.region.empty() ? std::string(tr("Unknown")) : g.region});
    std::string file = tr("Folder");
    if (!g.folder)
    {
        const auto dot = g.path.find_last_of('.');
        file = dot == std::string::npos ? std::string(tr("File")) : upper(g.path.substr(dot + 1));
    }
    rows.push_back({tr("Format"), file});
    rows.push_back({tr("Emulator"), emulator_name(item.system)});
    if (!g.disc_id.empty())
        rows.push_back({tr("Serial"), g.disc_id});
    rows.push_back({tr("File"), g.path}); // wide
    return rows;
}

void ShelfScreen::inspect_input(const hui::InputFrame &in, double now, float dt)
{
    const bool moving = std::fabs(in.stick2_x) > .05f || std::fabs(in.stick2_y) > .05f || in.nav != hui::Direction::none;
    if (moving)
        isp_idle_at_ = now;
    if (isp_state_ == 2 && now - isp_idle_at_ > 1.5)
        isp_spin_ += dt * .22f * 3.0f;
    // right stick (or the D-pad) turns, L2 / R2 zoom; Cross: open the case, take the disc out, play; Circle: one step back
    isp_yaw_ += in.stick2_x * 160.0f * dt;
    isp_pitch_ = std::clamp(isp_pitch_ + in.stick2_y * 120.0f * dt, -45.0f, 45.0f);
    if (in.nav == hui::Direction::left)
        isp_yaw_ -= 30.0f;
    else if (in.nav == hui::Direction::right)
        isp_yaw_ += 30.0f;
    isp_zoom_ = std::clamp(isp_zoom_ + (in.trigger_r - in.trigger_l) * 1.2f * dt, .7f, 1.5f);
    if (in.is_pressed(hui::Action::confirm))
    {
        if (isp_state_ == 0)
            isp_state_ = 1, isp_lid_.set(1.0f, now); // 165 degrees, as inspect3d.js
        else if (isp_state_ == 1)
            isp_state_ = 2, isp_out_.set(1.0f, now);
        else
            play(now);
    }
    if (in.is_pressed(hui::Action::back) || in.is_pressed(hui::Action::r3))
    {
        if (isp_state_ == 2 && !in.is_pressed(hui::Action::r3))
            isp_state_ = 1, isp_out_.set(0.0f, now);
        else if (isp_state_ == 1 && !in.is_pressed(hui::Action::r3))
            isp_state_ = 0, isp_lid_.set(0.0f, now);
        else
        {
            isp_on_ = false, isp_state_ = 0;
            isp_in_.set(0.0f, now), isp_lid_.set(0.0f, now), isp_out_.set(0.0f, now);
        }
    }
}

void ShelfScreen::open_details(double now)
{
    const Item *it = model_.item();
    if (!it || it->kind != ItemKind::game)
        return;
    det_open_ = true;
    det_.set(1.0f, now);
    set_flipped(false, now);
}

void ShelfScreen::details_input(const hui::InputFrame &in, double now)
{
    // ← → another game with the panel open; ✕ Play; △ Favorite; ○ / □ close (app.js details mode)
    if (in.is_pressed(hui::Action::back) || in.is_pressed(hui::Action::west) || in.is_pressed(hui::Action::menu))
    {
        det_open_ = false;
        det_.set(0.0f, now);
        return;
    }
    if (in.nav == hui::Direction::left && model_.step(-1))
        on_moved(now);
    else if (in.nav == hui::Direction::right && model_.step(1))
        on_moved(now);
    if (in.is_pressed(hui::Action::confirm))
        play(now);
    if (in.is_pressed(hui::Action::north))
        toggle_favorite(now);
}

void ShelfScreen::draw_details(GlCanvas &canvas, double now)
{
    const float t = det_.at(now);
    if (t <= 0.001f && !det_open_)
        return;
    const Item *it = model_.item();
    if (!it || it->kind != ItemKind::game)
        return;
    DrawList &list = canvas.list();
    const auto &g = it->game;
    // .details: width 460 from the right, rgba(8,10,18,.74) over a blur (a little more opaque here), --line on the left
    const float pw = 460.0f, px = W - pw * t + (1.0f - t) * pw * .04f;
    list.rounded_rect({px, 0, pw, H}, 0, Color{8 / 255.f, 10 / 255.f, 18 / 255.f, .95f});
    list.rounded_rect({px, 0, 1, H}, 0, kLine);
    list.push_clip({px, 0, pw, H});

    // .d-hero: min-height 200, padding 34 28 22; the cover as a dark backdrop (brightness .55) under a gradient
    const std::uint32_t cover = cover_texture(canvas, *it);
    const float hero_h = 232.0f;
    {
        // .d-hero-bg is the cover blurred (28px) and darkened (.55): drawn here as a wash in the game's own hue (the
        // colour the shelf's backdrop takes from it), since there is no blur pass
        const float hue = static_cast<float>(it->hue);
        list.linear_gradient({px, 0, pw, hero_h}, 160.0f, hsl(hue, 55, 30), hsl(hue + 30, 50, 10));
        list.radial_gradient({px, 0, pw, hero_h}, px + pw * .3f, hero_h * .35f, pw * .7f, hero_h * .8f,
                             hsl(hue + 10, 70, 45, .55f), transparent(hsl(hue + 10, 70, 45)), 0.0f, 1.0f);
    }
    list.gradient_rect({px, 0, pw, hero_h}, 0, Color{8 / 255.f, 10 / 255.f, 18 / 255.f, .1f},
                       Color{9 / 255.f, 11 / 255.f, 19 / 255.f, .9f});
    list.rounded_rect({px, hero_h - 1, pw, 1}, 0, kLine);
    // .d-hero-row: the cover 96 wide (its own case's proportions), the head beside it, both on the bottom line
    const auto &st = data::style(it->system);
    const float cw = 96.0f, ch = std::min(150.0f, cw / st.ratio), cx = px + 28.0f, cy = hero_h - 22.0f - ch;
    list.shadow({cx, cy + 14, cw, ch}, 6, 30, Color{0, 0, 0, .6f});
    if (cover)
        list.image(cover, {cx, cy, cw, ch}, hui::gfx::kFullUv, Color{1, 1, 1, 1}, 6.0f);
    const float hx = cx + cw + 18.0f, hw = px + pw - 28.0f - hx;
    // .d-kicker: "GameCube · 3 of 12", 11px .22em uppercase dim; .d-title 30px 600, two lines at most
    int nth = 0, games_n = 0;
    for (std::size_t i = 0; i < model_.cur().items.size(); ++i)
        if (model_.cur().items[i].kind == ItemKind::game)
        {
            ++games_n;
            if (static_cast<int>(i) == model_.selected())
                nth = games_n;
        }
    const std::string kicker = std::string(tab_name(it->system)) + " \xc2\xb7 " +
                               trf("%s of", std::to_string(nth)) + " " + std::to_string(games_n);
    auto lines = wrap_lines(Face::lexend600, it->title, 30.0f, hw);
    if (lines.size() > 2)
    {
        lines.resize(2);
        while (!lines[1].empty() && measure(Face::lexend600, lines[1] + "\xe2\x80\xa6", 30.0f) > hw)
            lines[1].pop_back();
        lines[1] += "\xe2\x80\xa6";
    }
    const float title_h = 35.0f * static_cast<float>(lines.size());
    float ty = hero_h - 22.0f - 28.0f - title_h - 8.0f - 14.0f;
    text(list, Face::lexend500, upper(kicker), hx, ty, 11.0f, kInkDim, Align::left, .22f, 14.0f);
    ty += 14.0f + 8.0f;
    for (const auto &l : lines)
    {
        text(list, Face::lexend600, l, hx, ty, 30.0f, kInk, Align::left, 0.0f, 35.0f);
        ty += 35.0f;
    }
    // .d-chips: .chipx 11px 600, 4px 9px, rgba(255,255,255,.1), 1px rgba(255,255,255,.14)
    {
        // fillDetails(): year, first genre, players, ESRB (a white chip), the community score (gold), then ours
        struct Chip
        {
            std::string text;
            int kind; // 0 plain, 1 ESRB, 2 score
        };
        std::vector<Chip> chips;
        const auto &in = g.info;
        if (!in.year.empty())
            chips.push_back({in.year, 0});
        if (!in.genres.empty())
            chips.push_back({in.genres.substr(0, in.genres.find(';')), 0});
        if (!in.players.empty())
            chips.push_back({in.players == "1" ? std::string(tr("1 player")) : trf("%s players", in.players), 0});
        if (!in.esrb.empty())
            chips.push_back({in.esrb.substr(0, in.esrb.find(" - ")), 1});
        if (!in.rating.empty())
        {
            char r[24];
            std::snprintf(r, sizeof r, "\xe2\x98\x85 %.1f", std::atof(in.rating.c_str()));
            chips.push_back({r, 2});
        }
        const auto dot = g.path.find_last_of('.');
        if (chips.empty() && !g.folder && dot != std::string::npos && g.path.size() - dot <= 6 && g.path.rfind("/appdb/", 0) != 0)
            chips.push_back({upper(g.path.substr(dot + 1)), 0});
        if (settings_.is_favorite(g.id))
            chips.push_back({tr("Favorite"), 0});
        float chx = hx;
        const float chy = ty + 6.0f;
        for (const auto &c : chips)
        {
            const float w = measure(Face::lexend600, c.text, 11.0f, .04f) + 18.0f;
            if (chx + w > px + pw - 20.0f)
                break;
            if (c.kind == 1) // .chipx.esrb: white, dark text
                list.rounded_rect({chx, chy, w, 22}, 11, Color{1, 1, 1, .9f});
            else if (c.kind == 2) // .chipx.score
                list.bordered_rect({chx, chy, w, 22}, 11, Color{1, 207 / 255.f, 107 / 255.f, .16f}, 1.0f,
                                   Color{1, 207 / 255.f, 107 / 255.f, .35f});
            else
                list.bordered_rect({chx, chy, w, 22}, 11, Color{1, 1, 1, .1f}, 1.0f, Color{1, 1, 1, .14f});
            text(list, Face::lexend600, c.text, chx + 9, chy + 4, 11.0f,
                 c.kind == 1 ? rgb(0x111111) : c.kind == 2 ? rgb(0xffd98a) : kInk, Align::left, .04f, 14.0f);
            chx += w + 6.0f;
        }
    }

    // .d-stats: your history with the game, as tiles (Last played, Launches)
    float y = hero_h + 18.0f;
    {
        const auto *h = settings_.history_of(g.id);
        std::string last = tr("Never");
        if (h && h->count > 0)
        {
            const long long days = (static_cast<long long>(std::time(nullptr)) - h->last) / 86400;
            last = days <= 0 ? tr("Today") : days == 1 ? tr("Yesterday") : trf("%s days ago", std::to_string(days));
        }
        const std::pair<std::string, std::string> stats[] = {{tr("Last time"), last},
                                                             {tr("Launches"), std::to_string(h ? h->count : 0)}};
        const float sw = (pw - 56.0f - 8.0f) * .5f;
        for (int i = 0; i < 2; ++i)
        {
            const float sx = px + 28.0f + i * (sw + 8.0f);
            list.bordered_rect({sx, y, sw, 56}, 12, Color{1, 1, 1, .05f}, 1.0f, kLine);
            text(list, Face::lexend500, upper(stats[i].first), sx + 12, y + 10, 10.0f, kInkDim, Align::left, .14f, 13.0f);
            text(list, Face::lexend600, stats[i].second, sx + 12, y + 27, 16.0f, kInk, Align::left, 0.0f, 20.0f);
        }
        y += 56.0f + 22.0f;
    }

    // About this game (.d-sec, .d-desc.clamp): the overview, five lines
    if (!g.info.overview.empty())
    {
        text(list, Face::lexend500, upper(tr("About this game")), px + 28, y, 11.0f,
             Color{243 / 255.f, 245 / 255.f, 251 / 255.f, .42f}, Align::left, .16f, 14.0f);
        y += 14.0f + 10.0f;
        std::string flat = g.info.overview;
        for (char &ch : flat)
            if (ch == '\n')
                ch = ' ';
        auto desc = wrap_lines(Face::lexend500, flat, 13.0f, pw - 56.0f);
        if (desc.size() > 5)
            desc.resize(5), desc.back() += "\xe2\x80\xa6";
        for (const auto &l : desc)
        {
            text(list, Face::lexend500, l, px + 28, y, 13.0f, Color{243 / 255.f, 245 / 255.f, 251 / 255.f, .82f}, Align::left,
                 0.0f, 19.0f);
            y += 19.0f;
        }
        y += 16.0f;
    }
    // Game info (.d-sec, then the .d-grid boxes: two columns, the file path across both)
    text(list, Face::lexend500, upper(tr("Game info")), px + 28, y, 11.0f, Color{243 / 255.f, 245 / 255.f, 251 / 255.f, .42f},
         Align::left, .16f, 14.0f);
    y += 14.0f + 10.0f;
    {
        const float gw = (pw - 56.0f - 8.0f) * .5f;
        int col = 0;
        const std::string skip[] = {tr("Released"), tr("Genre"), tr("Players"), tr("Rating"), tr("Community score"),
                                    tr("Format")};
        for (const auto &[k, v] : info_rows(*it))
        {
            if (std::find(std::begin(skip), std::end(skip), k) != std::end(skip))
                continue;
            const bool wide = k == tr("File");
            if (wide && col == 1)
                col = 0, y += 52.0f + 8.0f;
            const float bx = px + 28.0f + (wide ? 0.0f : col * (gw + 8.0f)), bw = wide ? pw - 56.0f : gw;
            std::string value = v;
            const float room = bw - 24.0f;
            // a long path keeps its end (the file name)
            while (value.size() > 4 && measure(Face::lexend500, value, 13.0f) > room)
            {
                value.erase(0, 1);
                while (!value.empty() && (static_cast<unsigned char>(value[0]) & 0xc0) == 0x80)
                    value.erase(0, 1);
            }
            if (value != v)
                value = "\xe2\x80\xa6" + value;
            list.bordered_rect({bx, y, bw, 52}, 12, Color{1, 1, 1, .05f}, 1.0f, kLine);
            text(list, Face::lexend500, upper(k), bx + 12, y + 9, 10.0f, kInkDim, Align::left, .14f, 13.0f);
            text(list, Face::lexend500, value, bx + 12, y + 26, 13.0f, kInk, Align::left, 0.0f, 17.0f);
            if (wide || ++col == 2)
                col = 0, y += 52.0f + 8.0f;
        }
        if (col == 1)
            y += 52.0f + 8.0f;
    }

    // Your copy: which pictures are here (.d-art-chip, solid green when there)
    y += 10.0f;
    text(list, Face::lexend500, upper(tr("Your copy")), px + 28, y, 11.0f, Color{243 / 255.f, 245 / 255.f, 251 / 255.f, .42f},
         Align::left, .16f, 14.0f);
    y += 14.0f + 10.0f;
    {
        const bool front = !g.cover_front.empty() && !g.cover_square, back = !g.cover_back.empty();
        const std::pair<const char *, bool> parts[] = {{"Front cover", front}, {"Back cover", back}};
        float chx = px + 28.0f;
        for (const auto &[label, ok] : parts)
        {
            const std::string s = std::string(ok ? "\xe2\x9c\x93 " : "") + tr(label);
            const float w = measure(Face::lexend500, s, 12.0f) + 20.0f;
            if (ok)
                list.bordered_rect({chx, y, w, 24}, 12, Color{95 / 255.f, 224 / 255.f, 160 / 255.f, .08f}, 1.0f,
                                   Color{95 / 255.f, 224 / 255.f, 160 / 255.f, .45f});
            else
                list.bordered_rect({chx, y, w, 24}, 12, Color{0, 0, 0, 0}, 1.0f, Color{1, 1, 1, .22f});
            text(list, Face::lexend500, s, chx + 10, y + 4, 12.0f, ok ? rgb(0xbff3d8) : kInkDim, Align::left, 0.0f, 16.0f);
            chx += w + 6.0f;
        }
        y += 24.0f + 16.0f;
    }
    // .d-how: "Plays in PS5SX2. Close the emulator to come back here."
    if (g.path.rfind("/appdb/", 0) != 0)
        text(list, Face::lexend500, trf("Plays in %s. Close the emulator to come back here.", emulator_name(it->system)),
             px + 28, y, 12.0f, kInkDim, Align::left, 0.0f, 18.0f);

    // .d-actions (sticky at the bottom): Play across, Favorite under it, each with its button
    {
        const float ah = 16.0f + 40.0f + 8.0f + 40.0f + 22.0f, ay = H - ah;
        list.rounded_rect({px, ay, pw, ah}, 0, Color{10 / 255.f, 12 / 255.f, 20 / 255.f, .97f});
        list.rounded_rect({px, ay, pw, 1}, 0, kLine);
        const float bx = px + 28.0f, bw = pw - 56.0f;
        // .btn.primary: --ink background, #10121a text, 13px 600, round
        list.rounded_rect({bx, ay + 16, bw, 40}, 20, kInk);
        const std::string play = tr("Play");
        const float pwid = measure(Face::lexend600, play, 13.0f) + 20.0f;
        const float pxs = bx + (bw - pwid) * .5f, pcy = ay + 16 + 20;
        list.line(pxs, pcy - 5, pxs + 10, pcy + 5, 1.8f, rgb(0x10121a));
        list.line(pxs + 10, pcy - 5, pxs, pcy + 5, 1.8f, rgb(0x10121a));
        text(list, Face::lexend600, play, pxs + 20, ay + 16 + 11, 13.0f, rgb(0x10121a), Align::left, 0.0f, 18.0f);
        const bool fav = settings_.is_favorite(g.id);
        const float fy = ay + 16 + 40 + 8;
        list.bordered_rect({bx, fy, bw, 40}, 20, Color{0, 0, 0, 0}, 1.0f, kLine);
        const std::string fl = fav ? tr("Remove from favorites") : tr("Add to favorites");
        const float fwid = measure(Face::lexend600, fl, 13.0f) + 22.0f;
        const float fxs = bx + (bw - fwid) * .5f, fcy = fy + 20;
        list.triangle({fxs, fcy - 6.0f, 12.0f, 11.0f}, rgb(0x3fd0b6), 1.6f);
        text(list, Face::lexend600, fl, fxs + 22, fy + 11, 13.0f, kInk, Align::left, 0.0f, 18.0f);
    }
    list.pop_clip();
}

void ShelfScreen::draw(GlCanvas &canvas, double now, const Clock &clock)
{
    if (!model_.empty() && shown_shelf_ != model_.current())
        build_slots(now);
    draw_backdrop(canvas.list(), now);
    if (model_.empty())
    {
        draw_empty(canvas.list());
        draw_top_bar(canvas.list(), now, clock);
        return;
    }
    const float away = aside(now), dx = -.17f * W * away;
    canvas.list().push_transform(1.0f, 0.0f, 0.0f, dx, 0.0f);
    draw_floor_and_halo(canvas.list(), now);
    canvas.list().pop_transform();
    draw_cases(canvas, now);
    auto &hud = canvas.list();
    // .info: translateX(-17vw), opacity 0; the hint bar hands over to the panel
    hud.push_opacity((1.0f - away) * (1.0f - isp_in_.at(now))); // the case in your hands covers the title strip
    hud.push_transform(1.0f, 0.0f, 0.0f, dx, 0.0f);
    draw_info(hud, now);
    hud.pop_transform();
    hud.pop_opacity();
    draw_top_bar(hud, now, clock);
    draw_letter_flash(hud, now);
    if (!model_.empty() && away < 1.0f)
    {
        hud.push_opacity(1.0f - away);
        draw_hints(hud);
        hud.pop_opacity();
    }
    draw_details(canvas, now);
    draw_menu(canvas, now);
    draw_launch(canvas, now);
    draw_toast(hud, now);
}
} // namespace boxshelf::app
