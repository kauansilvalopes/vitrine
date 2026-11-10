// Boxshelf PS5 - the shelf screen. SPDX-License-Identifier: GPL-3.0-or-later
//
// Every number below comes from Boxshelf's renderer (style.css / setup.css / app.js); the comment next to it says
// which rule. CSS pixels are virtual pixels here (the canvas is 1920x1080, the size Boxshelf was measured at).
#include "app/scene.hpp"

#include "app/ui_fonts.hpp"
#include "data/scan.hpp"
#include "data/covers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

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
    std::string out(s);
    for (char &c : out)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
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
    case System::home: return "Home";
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
    case System::custom: return "Others";
    case System::media: return "Media";
    default: return "";
    }
}

// The game's meta line (app.js metaFor()).
std::vector<std::string> meta_for(const Item &it, data::System shelf)
{
    switch (it.kind)
    {
    case ItemKind::emulator:
        return {"Emulator", "Open settings"};
    case ItemKind::setup:
        return {std::string("No ") + tab_name(shelf) + " games yet",
                std::string("Cross opens ") + tab_name(shelf) + " settings to add a games folder"};
    case ItemKind::random:
        return {"Shuffle", "Lands on a random game"};
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
    model_ = std::move(model);
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
        return toast("Still starting the last game", false, false, now);
    const auto &g = it->game;
    LaunchRequest r;
    r.title = it->title;
    const auto sys = it->system;
    if (sys == data::System::ps5 || sys == data::System::ps4 || sys == data::System::custom || sys == data::System::media)
        r.app = g.app;
    else
    {
        r.app = std::string(data::emulator_app(sys));
        r.target = g.path;
        r.ps1_folder = sys == data::System::psx && g.folder;
        r.switch_folder = sys == data::System::sw && g.folder;
    }
    if (r.app.size() != 9)
        return toast(std::string("No emulator for ") + tab_name(sys) + " on this console", false, false, now);
    launcher_.start(std::move(r));
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
        toast(st.message, st.working, st.ok && !st.working, now);
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
    if ((in.released & hui::action_bit(hui::Action::north)) && north_down_ >= 0.0)
    {
        if (now - north_down_ < .5)
            set_flipped(!flipped_, now);
        north_down_ = -1.0;
    }
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
    // .vignette: radial-gradient(ellipse 80% 70% at 50% 42%, transparent 35%, rgba(0,0,0,.5) 100%),
    //            linear-gradient(to bottom, rgba(0,0,0,.25), transparent 20%, transparent 62%, rgba(0,0,0,.5))
    const Color black = rgb(0x000000);
    list.radial_gradient({0, 0, W, H}, W * .5f, H * .42f, W * .8f, H * .7f, black.with_alpha(0), black.with_alpha(.5f),
                         0.35f, 1.0f);
    list.gradient_rect({0, 0, W, H * .2f}, 0, black.with_alpha(.25f), black.with_alpha(0));
    list.gradient_rect({0, H * .62f, W, H * .38f}, 0, black.with_alpha(0), black.with_alpha(.5f));
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
    // the info rows (app.js infoRows()), 13px Lexend: key at 6 % in white .55, value at 34 %
    std::vector<std::pair<std::string, std::string>> rows;
    const auto &g = item.game;
    const bool app_like = item.system == data::System::ps5 || item.system == data::System::ps4 ||
                          item.system == data::System::custom || item.system == data::System::media;
    if (app_like)
    {
        rows.push_back({"Type", item.system == data::System::media    ? std::string("Streaming app")
                                : item.system == data::System::custom ? std::string("Homebrew")
                                                                      : std::string(tab_name(item.system)) + " game"});
        rows.push_back({"Title ID", g.app.empty() ? std::string("Unknown") : g.app});
    }
    else
    {
        rows.push_back({"Region", g.region.empty() ? std::string("Unknown") : g.region});
        std::string file = "Folder";
        if (!g.folder)
        {
            const auto dot = g.path.find_last_of('.');
            file = dot == std::string::npos ? std::string("File") : upper(g.path.substr(dot + 1));
        }
        rows.push_back({"File", file});
        rows.push_back({"Emulator", emulator_name(item.system)});
        if (!g.disc_id.empty())
            rows.push_back({"Serial", g.disc_id});
    }
    float y = top + shot_h * 2.0f + 44.0f * k;
    const float fs = 13.0f * k;
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
    canvas.set_camera(kPerspective, kTrackX, kTrackY);
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
    struct BandTile
    {
        std::string label;
        float y;
    };
    std::vector<BandTile> tiles;
    std::vector<float> rows(slots_.size(), -1.0f);
    std::vector<float> tile_of(slots_.size(), 0.0f);
    const UiFont title_face = app::face(Face::lexend600);
    canvas.begin_spine();
    {
        DrawList &sl = canvas.list();
        const Color plastic = rgb(st.case_color), ink = rgb(st.spine_ink);
        const Color band_bg = rgb(st.spine_band.bg), band_fg = rgb(st.spine_band.fg);
        float next_row = 0.0f;
        for (int i : drawn)
        {
            if (next_row + R > AH)
                break;
            const Item &item = shelf.items[static_cast<std::size_t>(i)];
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
                                                                      : std::string(st.spine_band.label);
            auto found = std::find_if(tiles.begin(), tiles.end(), [&](const BandTile &b) { return b.label == label; });
            if (found == tiles.end())
            {
                const float ty = tiles.empty() ? 0.0f : tiles.back().y + band_h + 4.0f;
                if (ty + band_h <= AH)
                {
                    tiles.push_back({label, ty});
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
            const float tw = static_cast<float>(GlCanvas::back_width), th = static_cast<float>(GlCanvas::back_height);
            float w = tw, h = tw * bh / bw;
            if (h > th)
                h = th, w = th * bw / bh;
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
        const float alpha = slot.fade.at(now);
        const Item &item = shelf.items[static_cast<std::size_t>(i)];
        const float x = slot.x.at(now), z = slot.z.at(now), ry = slot.ry.at(now);
        const bool is_sel = i == sel;
        CaseOp op;
        op.x = kTrackX + x;
        op.y = kTrackY;
        op.z = z;
        op.width = bw;
        op.height = bh;
        op.depth = bd;
        op.yaw = (ry + (is_sel ? turn : 0.0f)) * kPi / 180.0f;
        op.front = cover_texture(canvas, item);
        if (is_sel && back_tex)
        {
            op.back = back_tex;
            op.flip_back = back_drawn;
            for (int c = 0; c < 4; ++c)
                op.back_uv[c] = back_uv[c];
        }
        op.side_r = side.r, op.side_g = side.g, op.side_b = side.b;
        op.alpha = alpha;
        op.shade = is_sel ? 1.0f : 0.66f; // .slot:not(.sel) .dim{opacity:.34}
        // The front is rounded only away from the spine you see; cases right of the selection (.rside) mirror it.
        op.corner = st.rc;
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
        // Floor: the selected case's real reflection (setup.css box-reflect: below 3px, .3 fading over 42 %)
        if (is_sel)
        {
            CaseOp r = op;
            r.reflection = true;
            r.floor_y = kTrackY + bh * .5f + 1.5f;
            r.fade_length = bh * .42f;
            canvas.case3d(r);
        }
        canvas.case3d(op);
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
        const std::string word = wm.upper ? upper(st.word) : std::string(st.word);
        const float ww = measure(wm.face, word, wm.size, wm.spacing);
        list.shadow({left, top + 2, ww, wm.size}, wm.size * .3f, 16.0f, wm.glow.with_alpha(wm.glow.a * .6f));
        const std::size_t first = list.mark();
        text(list, wm.face, word, left, top, wm.size, kInk, Align::left, wm.spacing, wm.size);
        list.recolor(first, top, top + wm.size, wm.top, wm.bottom);
        // .wm-tag: 600 9px, letter-spacing .38em, uppercase; gap 7px
        float y = top + wm.size + 7.0f;
        text(list, Face::lexend600, upper(st.word_sub), left, y, 9.0f, wm.tag, Align::left, .38f, 9.0f);
        // .sys-count: margin-top 8px; 11px 500, .24em, uppercase, --ink-dim
        y += 9.0f + 8.0f;
        char count[48];
        const int n = model_.cur().game_count;
        std::snprintf(count, sizeof count, "%d %s", n, n == 1 ? "GAME" : "GAMES");
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
            const float sx = cx - 11.0f - 12.0f - 24.0f - 12.0f, sy = cy;
            list.rounded_rect({sx, sy - 3.0f, 4.0f, 6.0f}, 1.0f, kInkDim);
            list.triangle({sx + 1.0f, sy - 6.0f, 8.0f, 12.0f}, kInkDim, 0.0f, -1.5708f);
            list.arc(sx + 6.0f, sy, 6.0f, 1.4f, 0.6f, 1.9f, kInkDim);
            list.arc(sx + 6.0f, sy, 10.0f, 1.4f, 0.5f, 2.1f, kInkDim);
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
        const float mw = measure(Face::lexend500, "Menu", 11.0f) + 14.0f + 6.0f + 16.0f, mh = 22.0f;
        list.bordered_rect({right - mw, y, mw, mh}, mh * .5f, Color{0, 0, 0, 0}, 1.0f, kLine);
        const float gx = right - mw + 8.0f + 1.0f, gy = y + mh * .5f;
        for (float dy : {-3.5f, 0.0f, 3.5f})
            list.line(gx, gy + dy, gx + 11.0f, gy + dy, 1.4f, kInkDim);
        text(list, Face::lexend500, "Menu", right - 8.0f, y + 4.0f, 11.0f, kInkDim, Align::right, 0.0f, 14.0f);
    }
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
    const std::string title = it ? it->title : "No games here yet";
    list.shadow({W * .5f - measure(Face::lexend500, title, 44.0f) * .5f, y + 10, measure(Face::lexend500, title, 44.0f), 34},
                12, 20, Color{0, 0, 0, .4f});
    text(list, Face::lexend500, title, W * .5f, y, 44.0f, kInk, Align::center, -.005f);
    y += app::face(Face::lexend500).font->line_height(44.0f) + 6.0f;
    if (it)
    {
        const auto parts = meta_for(*it, model_.cur().system);
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
    const char *play = !it ? "Set up"
                       : it->kind == ItemKind::setup ? "Set up"
                       : it->kind == ItemKind::emulator ? "Open"
                       : it->kind == ItemKind::random ? "Pick a game"
                                                      : "Play";
    const Hint hints[] = {
        {{"L2", "R2"}, "System", nullptr},
        {{"tri", nullptr}, "Flip", "\xe2\x99\xa5"},
        {{"sq", nullptr}, "Details", nullptr},
        {{"x", nullptr}, play, nullptr},
        {{"R3", nullptr}, "Inspect", nullptr},
        {{"L3", nullptr}, "Shuffle", nullptr},
        {{"share", nullptr}, "Settings", "Museum"},
        {{"menu", nullptr}, "Menu", "Sort"},
    };
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
        return 4.0f + measure(Face::lexend600, "HOLD", 9.5f, .06f) + 10.0f + 4.0f + label;
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
    for (std::size_t n = 0; n < std::size(hints); ++n)
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
            const float iw = measure(Face::lexend600, "HOLD", 9.5f, .06f) + 10.0f;
            list.rounded_rect({hx, cy - 7.5f, iw, 15.0f}, 4.0f, Color{1, 1, 1, .1f * .82f});
            text(list, Face::lexend600, "HOLD", hx + 5.0f, cy - 7.0f, 9.5f, Color{1, 1, 1, .7f * .82f}, Align::left, .06f, 14.0f);
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
    text(list, Face::lexend600, loading ? upper(status_) : "NOTHING ON THE SHELF YET", W * .5f, cy - 52.0f, 11.0f,
         kInkDim, Align::center, .24f, 14.0f);
    text(list, Face::lexend600, loading ? "Reading your library" : "Connect your emulators and game folders", W * .5f,
         cy - 28.0f, 34.0f, kInk, Align::center, 0.0f, 42.0f);
    text(list, Face::lexend500,
         loading ? "Installed games, then the folders your emulators use"
                 : "Put your games in Emulation/roms/<system> on your drive and open Boxshelf again.",
         W * .5f, cy + 24.0f, 16.0f, kInkDim, Align::center, 0.0f, 22.0f);
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
    draw_floor_and_halo(canvas.list(), now);
    draw_cases(canvas, now);
    auto &hud = canvas.list();
    draw_info(hud, now);
    draw_top_bar(hud, now, clock);
    draw_letter_flash(hud, now);
    if (!model_.empty())
        draw_hints(hud);
    draw_toast(hud, now);
}
} // namespace boxshelf::app
