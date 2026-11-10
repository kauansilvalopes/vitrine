// Boxshelf PS5 - the shelf scene. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/scene.hpp"

#include <cmath>

namespace boxshelf::app
{
namespace
{
using hui::gfx::Color;
using hui::gfx::Rect;

constexpr float W = hui::gfx::kVirtualWidth;
constexpr float H = hui::gfx::kVirtualHeight;
// style.css: the track (and the shelf's focus point) sits at top:42%.
constexpr float kTrackY = H * 0.42f;

// style.css `html,body{background:#07070c}`.
constexpr std::uint32_t kBody = 0x07070c;

// `.vignette`, second layer: linear-gradient(to bottom, rgba(0,0,0,.25), transparent 20%, transparent 62%,
// rgba(0,0,0,.5)). The first layer (a radial ellipse) is approximated with soft edge shadows until the
// batch shader gains a radial-gradient shape (phase 2).
void vignette(hui::gfx::DrawList &list)
{
    const Color black = Color::rgb(0x000000);
    list.gradient_rect({0, 0, W, H * 0.20f}, 0, black.with_alpha(0.25f), black.with_alpha(0.0f));
    list.gradient_rect({0, H * 0.62f, W, H * 0.38f}, 0, black.with_alpha(0.0f), black.with_alpha(0.5f));
    // radial-gradient(ellipse 80% 70% at 50% 42%, transparent 35%, rgba(0,0,0,.5) 100%)
    const float clear_half = W * 0.80f * 0.35f; // horizontal radius that stays transparent
    const float side = W * 0.5f - clear_half;
    list.gradient_rect_h({0, 0, side, H}, 0, black.with_alpha(0.5f), black.with_alpha(0.0f));
    list.gradient_rect_h({W - side, 0, side, H}, 0, black.with_alpha(0.0f), black.with_alpha(0.5f));
}

// `.floor`: centred under the shelf, radial-gradient(ellipse 50% 30% at 50% 30%, rgba(0,0,0,.45), transparent
// 70%). The selected case's coloured glow (first layer) arrives with the cases in phase 2.
void floor_shadow(hui::gfx::DrawList &list, float case_height)
{
    const float top = kTrackY + case_height * 0.5f - 10.0f;
    const float width = std::fmin(1400.0f, W * 1.4f), height = 160.0f;
    // Ellipse centre and radii in the .floor box; the fade reaches transparent at 70 % of the radii.
    const float cx = W * 0.5f, cy = top + height * 0.30f;
    const float rx = width * 0.50f * 0.70f, ry = height * 0.30f * 0.70f;
    const Rect core{cx - rx * 0.5f, cy - ry * 0.5f, rx, ry};
    list.shadow(core, ry * 0.5f, ry, Color::rgb(0x000000, 0.45f));
}
} // namespace

void draw_scene(GlCanvas &canvas, const SceneInput &input) noexcept
{
    (void)input;
    auto &list = canvas.list();
    list.rounded_rect({0, 0, W, H}, 0, Color::rgb(kBody));
    // bh = min(vh*.4, vw*.22/ratio, 520..620); the PS5 case ratio (.787) gives the default height.
    const float case_height = std::fmin(H * 0.4f, W * 0.22f / 0.787f);
    floor_shadow(list, case_height);
    vignette(list);
}
} // namespace boxshelf::app
