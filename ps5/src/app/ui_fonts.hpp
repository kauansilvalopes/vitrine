// Boxshelf PS5 - the faces Boxshelf's renderer uses (renderer/fonts), baked to SDF. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gfx/font.hpp"
#include "gfx/gl_batch.hpp"

#include <cstdint>

namespace boxshelf::app
{
enum class Face : std::uint8_t
{
    lexend500, // body text, tabs, hints, titles
    lexend600, // spines, keycaps
    lexend800, // default system word
    chakra700, // GameCube wordmark
    syne800,   // Wii, Wii U
    unbounded700, // Switch, 3DS
    michroma400,  // PS2, PSP, PS3
    audiowide400, // PS4
    dmserif_italic, // Media
    bungee400,      // Xbox 360
    count
};

struct UiFont
{
    const hui::gfx::Font *font = nullptr;
    std::uint32_t texture = 0;
    explicit operator bool() const
    {
        return font != nullptr;
    }
};

// Loads every face from dir ("/app0/assets/fonts" on the console, "assets/fonts" on the PC). Faces that fail stay
// empty and fall back to Lexend 500. Inter (already loaded by the caller) becomes the fallback for other scripts.
bool load_ui_fonts(hui::gfx::GlBatch &batch, const char *dir, const hui::gfx::Font *inter,
                   std::uint32_t inter_texture);
UiFont face(Face f);
} // namespace boxshelf::app
