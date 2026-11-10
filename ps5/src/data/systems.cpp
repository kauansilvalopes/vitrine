// Boxshelf PS5 - case style table. SPDX-License-Identifier: GPL-3.0-or-later
// Values copied from Boxshelf renderer/app.js STYLE / WORDMARKS and renderer/systems-extra.js (psx, nds).
#include "data/systems.hpp"

#include <algorithm>

namespace boxshelf::data
{
namespace
{
using S = System;
// clang-format off
constexpr CaseStyle kStyles[kSystemCount] = {
    // id      key     rc     re    hue ratio  depth  edge  h    wrap caseColor caseSide  spineInk  band?  band{bg,fg,label}                       spineBand{bg,fg,label}               tint top  tint bot  glow      word         sub
    {S::home, "home",  .04f,  .22f, 268, .71f,  .09f,  .025f, 1,  false, 0x1b1d27, 0x101218, 0xe8e8f0, true, {0x1b1d27, 0xc9cfe0, "HOME"},         {0x2a2d3a, 0xc9cfe0, "HOME"},        0x3b2f6e, 0x0a0b16, 0xc9b8ff, "Home",       "Continue & favorites"},
    {S::gc,   "gc",    .03f,  .2f,  258, .712f, .105f, .022f, 1,  false, 0x16161b, 0x0b0b0e, 0xe8e8f0, true, {0x26262e, 0xc3c3cf, "GAMECUBE"},     {0x2c2c35, 0xc3c3cf, "GCN"},         0x3a2a86, 0x120d2c, 0x9d8cff, "GameCube",   "Home console"},
    {S::wii,  "wii",   .04f,  .22f, 205, .711f, .104f, .03f,  1,  false, 0xf2f3f5, 0xd9dbe0, 0x4a4f58, true, {0xffffff, 0x8e949e, "Wii"},          {0xffffff, 0x8e949e, "Wii"},         0x2f7bff, 0x0b2a7a, 0x9fd0ff, "Wii",        "Motion console"},
    {S::wiiu, "wiiu",  .04f,  .22f, 194, .711f, .104f, .03f,  1,  false, 0x1d8fd1, 0x146ea6, 0xffffff, true, {0x1d8fd1, 0xffffff, "Wii U"},        {0xffffff, 0x1d8fd1, "Wii U"},       0x0f6fb0, 0x04203a, 0x6fd0ff, "Wii U",      "Home console"},
    {S::sw,   "sw",    .06f,  .28f, 355, .612f, .096f, .035f, 1,  false, 0xd8101f, 0xa30a16, 0xffffff, true, {0xd8101f, 0xffffff, "SWITCH"},       {0xd8101f, 0xffffff, "SW"},          0x7a2bd6, 0x2a0b4e, 0xff6470, "Switch",     "Hybrid console"},
    {S::sw2,  "sw2",   .06f,  .28f, 352, .612f, .096f, .035f, 1,  false, 0xe60012, 0xad000d, 0xffffff, true, {0xe60012, 0xffffff, "SWITCH 2"},     {0xe60012, 0xffffff, "SW2"},         0x16608f, 0x071a2e, 0xff5a66, "Switch 2",   "Hybrid console"},
    {S::psp,  "psp",   .055f, .24f, 235, .589f, .14f,  .03f,  1,  false, 0x101014, 0x07070a, 0xe8e8f0, true, {0x0a0a0d, 0xffffff, "PSP"},          {0x0a0a0d, 0xffffff, "PSP"},         0x4a4f63, 0x0b0c12, 0xcfd6ff, "PSP",        "Handheld"},
    {S::ps2,  "ps2",   .035f, .2f,  226, .711f, .104f, .03f,  1,  false, 0x0f1d55, 0x0a1440, 0xffffff, true, {0x0f1d55, 0xdfe6ff, "PS2"},          {0x0f1d55, 0xdfe6ff, "PS2"},         0x1b2a7a, 0x070b24, 0x8fa6ff, "PS2",        "Home console"},
    {S::ps3,  "ps3",   .065f, .26f, 221, .787f, .1f,   .03f,  1,  false, 0x203553, 0x16263f, 0xffffff, true, {0x0c0d10, 0xffffff, "PS3"},          {0x0c0d10, 0xffffff, "PS3"},         0x24406e, 0x090f1e, 0x7ab4ff, "PS3",        "Home console"},
    {S::ps4,  "ps4",   .065f, .26f, 216, .787f, .089f, .03f,  1,  false, 0x1d4ed8, 0x153a9e, 0xffffff, true, {0x1d4ed8, 0xffffff, "PS4"},          {0x1d4ed8, 0xffffff, "PS4"},         0x1e3a8a, 0x0b1533, 0x6aa6ff, "PS4",        "Home console"},
    {S::ps5,  "ps5",   .065f, .26f, 220, .787f, .089f, .03f,  1,  false, 0x2b62d9, 0x1f4cb3, 0x111318, true, {0xffffff, 0x111318, "PS5"},          {0xffffff, 0x111318, "PS5"},         0x24345e, 0x0a0f1f, 0x9ec2ff, "PS5",        "Home console"},
    {S::x360, "x360",  .04f,  .22f, 115, .711f, .104f, .018f, 1,  true,  0x1a7f1a, 0x0f5a12, 0xffffff, true, {0x107c10, 0xffffff, "XBOX 360"},     {0x107c10, 0xffffff, "X360"},        0x145a16, 0x061a07, 0x7ee87e, "Xbox 360",   "Home console"},
    {S::n3ds, "n3ds",  .05f,  .24f, 352, 1.127f,.1f,   .03f,  .64f, false, 0xf4f4f6, 0xdcdde2, 0x3a3d45, true, {0xffffff, 0xd40f2a, "NINTENDO 3DS"}, {0xffffff, 0xd40f2a, "3DS"},       0xc0182e, 0x3a0710, 0xff7a8a, "3DS",        "Handheld"},
    // systems-extra.js: S('psx', {...jewel, ...}) -> jewel = {rc .012, re .06, edge .02, depth .073}; spineInk default #fff
    {S::psx,  "psx",   .012f, .06f, 220, 1.136f,.073f, .02f,  .7f, false, 0x121216, 0x08080a, 0xffffff, false,{0, 0, ""},                        {0x0a0a0c, 0xffffff, "PlayStation"}, 0x3b3f4f, 0x0c0d12, 0xc7cde0, "PlayStation","Home console"},
    {S::nds,  "nds",   .05f,  .24f, 0,   .9f,   .1f,   .03f,  1,  false, 0xf4f4f6, 0xdcdde2, 0x3a3d45, true, {0xffffff, 0x3a3d45, "NINTENDO DS"},  {0xffffff, 0x3a3d45, "DS"},          0x5a5f6e, 0x15161b, 0xd9dde8, "DS",         "Handheld"},
    // app.js custom: the PC case in a warm tint, band:null, spine label "PC"
    {S::custom,"custom",.02f, .16f, 28,  .667f, .07f,  .015f, 1,  false, 0x211b17, 0x15110e, 0xf3e6da, false,{0, 0, ""},                        {0x15110e, 0xe0a878, "PC"},          0x6e3f1d, 0x1c120a, 0xffbf8a, "Custom",     "PC ports & recomps"},
    {S::media,"media", .03f,  .18f, 285, .71f,  .06f,  .02f,  1,  false, 0x1a1c24, 0x0e0f14, 0xe8e8f0, true, {0x1a1c24, 0x9aa3b8, "APP"},          {0x2a2d38, 0xc8cede, "APP"},         0x2a3550, 0x0a0d16, 0xb9a6ff, "Media",      "Streaming"},
};
// clang-format on
} // namespace

const CaseStyle &style(System system) noexcept
{
    const int i = static_cast<int>(system);
    return kStyles[i >= 0 && i < kSystemCount ? i : static_cast<int>(System::media)];
}

System system_from_key(std::string_view key) noexcept
{
    for (const auto &s : kStyles)
        if (s.key == key)
            return s.id;
    return System::count;
}

CaseSize case_size(const CaseStyle &s, float vw, float vh, bool effects) noexcept
{
    const float h = std::min({vh * 0.4f, vw * 0.22f / s.ratio, effects ? 620.0f : 520.0f});
    const float w = h * s.ratio;
    return {w, h, w * s.depth};
}
} // namespace boxshelf::data
