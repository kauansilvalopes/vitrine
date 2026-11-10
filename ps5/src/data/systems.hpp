// Boxshelf PS5 - the shelves this console can fill, and each one's case style. SPDX-License-Identifier: GPL-3.0-or-later
//
// Source of truth: Boxshelf's renderer/app.js `STYLE` + `WORDMARKS` (lines 11-30) and renderer/systems-extra.js
// (psx, nds), and main/systems.js for ROM folder names and extensions. Keep the numbers identical to those files.
#pragma once

#include <cstdint>
#include <string_view>

namespace boxshelf::data
{
// Shelf order follows main/systems.js CONSOLES (then the retro rows), with Home first and Media last, as Boxshelf
// builds its list. Systems no PS5 emulator runs (PSP, PS3, Xbox 360, Switch 2) are kept for parity of the style
// table but have no scan rules.
enum class System : std::uint8_t
{
    home,
    gc,
    wii,
    wiiu,
    sw,
    sw2,
    psp,
    ps2,
    ps3,
    ps4,
    ps5,
    x360,
    n3ds,
    psx,
    nds,
    custom, // PC ports and recomps (Boxshelf's Custom shelf): homebrew ports installed on the PS5
    media,
    count
};
constexpr int kSystemCount = static_cast<int>(System::count);

struct Band
{
    std::uint32_t bg = 0, fg = 0; // 0xRRGGBB
    std::string_view label;
};

// One shelf's case. ratio = width / height of the real box; depth, edge as fractions of the case width;
// rc = corner radius (fraction of width); re = spine bevel (fraction of depth); h = museum height factor (1 when
// Boxshelf leaves it unset). wrap: one printed sheet runs round front, spine and back (Xbox 360).
struct CaseStyle
{
    System id;
    std::string_view key; // Boxshelf's id ("gc", "n3ds", "psx", ...)
    float rc, re;
    int hue;
    float ratio, depth, edge, h;
    bool wrap;
    std::uint32_t case_color, case_side, spine_ink;
    bool has_band; // false where Boxshelf has band:null (PS1 jewel case)
    Band band, spine_band;
    std::uint32_t tint_top, tint_bottom, glow;
    std::string_view word, word_sub; // WORDMARKS: the system word shown top-left, and its subtitle
};

const CaseStyle &style(System system) noexcept;
// Boxshelf id -> System; System::count when unknown.
System system_from_key(std::string_view key) noexcept;

// Case size on a 1920x1080 screen (app.js place(): bh = min(vh*.4, vw*.22/ratio, fx ? 620 : 520)).
struct CaseSize
{
    float w, h, d;
};
CaseSize case_size(const CaseStyle &style, float viewport_w, float viewport_h, bool effects = true) noexcept;
} // namespace boxshelf::data
