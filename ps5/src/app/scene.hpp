// Boxshelf PS5 - the shelf scene. SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from Boxshelf's renderer (renderer/app.js, renderer/style.css). Everything here is drawn in the
// 1920x1080 virtual canvas; the console and the PC preview call the same function.
#pragma once

#include "gl_canvas.hpp"

namespace boxshelf::app
{
struct SceneInput
{
    // Seconds since the app started; drives the breathing halo and other loops.
    double time = 0.0;
};

// Draws one whole frame of the scene. Phase 0: the empty backdrop (body colour, vignette, floor).
void draw_scene(GlCanvas &canvas, const SceneInput &input) noexcept;
} // namespace boxshelf::app
