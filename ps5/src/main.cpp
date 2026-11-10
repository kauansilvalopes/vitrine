// Boxshelf PS5 - console entry point. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/scene.hpp"
#include "gl_main.hpp"
#include "platform/ps5/system.hpp"

namespace
{
std::int64_t start_us = 0;

void draw(boxshelf::GlCanvas &canvas) noexcept
{
    boxshelf::app::SceneInput input;
    input.time = static_cast<double>(hui::sys::monotonic_us() - start_us) / 1e6;
    boxshelf::app::draw_scene(canvas, input);
}
} // namespace

int main()
{
    hui::sys::log("[BOXSHELF] boot PPSA99850");
    start_us = hui::sys::monotonic_us();
    boxshelf::run_gl(draw);
}
