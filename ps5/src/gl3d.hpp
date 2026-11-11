// Boxshelf PS5 (engine from Estante) - small 3D pass for game cases and discs. SPDX-License-Identifier: GPL-3.0-or-later
//
// The 2D interface is drawn by the kit's draw list. This module draws a few textured meshes (a case, a disc) in
// perspective between two 2D layers. World units are virtual pixels: the plane z = 0 maps exactly onto the 1920x1080
// canvas, z grows toward the viewer, y grows downward. There is no depth buffer: a case is convex, so culling back
// faces is enough, and the caller draws shapes back to front.
#pragma once

#include <cstdint>

namespace boxshelf
{
struct Mat4
{
    float m[16]; // column major
};
Mat4 mat4_identity() noexcept;
Mat4 mat4_multiply(const Mat4 &a, const Mat4 &b) noexcept;
Mat4 mat4_translate(float x, float y, float z) noexcept;
Mat4 mat4_scale(float x, float y, float z) noexcept;
Mat4 mat4_rotate_x(float radians) noexcept;
Mat4 mat4_rotate_y(float radians) noexcept;
Mat4 mat4_rotate_z(float radians) noexcept;

// A jewel case: a box with the cover on the front, a second picture on the back and plain sides.
struct CaseOp
{
    float x = 960.0f, y = 540.0f, z = 0.0f; // centre
    float width = 300.0f, height = 450.0f, depth = 56.0f;
    float yaw = 0.0f;                        // turn about the vertical axis, radians
    float pitch = 0.0f;                      // tilt about the horizontal axis
    float roll = 0.0f;                       // turn about the axis out of the screen
    float scale = 1.0f;
    std::uint32_t front = 0, back = 0;       // GL texture names (0 draws a flat colour)
    std::uint32_t spine = 0;                 // picture for the left edge (rendered into a framebuffer)
    float shine = 0.0f;                      // glint on the front that slides as the case turns
    bool flip_back = false;                  // the back texture was rendered into a framebuffer (rows upside down)
    float side_r = 0.10f, side_g = 0.11f, side_b = 0.13f;
    float alpha = 1.0f;
    float shade = 1.0f;                      // multiplies the colour (dim cases far from the focus)
    bool reflection = false;                 // draw it mirrored about floor_y, fading out
    float floor_y = 0.0f, fade_length = 120.0f;
    int part = 0;                            // 0 the whole case, 1 everything but the front, 2 only the front
    float opening = 0.0f;                    // 0 closed .. 1 the front cover swung open on its left hinge
    // Boxshelf's case anatomy (style.css / setup.css):
    float spine_uv[4] = {0, 0, 1, 1};        // the part of `spine` this case shows (a row of the spine atlas)
    bool spine_both = false;                 // the spine picture on the right side too (setup.css .f-right.spine2)
    float glare = 0.0f;                      // .glare strength (1 selected, .55 the others; 0 none)
    float glare_pos = 0.5f;                  // its centre across the front, 0..1
    float sheen_pos = -9.0f;                 // .sheen sweep centre across the front (< -5: none)
    float corner = 0.0f;                     // corner radius as a fraction of the width (style.rc)
    float corner_side = 0.0f;                // which front corners round: 1 the right ones, -1 the left, 0 all
    float spine_band[4] = {0, 0, 0, 0};      // .spine-band tile in `spine`: u left, v top, u right, v bottom
    float spine_band_h = 0.0f;               // its share of the height (.09; 0 none)
    float spine_shade[4] = {0, 0, 0, .5f};   // .f-left::after film: black alpha back, white alpha, black front, stop
    float film = 0.0f;                       // .f-front::after film strength (1 selected, .55 others)
    float back_uv[4] = {0, 0, 1, 1};         // the part of `back` this case shows
    bool favorite = false;                   // style.css .slot.fav .f-front::before: the heart badge
};

// A disc: label picture on the face, silver rim, hub and hole.
struct DiscOp
{
    float x = 960.0f, y = 540.0f, z = 0.0f;
    float radius = 185.0f;
    float spin = 0.0f;                       // turn in the plane of the disc, radians
    float yaw = 0.0f;                        // tilt about the vertical axis
    std::uint32_t label = 0;
    float alpha = 1.0f;
    float sheen = 0.0f;                      // the bright bands of a real disc, fixed while the disc spins
    float tint_r = 0.82f, tint_g = 0.84f, tint_b = 0.90f; // colour of the silver
    float hole_r = 0.06f, hole_g = 0.065f, hole_b = 0.08f; // what shows through the centre hole
};

// Inspect (Boxshelf's inspect3d.js): one game case held in the middle of the screen, open-able, with its disc.
struct InspectOp
{
    float height = 517.0f;                   // the case's height on screen (virtual pixels)
    float ratio = .71f, depth = .1f;         // the style's width / height and depth / width
    float corner = .03f;                     // style.rc
    float media_size = .62f;                 // app.js MEDIA[sys].size: the disc's diameter / the case's width
    float disc_lift = 0.0f;                  // inspect3d.js discLift(): .1 for GameCube
    float edge = .022f;
    bool cart = false;                       // app.js MEDIA kind "cart" (Switch, 3DS): a cartridge, not a disc                      // style.edge: the border around the cover art (cropped off the disc label)
    std::uint32_t front = 0, back = 0, spine = 0;
    bool flip_back = false;
    float back_uv[4] = {0, 0, 1, 1};
    float spine_uv[4] = {0, 0, 1, 1}, spine_band[4] = {0, 0, 0, 0}, spine_shade[4] = {0, 0, 0, .5f};
    float spine_band_h = 0.0f;
    float case_r = .1f, case_g = .1f, case_b = .12f;   // caseColor
    float side_r = .06f, side_g = .06f, side_b = .07f; // caseSide
    float inner_r = .17f, inner_g = .18f, inner_b = .2f; // the moulded inside (dark plastic lifted, as inspect3d.js does)
    float rx = 0.0f, ry = 0.0f;              // the turn, degrees
    float lid = 0.0f;                        // 0 closed .. 1 open (165 degrees)
    float out = 0.0f;                        // 0 the disc in the case .. 1 out in front
    float spin = 0.0f;                       // the disc's own slow turn, radians
    float disc_turn = 0.0f;                  // the turn the disc keeps once out, degrees
    float zoom = 1.0f, scale = 1.0f, offset_y = 0.0f, alpha = 1.0f; // L2 / R2 zoom; the entrance (scale .84 -> 1)
};

class Gl3D
{
  public:
    bool init() noexcept;
    void release() noexcept;
    // CSS perspective: distance and origin in virtual pixels. The default (2015.3 at the centre) is the 30 degree
    // field of view the engine always had; Boxshelf's shelf uses 1800 at (960, 42 % of 1080).
    void set_camera(float distance, float origin_x, float origin_y) noexcept
    {
        camera_distance_ = distance;
        camera_x_ = origin_x;
        camera_y_ = origin_y;
    }
    // Called once per frame by the renderer's custom layer: sets the state the passes need.
    void begin(int surface_width, int surface_height) noexcept;
    // Restores the state the 2D batch expects (culling off).
    void end() noexcept;
    void draw(const CaseOp &op) noexcept;
    void draw(const DiscOp &op) noexcept;
    // Inspect: renders into its own target (with depth); inspect_texture() is the picture, rows bottom-up.
    bool ensure_inspect_target(int width, int height) noexcept;
    void draw(const InspectOp &op, int surface_width, int surface_height) noexcept;
    std::uint32_t inspect_texture() const noexcept
    {
        return isp_tex_;
    }
    bool ready() const noexcept
    {
        return program_ != 0;
    }

  private:
    struct Vertex
    {
        float x, y, z, u, v, nx, ny, nz;
    };
    void draw_range(const Mat4 &model, int first, int count, std::uint32_t texture, float r, float g, float b,
                    float alpha, const CaseOp *reflect) noexcept;

    unsigned program_ = 0, vao_ = 0, buffer_ = 0;
    float camera_distance_ = 2015.2846f, camera_x_ = 960.0f, camera_y_ = 540.0f;
    int surface_width_ = 1920, surface_height_ = 1080;
    Mat4 projection_{};
    // uniform locations
    float flip_ = 0.0f, shine_ = 0.0f, sheen_ = 0.0f, spin_ = 0.0f, ambient_ = 0.62f;
    float uvrect_[4] = {0, 0, 1, 1}, glare_[4] = {0, 0, 1, -9}, round_[2] = {0, 0};
    float spine_[3] = {0, 0, 0}, band_[4] = {0, 0, 0, 0}, shade_[4] = {0, 0, 0, .5f}, film_ = 0.0f;
    float badge_[3] = {0, 1, 1}; // on, face width and height in virtual pixels
    int u_badge_ = -1;
    unsigned isp_fbo_ = 0, isp_resolve_ = 0, isp_color_ = 0, isp_depth_ = 0, isp_tex_ = 0;
    int isp_w_ = 0, isp_h_ = 0;
    int u_uvrect_ = -1, u_glare_ = -1, u_round_ = -1, u_spine_ = -1, u_band_ = -1, u_shade_ = -1, u_film_ = -1;
    int u_shine_ = -1, u_sheen_ = -1, u_spin_ = -1;
    int spine_first_ = 0, spine_right_first_ = 0;
    int u_flip_ = -1;
    int u_mvp_ = -1, u_model_ = -1, u_tex_ = -1, u_color_ = -1, u_textured_ = -1, u_ambient_ = -1, u_fade_ = -1,
        u_light_ = -1;
    int box_front_ = 0, box_back_ = 6, box_sides_ = 12, box_sides_count_ = 24, disc_first_ = 36, disc_count_ = 0;
    int cover_first_ = 0, cover_count_ = 0;
};
} // namespace boxshelf
