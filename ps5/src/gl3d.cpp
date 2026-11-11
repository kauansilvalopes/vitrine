// Boxshelf PS5 (engine from Estante) - small 3D pass for game cases and discs. SPDX-License-Identifier: GPL-3.0-or-later
#include "gl3d.hpp"

#include "gfx/gl_program.hpp"
#include "platform/ps5/system.hpp"

#include <GL/glcorearb.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace boxshelf
{
Mat4 mat4_identity() noexcept
{
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 mat4_multiply(const Mat4 &a, const Mat4 &b) noexcept
{
    Mat4 r{};
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
        {
            float v = 0.0f;
            for (int k = 0; k < 4; k++)
                v += a.m[k * 4 + row] * b.m[col * 4 + k];
            r.m[col * 4 + row] = v;
        }
    return r;
}

Mat4 mat4_translate(float x, float y, float z) noexcept
{
    Mat4 r = mat4_identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

Mat4 mat4_scale(float x, float y, float z) noexcept
{
    Mat4 r = mat4_identity();
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}

Mat4 mat4_rotate_x(float a) noexcept
{
    Mat4 r = mat4_identity();
    r.m[5] = std::cos(a);
    r.m[6] = std::sin(a);
    r.m[9] = -std::sin(a);
    r.m[10] = std::cos(a);
    return r;
}

Mat4 mat4_rotate_y(float a) noexcept
{
    Mat4 r = mat4_identity();
    r.m[0] = std::cos(a);
    r.m[2] = -std::sin(a);
    r.m[8] = std::sin(a);
    r.m[10] = std::cos(a);
    return r;
}

Mat4 mat4_rotate_z(float a) noexcept
{
    Mat4 r = mat4_identity();
    r.m[0] = std::cos(a);
    r.m[1] = std::sin(a);
    r.m[4] = -std::sin(a);
    r.m[5] = std::cos(a);
    return r;
}

namespace
{
const char *vertex_source = R"(
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec3 a_normal;
uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_flip;
uniform vec4 u_uvrect; // u0, v0, u1, v1: the part of the texture this face shows
out vec2 v_uv;
out vec2 v_face; // 0..1 across the face, before u_uvrect
out vec3 v_normal;
out float v_y;
void main()
{
    vec4 world = u_model * vec4(a_pos, 1.0);
    v_face = a_uv;
    vec2 uv = vec2(a_uv.x, u_flip > 0.5 ? 1.0 - a_uv.y : a_uv.y);
    v_uv = mix(u_uvrect.xy, u_uvrect.zw, uv);
    v_normal = mat3(u_model) * a_normal;
    v_y = world.y;
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";

const char *fragment_source = R"(
in vec2 v_uv;
in vec2 v_face;
in vec3 v_normal;
in float v_y;
uniform sampler2D u_tex;
uniform vec4 u_glare;  // x: band centre across the face (0..1), y: strength, z: face height / width, w: sheen centre
uniform vec2 u_round;  // x: corner radius in face widths (0: square); y: which corners (0 all, 1 right only, -1 left)
uniform vec4 u_spine;  // x: 1 on a spine face, y: the band's share of the height (0: none), z: 1 on the right side
uniform vec4 u_band;   // the band tile in the texture: u at its left, v at its top, u at its right, v at its bottom
uniform vec4 u_shade;  // spine film (setup.css .f-left::after): black alpha at the back, white alpha, black at the front, white stop
uniform float u_film;  // strength of the front's ::after film (1 selected, .55 the others)
uniform vec3 u_badge;  // x: 1 draws the favourite heart, y / z: the face's width / height in virtual pixels
uniform vec4 u_color;
uniform int u_textured;
uniform float u_ambient;
uniform vec3 u_light;
uniform vec2 u_fade;
uniform float u_shine;
uniform float u_sheen;
uniform float u_spin;
out vec4 frag;
float dot2(vec2 v) { return dot(v, v); }
// Inigo Quilez's heart: the tip at (0, 0), the top near y = 1
float heart(vec2 p)
{
    p.x = abs(p.x);
    if (p.y + p.x > 1.0)
        return sqrt(dot2(p - vec2(0.25, 0.75))) - sqrt(2.0) / 4.0;
    return sqrt(min(dot2(p - vec2(0.0, 1.0)), dot2(p - 0.5 * max(p.x + p.y, 0.0)))) * sign(p.x - p.y);
}
void main()
{
    vec2 uv = v_uv;
    // style.css .spine-band: the top 9 % of the spine, its label written across the spine (not along it), so it is a
    // tile of its own in the atlas, read transposed. Across the face: v_face.y 0 is the front edge on the left side and
    // the back edge on the right; both put the element's own left (back on .f-left, front on .f-right) at 1 - v_face.y.
    if (u_spine.x > 0.5 && u_spine.y > 0.0 && v_face.x < u_spine.y)
        uv = vec2(mix(u_band.x, u_band.z, 1.0 - v_face.y), mix(u_band.y, u_band.w, v_face.x / u_spine.y));
    vec4 base = u_textured != 0 ? texture(u_tex, uv) : u_color;
    if (u_spine.x > 0.5)
    {
        // the film, measured from the back edge: rgba(0,0,0,a0) -> rgba(255,255,255,a1) at w -> rgba(0,0,0,a2)
        float t = u_spine.z > 0.5 ? v_face.y : 1.0 - v_face.y;
        float f = t < u_shade.w ? t / u_shade.w : (t - u_shade.w) / (1.0 - u_shade.w);
        float fa = t < u_shade.w ? mix(u_shade.x, u_shade.y, f) : mix(u_shade.y, u_shade.z, f);
        float fw = t < u_shade.w ? mix(0.0, u_shade.y, f) : mix(u_shade.y, 0.0, f); // premultiplied white
        base.rgb = base.rgb * (1.0 - fa) + vec3(fw);
    }
    if (u_film > 0.0)
    {
        // .f-front::after: linear-gradient(90deg, rgba(0,0,0,.14), transparent 10%, transparent 88%, rgba(0,0,0,.18))
        // under linear-gradient(180deg, rgba(255,255,255,.16), rgba(255,255,255,.04) 18%, transparent 34%)
        float x = v_face.x, y = v_face.y;
        float dark = x < 0.10 ? 0.14 * (1.0 - x / 0.10) : (x > 0.88 ? 0.18 * (x - 0.88) / 0.12 : 0.0);
        float rim = y < 0.18 ? mix(0.16, 0.04, y / 0.18) : (y < 0.34 ? 0.04 * (1.0 - (y - 0.18) / 0.16) : 0.0);
        base.rgb *= 1.0 - dark * u_film;
        base.rgb = mix(base.rgb, vec3(1.0), rim * u_film);
    }
    float lit = max(dot(normalize(v_normal), normalize(u_light)), 0.0);
    float shade = u_ambient + (1.0 - u_ambient) * lit;
    vec4 c = vec4(base.rgb * shade, base.a * u_color.a);
    if (u_textured != 0)
        c.rgb *= u_color.rgb;
    if (u_shine > 0.0)
    {
        vec3 hv = normalize(normalize(u_light) + vec3(0.0, 0.0, 1.0));
        c.rgb += vec3(pow(max(dot(normalize(v_normal), hv), 0.0), 36.0)) * u_shine;
    }
    if (u_sheen > 0.0)
    {
        vec2 q = v_uv - 0.5;
        float band = pow(max(cos(2.0 * (atan(q.y, q.x) + u_spin)), 0.0), 10.0);
        c.rgb += vec3(band) * u_sheen * (0.25 + length(q));
    }
    if (u_glare.y > 0.0)
    {
        // style.css .glare: linear-gradient(104deg, ...) on a layer 1.2 faces wide, slid by the case's angle. Its stops
        // are p % of the line a 3-face layer would have (style.css comment), i.e. (p - 50 %) * (3 sin104 + h/w cos 76)
        // face widths from the centre, measured along the gradient direction (sin 104, -cos 104) with y down.
        float k = 3.0 * 0.97030 + 0.24192 * u_glare.z;
        float d = ((v_face.x - u_glare.x) * 0.97030 + (v_face.y - 0.5) * u_glare.z * 0.24192) / k + 0.5;
        float a = 0.0;
        if (d > 0.413 && d < 0.463) a = mix(0.0, 0.07, (d - 0.413) / 0.050);
        else if (d >= 0.463 && d < 0.497) a = mix(0.07, 0.34, (d - 0.463) / 0.034);
        else if (d >= 0.497 && d < 0.503) a = mix(0.34, 0.30, (d - 0.497) / 0.006);
        else if (d >= 0.503 && d < 0.533) a = mix(0.30, 0.06, (d - 0.503) / 0.030);
        else if (d >= 0.533 && d < 0.580) a = mix(0.06, 0.0, (d - 0.533) / 0.047);
        // the thin second line
        if (d > 0.547 && d < 0.567) a += mix(0.0, 0.12, (d - 0.547) / 0.020);
        else if (d >= 0.567 && d < 0.583) a += mix(0.12, 0.0, (d - 0.567) / 0.016);
        c.rgb = mix(c.rgb, vec3(1.0), a * u_glare.y);
    }
    if (u_glare.w > -5.0)
    {
        // style.css .sheen::after: a 42 %-wide white band (100deg, skewed -14deg) crossing the face once on landing
        float d = (v_face.x - u_glare.w) + 0.2493 * (v_face.y - 0.5) * u_glare.z;
        float a = clamp(1.0 - abs(d) / 0.21, 0.0, 1.0); // linear-gradient(100deg, transparent, rgba(255,255,255,.6), transparent)
        c.rgb = mix(c.rgb, vec3(1.0), 0.6 * a);
    }
    if (u_badge.x > 0.5)
    {
        // .slot.fav .f-front::before: right 8px, top 8px, 26px round, rgba(10,10,16,.75), a 14px heart #ff5c8a
        vec2 px = vec2((v_face.x - 1.0) * u_badge.y + 21.0, v_face.y * u_badge.z - 21.0);
        float aa = max(fwidth(px.x), 1e-3);
        float disc = clamp(0.5 - (length(px) - 13.0) / aa, 0.0, 1.0);
        c.rgb = mix(c.rgb, vec3(10.0, 10.0, 16.0) / 255.0, disc * 0.75);
        vec2 hp = vec2(px.x, -px.y) / 11.0 + vec2(0.0, 0.52);
        float h = clamp(0.5 - heart(hp) * 11.0 / aa, 0.0, 1.0);
        c.rgb = mix(c.rgb, vec3(1.0, 92.0 / 255.0, 138.0 / 255.0), h);
    }
    if (u_round.x > 0.0 && !(u_round.y > 0.5 && v_face.x < 0.5) && !(u_round.y < -0.5 && v_face.x > 0.5))
    {
        // rounded corners (style.css rc): distance outside the rounded rectangle, in face-width units
        vec2 p = abs(v_face - 0.5) * vec2(1.0, u_glare.z);
        vec2 b = vec2(0.5, 0.5 * u_glare.z) - vec2(u_round.x);
        vec2 q = p - b;
        float dist = length(max(q, 0.0)) - u_round.x;
        float px = fwidth(v_face.x);
        c.a *= clamp(0.5 - dist / max(px, 1e-4), 0.0, 1.0);
    }
    if (u_fade.y > 0.0)
    {
        float f = clamp(1.0 - (v_y - u_fade.x) / u_fade.y, 0.0, 1.0);
        c.a *= f * f;
    }
    frag = c;
}
)";
} // namespace

bool Gl3D::init() noexcept
{
    program_ = hui::gfx::build_program("boxshelf3d", vertex_source, fragment_source);
    if (!program_)
        return false;
    u_mvp_ = glGetUniformLocation(program_, "u_mvp");
    u_model_ = glGetUniformLocation(program_, "u_model");
    u_tex_ = glGetUniformLocation(program_, "u_tex");
    u_color_ = glGetUniformLocation(program_, "u_color");
    u_textured_ = glGetUniformLocation(program_, "u_textured");
    u_ambient_ = glGetUniformLocation(program_, "u_ambient");
    u_fade_ = glGetUniformLocation(program_, "u_fade");
    u_light_ = glGetUniformLocation(program_, "u_light");
    u_flip_ = glGetUniformLocation(program_, "u_flip");
    u_shine_ = glGetUniformLocation(program_, "u_shine");
    u_sheen_ = glGetUniformLocation(program_, "u_sheen");
    u_spin_ = glGetUniformLocation(program_, "u_spin");
    u_uvrect_ = glGetUniformLocation(program_, "u_uvrect");
    u_glare_ = glGetUniformLocation(program_, "u_glare");
    u_round_ = glGetUniformLocation(program_, "u_round");
    u_spine_ = glGetUniformLocation(program_, "u_spine");
    u_band_ = glGetUniformLocation(program_, "u_band");
    u_shade_ = glGetUniformLocation(program_, "u_shade");
    u_film_ = glGetUniformLocation(program_, "u_film");
    u_badge_ = glGetUniformLocation(program_, "u_badge");

    std::vector<Vertex> v;
    auto quad = [&](const float p[4][3], float nx, float ny, float nz, bool reversed = false)
    {
        static const float uv[4][2] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}}; // TL, BL, BR, TR
        const int order[6] = {0, 1, 2, 0, 2, 3}, flipped[6] = {0, 2, 1, 0, 3, 2};
        for (int k = 0; k < 6; k++)
        {
            const int i = reversed ? flipped[k] : order[k];
            v.push_back({p[i][0], p[i][1], p[i][2], uv[i][0], uv[i][1], nx, ny, nz});
        }
    };
    const float front[4][3] = {{-.5f, -.5f, .5f}, {-.5f, .5f, .5f}, {.5f, .5f, .5f}, {.5f, -.5f, .5f}};
    const float back[4][3] = {{.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}, {-.5f, -.5f, -.5f}};
    const float left[4][3] = {{-.5f, -.5f, .5f}, {-.5f, .5f, .5f}, {-.5f, .5f, -.5f}, {-.5f, -.5f, -.5f}};
    const float right[4][3] = {{.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}, {.5f, -.5f, .5f}};
    const float top[4][3] = {{.5f, -.5f, -.5f}, {.5f, -.5f, .5f}, {-.5f, -.5f, .5f}, {-.5f, -.5f, -.5f}};
    const float bottom[4][3] = {{.5f, .5f, .5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}, {-.5f, .5f, .5f}};
    const float tray[4][3] = {{-.45f, -.46f, -.44f}, {-.45f, .46f, -.44f}, {.45f, .46f, -.44f}, {.45f, -.46f, -.44f}};
    quad(front, 0, 0, 1);
    box_front_ = 0;
    quad(back, 0, 0, -1);
    box_back_ = 6;
    quad(left, -1, 0, 0, true);
    box_sides_ = 12;
    quad(right, 1, 0, 0, true);
    quad(top, 0, -1, 0, true);
    quad(bottom, 0, 1, 0, true);
    box_sides_count_ = 24;
    cover_first_ = static_cast<int>(v.size());
    quad(tray, 0, 0, 1);
    cover_count_ = 6;
    // The left edge of the case (the spine) as its own quad so a picture can run top to bottom along it.
    spine_first_ = static_cast<int>(v.size());
    {
        const float pos[4][3] = {{-.5f, -.5f, .5f}, {-.5f, .5f, .5f}, {-.5f, .5f, -.5f}, {-.5f, -.5f, -.5f}};
        const float uvs[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        const int order[6] = {0, 2, 1, 0, 3, 2};
        for (int i : order)
            v.push_back({pos[i][0], pos[i][1], pos[i][2], uvs[i][0], uvs[i][1], -1, 0, 0});
    }
    // The right edge with the spine's mapping (setup.css .f-right.spine2 carries the band and title too): u runs
    // top to bottom, v from the back edge to the front, so the title reads top to bottom from that side as well.
    spine_right_first_ = static_cast<int>(v.size());
    {
        const float pos[4][3] = {{.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}, {.5f, -.5f, .5f}};
        const float uvs[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        const int order[6] = {0, 2, 1, 0, 3, 2};
        for (int i : order)
            v.push_back({pos[i][0], pos[i][1], pos[i][2], uvs[i][0], uvs[i][1], 1, 0, 0});
    }
    disc_first_ = static_cast<int>(v.size());
    const int segments = 96;
    for (int i = 0; i < segments; i++)
    {
        // Counter-clockwise on screen with y down means the angle decreases.
        const float a0 = -6.2831853f * static_cast<float>(i) / segments;
        const float a1 = -6.2831853f * static_cast<float>(i + 1) / segments;
        v.push_back({0, 0, 0, 0.5f, 0.5f, 0, 0, 1});
        v.push_back({std::cos(a0) * .5f, std::sin(a0) * .5f, 0, 0.5f + std::cos(a0) * .5f, 0.5f + std::sin(a0) * .5f, 0, 0, 1});
        v.push_back({std::cos(a1) * .5f, std::sin(a1) * .5f, 0, 0.5f + std::cos(a1) * .5f, 0.5f + std::sin(a1) * .5f, 0, 0, 1});
    }
    disc_count_ = segments * 3;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &buffer_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(Vertex)), v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(20));
    glBindVertexArray(0);
    return true;
}

void Gl3D::release() noexcept
{
    if (buffer_)
        glDeleteBuffers(1, &buffer_);
    if (vao_)
        glDeleteVertexArrays(1, &vao_);
    if (program_)
        glDeleteProgram(program_);
    buffer_ = vao_ = program_ = 0;
}

void Gl3D::begin(int surface_width, int surface_height) noexcept
{
    surface_width_ = surface_width;
    surface_height_ = surface_height;
    // CSS `perspective: d; perspective-origin: ox oy` on the 1920x1080 virtual canvas: a point (x, y, z) in pixel
    // space (y down, z toward the viewer) lands at o + (p - o) * d / (d - z); the plane z = 0 maps 1:1.
    // clip = (ndc * w, 0, w) with w = 1 - z / d; no depth buffer, so clip z stays 0 (always inside the volume).
    const float d = camera_distance_, ox = camera_x_, oy = camera_y_;
    Mat4 p{};
    p.m[0] = 1.0f / 960.0f;                    // x -> clip x
    p.m[8] = -(ox / 960.0f - 1.0f) / d;        // z -> clip x
    p.m[12] = -1.0f;
    p.m[5] = -1.0f / 540.0f;                   // y -> clip y
    p.m[9] = -(1.0f - oy / 540.0f) / d;        // z -> clip y
    p.m[13] = 1.0f;
    p.m[11] = -1.0f / d;                       // z -> w
    p.m[15] = 1.0f;
    projection_ = p;
    glViewport(0, 0, surface_width, surface_height);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glUseProgram(program_);
    glBindVertexArray(vao_);
    glUniform1i(u_tex_, 0);
    glUniform3f(u_light_, -0.35f, -0.55f, 0.75f);
}

void Gl3D::end() noexcept
{
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glBindVertexArray(0);
}

void Gl3D::draw_range(const Mat4 &model, int first, int count, std::uint32_t texture, float r, float g, float b,
                      float alpha, const CaseOp *reflect) noexcept
{
    const Mat4 mvp = mat4_multiply(projection_, model);
    glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(u_model_, 1, GL_FALSE, model.m);
    glUniform4f(u_color_, r, g, b, alpha);
    glUniform1i(u_textured_, texture ? 1 : 0);
    glUniform1f(u_ambient_, ambient_);
    glUniform1f(u_flip_, flip_);
    glUniform1f(u_shine_, shine_);
    glUniform1f(u_sheen_, sheen_);
    glUniform1f(u_spin_, spin_);
    glUniform4f(u_uvrect_, uvrect_[0], uvrect_[1], uvrect_[2], uvrect_[3]);
    glUniform4f(u_glare_, glare_[0], glare_[1], glare_[2], glare_[3]);
    glUniform2f(u_round_, round_[0], round_[1]);
    glUniform4f(u_spine_, spine_[0], spine_[1], spine_[2], 0.0f);
    glUniform4f(u_band_, band_[0], band_[1], band_[2], band_[3]);
    glUniform4f(u_shade_, shade_[0], shade_[1], shade_[2], shade_[3]);
    glUniform1f(u_film_, film_);
    glUniform3f(u_badge_, badge_[0], badge_[1], badge_[2]);
    if (reflect)
        glUniform2f(u_fade_, reflect->floor_y, reflect->fade_length);
    else
        glUniform2f(u_fade_, 0.0f, 0.0f);
    if (texture)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    glDrawArrays(GL_TRIANGLES, first, count);
}

void Gl3D::draw(const CaseOp &op) noexcept
{
    if (!ready())
        return;
    const float s = op.scale;
    Mat4 place = mat4_multiply(mat4_translate(op.x, op.y, op.z),
                               mat4_multiply(mat4_rotate_z(op.roll), mat4_multiply(mat4_rotate_x(op.pitch), mat4_rotate_y(op.yaw))));
    if (op.reflection)
    {
        // Mirror about the floor line: y' = 2 floor - y. The mirror flips the winding, so cull the other side.
        const Mat4 mirror = mat4_multiply(mat4_translate(0, op.floor_y, 0),
                                          mat4_multiply(mat4_scale(1, -1, 1), mat4_translate(0, -op.floor_y, 0)));
        place = mat4_multiply(mirror, place);
        glFrontFace(GL_CW);
    }
    const float w = op.width * s, h = op.height * s, t = op.depth * s;
    const Mat4 body = mat4_multiply(place, mat4_scale(w, h, t));
    const CaseOp *fade = op.reflection ? &op : nullptr;
    const float a = op.alpha * (op.reflection ? 0.34f : 1.0f);
    // style.css puts .dim (the side cases' darker film) on .f-front only: spines, sides and back stay as they are. The
    // faces are flat CSS planes, so no lighting either (ambient 1).
    const float k = 1.0f, kf = op.shade;
    ambient_ = 1.0f;
    if (op.part != 2)
    {
        // The back wall and the sides first, then the tray inside.
        flip_ = op.flip_back ? 1.0f : 0.0f;
        glare_[2] = h / w;
        round_[0] = op.corner;
        round_[1] = -op.corner_side;
        for (int c = 0; c < 4; ++c)
            uvrect_[c] = op.back_uv[c];
        film_ = op.reflection ? 0.0f : op.film; // .f-back::after carries the same film
        if (op.back)
            draw_range(body, box_back_, 6, op.back, k, k, k, a, fade);
        else
            draw_range(body, box_back_, 6, 0, op.side_r * k, op.side_g * k, op.side_b * k, a, fade);
        flip_ = 0.0f;
        round_[0] = round_[1] = 0.0f;
        film_ = 0.0f;
        uvrect_[0] = uvrect_[1] = 0.0f;
        uvrect_[2] = uvrect_[3] = 1.0f;
        if (op.spine)
        {
            // Sides 12..17 are the left edge, 18..23 the right: the spine picture replaces them.
            const int skip = op.spine_both ? 12 : 6;
            draw_range(body, box_sides_ + skip, box_sides_count_ - skip, 0, op.side_r * k, op.side_g * k, op.side_b * k, a,
                       fade);
            for (int c = 0; c < 4; ++c)
                uvrect_[c] = op.spine_uv[c];
            flip_ = 1.0f;
            spine_[0] = 1.0f, spine_[1] = op.spine_band_h, spine_[2] = 0.0f;
            for (int c = 0; c < 4; ++c)
                band_[c] = op.spine_band[c], shade_[c] = op.spine_shade[c];
            draw_range(body, spine_first_, 6, op.spine, k, k, k, a, fade);
            if (op.spine_both)
            {
                spine_[2] = 1.0f;
                draw_range(body, spine_right_first_, 6, op.spine, k, k, k, a, fade);
            }
            spine_[0] = spine_[1] = spine_[2] = 0.0f;
            flip_ = 0.0f;
            uvrect_[0] = uvrect_[1] = 0.0f;
            uvrect_[2] = uvrect_[3] = 1.0f;
        }
        else
            draw_range(body, box_sides_, box_sides_count_, 0, op.side_r * k, op.side_g * k, op.side_b * k, a, fade);
        if (op.opening > 0.0f)
            draw_range(body, cover_first_, cover_count_, 0, 0.045f * k, 0.05f * k, 0.065f * k, a, fade);
    }
    if (op.part != 1)
    {
        // The front is a door on the left hinge once the case opens.
        Mat4 door = body;
        if (op.opening > 0.0f)
        {
            const Mat4 hinge = mat4_multiply(mat4_translate(-0.5f, 0, 0.5f),
                                             mat4_multiply(mat4_rotate_y(-op.opening * 1.75f), mat4_translate(0.5f, 0, -0.5f)));
            door = mat4_multiply(place, mat4_multiply(mat4_scale(w, h, t), hinge));
        }
        shine_ = op.reflection ? 0.0f : op.shine;
        glare_[0] = op.glare_pos;
        glare_[1] = op.reflection ? 0.0f : op.glare;
        glare_[2] = h / w;
        glare_[3] = op.reflection ? -9.0f : op.sheen_pos;
        round_[0] = op.corner;
        round_[1] = op.corner_side;
        film_ = op.reflection ? 0.0f : op.film;
        badge_[0] = op.favorite && !op.reflection ? 1.0f : 0.0f, badge_[1] = w, badge_[2] = h;
        if (op.front)
            draw_range(door, box_front_, 6, op.front, kf, kf, kf, a, fade);
        else
            draw_range(door, box_front_, 6, 0, op.side_r * kf * 1.6f, op.side_g * kf * 1.6f, op.side_b * kf * 1.6f, a,
                       fade);
    }
    shine_ = 0.0f;
    glare_[0] = 0.5f, glare_[1] = 0.0f, glare_[2] = 1.0f, glare_[3] = -9.0f;
    round_[0] = round_[1] = 0.0f;
    film_ = 0.0f;
    badge_[0] = 0.0f;
    ambient_ = 0.62f;
    if (op.reflection)
        glFrontFace(GL_CCW);
}

void Gl3D::draw(const DiscOp &op) noexcept
{
    if (!ready() || disc_count_ == 0)
        return;
    const Mat4 place = mat4_multiply(mat4_translate(op.x, op.y, op.z), mat4_rotate_y(op.yaw));
    const Mat4 turn = mat4_multiply(place, mat4_rotate_z(op.spin));
    auto disc = [&](float radius, std::uint32_t texture, float r, float g, float b, float zoffset)
    {
        const Mat4 model = mat4_multiply(turn, mat4_multiply(mat4_translate(0, 0, zoffset), mat4_scale(radius * 2.0f, radius * 2.0f, 1.0f)));
        draw_range(model, disc_first_, disc_count_, texture, r, g, b, op.alpha, nullptr);
    };
    const float R = op.radius;
    sheen_ = op.sheen;
    spin_ = op.spin;
    disc(R, 0, op.tint_r, op.tint_g, op.tint_b, 0.0f);              // silver rim
    disc(R * 0.93f, op.label, op.label ? 1.0f : op.tint_r * 0.8f, op.label ? 1.0f : op.tint_g * 0.8f,
         op.label ? 1.0f : op.tint_b * 0.8f, 0.4f); // printed label
    sheen_ = 0.0f;
    disc(R * 0.30f, 0, op.tint_r * 0.9f, op.tint_g * 0.9f, op.tint_b * 0.92f, 0.8f); // clear hub
    disc(R * 0.12f, 0, op.hole_r, op.hole_g, op.hole_b, 1.2f);       // hole
}

// ---- Inspect: the case in your hands (Boxshelf's inspect3d.js) -------------------------------------------------
// An open case seen from any side needs real depth, so this pass has its own target with a depth buffer (4x MSAA,
// resolved into a texture the 2D layer then shows over the dimmed shelf).

bool Gl3D::ensure_inspect_target(int width, int height) noexcept
{
    if (isp_fbo_ && isp_w_ == width && isp_h_ == height)
        return true;
    if (!ready())
        return false;
    isp_w_ = width, isp_h_ = height;
    if (!isp_tex_)
        glGenTextures(1, &isp_tex_);
    glBindTexture(GL_TEXTURE_2D, isp_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    if (!isp_resolve_)
        glGenFramebuffers(1, &isp_resolve_);
    glBindFramebuffer(GL_FRAMEBUFFER, isp_resolve_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, isp_tex_, 0);
    GLint max_samples = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
    const int samples = std::max(1, std::min(4, static_cast<int>(max_samples)));
    if (!isp_color_)
        glGenRenderbuffers(1, &isp_color_);
    glBindRenderbuffer(GL_RENDERBUFFER, isp_color_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
    if (!isp_depth_)
        glGenRenderbuffers(1, &isp_depth_);
    glBindRenderbuffer(GL_RENDERBUFFER, isp_depth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
    if (!isp_fbo_)
        glGenFramebuffers(1, &isp_fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, isp_fbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, isp_color_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, isp_depth_);
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
    if (!ok)
    {
        hui::sys::log("[BOXSHELF] inspect target %dx%d incomplete", width, height);
        isp_w_ = isp_h_ = 0;
    }
    return ok;
}

void Gl3D::draw(const InspectOp &op, int surface_width, int surface_height) noexcept
{
    if (!ready() || !isp_fbo_)
        return;
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    glBindFramebuffer(GL_FRAMEBUFFER, isp_fbo_);
    glViewport(0, 0, isp_w_, isp_h_);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepthf(1.0f);
    glDepthMask(GL_TRUE); // glClear only clears the depth buffer while depth writes are on (the pass ends with them off)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // inspect3d.js: a 30 degree camera 3.9 case heights away, looking at the middle of the screen
    const float d = op.height * 3.9f, ox = 960.0f, oy = 540.0f;
    Mat4 p{};
    p.m[0] = 1.0f / 960.0f;
    p.m[8] = -(ox / 960.0f - 1.0f) / d;
    p.m[12] = -1.0f;
    p.m[5] = -1.0f / 540.0f;
    p.m[9] = -(1.0f - oy / 540.0f) / d;
    p.m[13] = 1.0f;
    p.m[10] = -1.0f / (d * 0.9f); // depth: nearer (larger z) is smaller, monotonic in front of the camera
    p.m[11] = -1.0f / d;
    p.m[15] = 1.0f;
    projection_ = p;
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE); // thin plates are seen from both sides
    glUseProgram(program_);
    glBindVertexArray(vao_);
    glUniform1i(u_tex_, 0);
    // lit like the room: a key light from the upper right in front, the rest ambient (three.js key + fill + rim)
    glUniform3f(u_light_, 0.45f, -0.55f, 0.70f);
    ambient_ = 0.58f;

    const float H = op.height, w = H * op.ratio;
    const float lt = H * std::max(.014f, op.depth * op.ratio * .18f);  // lid thickness
    const float tb = H * .012f;                                       // back plate
    const float D = H * std::max(.045f, op.depth * op.ratio * .55f);  // tray depth
    const float wall = H * .011f;
    const float R = w * op.media_size * .5f;                          // disc radius
    const float lift = H * op.disc_lift;                              // GameCube: the hub sits above the middle
    const float o = op.out, deg = 3.14159265f / 180.0f;
    const Mat4 scene = mat4_multiply(mat4_translate(960.0f, 540.0f + op.offset_y, 0.0f),
                                     mat4_multiply(mat4_scale(op.scale * op.zoom, op.scale * op.zoom, op.scale * op.zoom),
                                                   mat4_multiply(mat4_rotate_x(-op.rx * deg), mat4_rotate_y(op.ry * deg))));
    // disc out: the case slides away to the left and turns 30 degrees (three.js caseG)
    const Mat4 caseM = mat4_multiply(scene, mat4_multiply(mat4_translate(-2.6f * H * o, .3f * H * o, -.6f * H * o),
                                                          mat4_rotate_y(-30.0f * deg * o)));
    const float a = op.alpha;
    auto box = [&](const Mat4 &m, float cx, float cy, float cz, float sx, float sy, float sz, float r, float g, float b)
    {
        const Mat4 mm = mat4_multiply(m, mat4_multiply(mat4_translate(cx, cy, cz), mat4_scale(sx, sy, sz)));
        draw_range(mm, box_front_, 6, 0, r, g, b, a, nullptr);
        draw_range(mm, box_back_, 6, 0, r * .9f, g * .9f, b * .9f, a, nullptr);
        draw_range(mm, box_sides_, box_sides_count_, 0, r * .85f, g * .85f, b * .85f, a, nullptr);
    };
    // a face: the unit box's front quad (z = +.5) at z, facing +z (or -z with `back`)
    auto face = [&](const Mat4 &m, float cx, float cy, float cz, float sx, float sy, std::uint32_t tex, float r, float g,
                    float b, bool back, bool flip = false)
    {
        Mat4 mm = mat4_multiply(m, mat4_translate(cx, cy, cz));
        if (back)
            mm = mat4_multiply(mm, mat4_rotate_y(3.14159265f));
        mm = mat4_multiply(mm, mat4_multiply(mat4_scale(sx, sy, 1.0f), mat4_translate(0, 0, -.5f)));
        flip_ = flip ? 1.0f : 0.0f;
        draw_range(mm, box_front_, 6, tex, r, g, b, a, nullptr);
        flip_ = 0.0f;
    };
    auto circle = [&](const Mat4 &m, float cx, float cy, float cz, float radius, std::uint32_t tex, float r, float g,
                      float b, float alpha, bool back)
    {
        Mat4 mm = mat4_multiply(m, mat4_translate(cx, cy, cz));
        if (back)
            mm = mat4_multiply(mm, mat4_rotate_y(3.14159265f));
        mm = mat4_multiply(mm, mat4_scale(radius * 2.0f, radius * 2.0f, 1.0f));
        draw_range(mm, disc_first_, disc_count_, tex, r, g, b, a * alpha, nullptr);
    };
    const float sr = op.side_r, sg = op.side_g, sb = op.side_b;

    // ---- the tray: back plate (its outside printed with the back), the walls, the moulded inside with the hub
    if (o < .995f)
    {
        const float zb = -D * .5f + tb * .5f;
        box(caseM, 0, 0, zb, w, H, tb, sr, sg, sb);
        if (op.back)
        {
            for (int c = 0; c < 4; ++c)
                uvrect_[c] = op.back_uv[c];
            face(caseM, 0, 0, -D * .5f - .6f, w - 2.0f, H - 2.0f, op.back, 1, 1, 1, true, op.flip_back);
            uvrect_[0] = uvrect_[1] = 0.0f, uvrect_[2] = uvrect_[3] = 1.0f;
        }
        const float floor = -D * .5f + tb;
        face(caseM, 0, 0, floor + .3f, w - 2.0f * wall, H - 2.0f * wall, 0, op.inner_r, op.inner_g, op.inner_b, false);
        const float cyl = -lift;
        if (op.cart)
        {
            // the cartridge's cradle: a recess a touch bigger than the cartridge (interiorTexture, kind cart)
            const float cw = R * 2.0f * .9f * 1.06f;
            face(caseM, 0, cyl, floor + .6f, cw, cw * 1.1f, 0, op.inner_r * .78f, op.inner_g * .78f, op.inner_b * .78f, false);
        }
        else
        {
            // the cradle the disc sits in (a shallow darker round) and the hub: a collar and the push button
            circle(caseM, 0, cyl, floor + .6f, R * 1.035f, 0, op.inner_r * .84f, op.inner_g * .84f, op.inner_b * .84f, 1.0f,
                   false);
            const float hr = R * .135f;
            circle(caseM, 0, cyl, floor + 1.5f, hr * 1.55f, 0, op.inner_r * 1.12f + .03f, op.inner_g * 1.12f + .03f,
                   op.inner_b * 1.12f + .03f, 1.0f, false);
            circle(caseM, 0, cyl, floor + 3.0f, hr * .48f, 0, op.inner_r * .7f, op.inner_g * .7f, op.inner_b * .7f, 1.0f, false);
        }
        // walls: one rounded ring in Boxshelf; four slabs here, standing on the back plate up to the lid
        const float zw = tb * .5f, Dw = D - tb;
        box(caseM, -w * .5f + wall * .5f, 0, zw, wall, H, Dw, sr, sg, sb);
        box(caseM, w * .5f - wall * .5f, 0, zw, wall, H, Dw, sr, sg, sb);
        box(caseM, 0, -H * .5f + wall * .5f, zw, w - 2.0f * wall, wall, Dw, sr, sg, sb);
        box(caseM, 0, H * .5f - wall * .5f, zw, w - 2.0f * wall, wall, Dw, sr, sg, sb);
        // the printed spine on the hinge side, the whole thickness of tray and lid
        if (op.spine)
        {
            const float sh = H - H * op.ratio * op.corner * 1.1f, sd = D + lt - .4f;
            const Mat4 sm = mat4_multiply(caseM, mat4_multiply(mat4_translate(-.8f, 0, lt * .5f), mat4_scale(w, sh, sd)));
            for (int c = 0; c < 4; ++c)
                uvrect_[c] = op.spine_uv[c];
            spine_[0] = 1.0f, spine_[1] = op.spine_band_h, spine_[2] = 0.0f;
            for (int c = 0; c < 4; ++c)
                band_[c] = op.spine_band[c], shade_[c] = op.spine_shade[c];
            flip_ = 1.0f;
            draw_range(sm, spine_first_, 6, op.spine, 1, 1, 1, a, nullptr);
            flip_ = 0.0f;
            spine_[0] = spine_[1] = 0.0f;
            uvrect_[0] = uvrect_[1] = 0.0f, uvrect_[2] = uvrect_[3] = 1.0f;
        }

        // ---- the lid on its hinge at the spine edge, opening to 165 degrees
        const Mat4 lidM = mat4_multiply(caseM, mat4_multiply(mat4_translate(-w * .5f, 0, D * .5f + lt * .5f),
                                                             mat4_rotate_y(-165.0f * deg * op.lid)));
        box(lidM, w * .5f, 0, 0, w, H, lt, sr, sg, sb);
        // the printed cover (it carries the case's own border, as on the shelf)
        shine_ = .18f;
        face(lidM, w * .5f, 0, lt * .5f + .4f, w - 1.0f, H - 1.0f, op.front, op.front ? 1.0f : op.case_r,
             op.front ? 1.0f : op.case_g, op.front ? 1.0f : op.case_b, false);
        shine_ = 0.0f;
        // inside: the plastic, the booklet in its two clips (a blank sheet when there is no manual)
        face(lidM, w * .5f, 0, -lt * .5f - .4f, w - 4.0f, H - 4.0f, 0, op.inner_r, op.inner_g, op.inner_b, true);
        face(lidM, w * .5f + H * .01f, 0, -lt * .5f - 2.0f, w * .8f, H * .86f, 0, 232 / 255.f, 228 / 255.f, 220 / 255.f, true);
        for (float cy : {-.34f, .34f})
            box(lidM, w * .5f - .4f * w + w * .035f, cy * H, -lt * .5f - 6.0f, w * .07f, H * .09f, 6.0f, op.case_r,
                op.case_g, op.case_b);
    }

    // ---- the disc: seated on the hub, then out in front of you (bigger, spinning slowly when left alone)
    if (op.lid > .02f || o > 0.0f)
    {
        const float TH = H * .0045f, floor = -D * .5f + tb;
        const float zIn = floor + 4.0f + TH * .5f;
        // the disc keeps the case's turn while seated; out, it squares up to face you
        const Mat4 turn = mat4_multiply(mat4_translate(960.0f, 540.0f + op.offset_y, 0.0f),
                                        mat4_multiply(mat4_scale(op.scale * op.zoom, op.scale * op.zoom, op.scale * op.zoom),
                                                      mat4_multiply(mat4_rotate_x(-op.rx * deg * (1.0f - o)),
                                                                    mat4_rotate_y(op.ry * deg * (1.0f - o)))));
        const float ms = 1.0f + (op.cart ? .9f : .55f) * o;
        const Mat4 discM = mat4_multiply(turn, mat4_multiply(mat4_translate(0, -lift * (1.0f - o), zIn * (1.0f - o) + .25f * H * o),
                                                             mat4_multiply(mat4_scale(ms, ms, ms),
                                                                           mat4_multiply(mat4_rotate_y(op.disc_turn * deg * o),
                                                                                         mat4_rotate_z(op.spin)))));
        if (op.cart)
        {
            // inspect3d.js cart: a dark rounded shell, the label on its face, gold contacts along the bottom
            const float cw = R * 2.0f * .9f, ch = cw * 1.1f, cd = H * .035f;
            const Mat4 cartM = mat4_multiply(turn, mat4_multiply(mat4_translate(0, -lift * (1.0f - o), (floor + cd * .5f + 2.0f) * (1.0f - o) + .25f * H * o),
                                                                 mat4_multiply(mat4_scale(ms, ms, ms), mat4_rotate_y(op.disc_turn * deg * o))));
            box(cartM, 0, 0, 0, cw, ch, cd, 36 / 255.f, 37 / 255.f, 43 / 255.f);
            const float ex = op.edge, ey = op.edge * op.ratio;
            uvrect_[0] = ex, uvrect_[1] = ey, uvrect_[2] = 1.0f - ex, uvrect_[3] = 1.0f - ey;
            face(cartM, 0, -ch * .06f, cd * .5f + .3f, cw * .86f, ch * .74f, op.front, 1, 1, 1, false);
            uvrect_[0] = uvrect_[1] = 0.0f, uvrect_[2] = uvrect_[3] = 1.0f;
            box(cartM, 0, ch * .5f - ch * .08f, 0, cw * .7f, ch * .12f, cd + 2.0f, 201 / 255.f, 162 / 255.f, 74 / 255.f);
        }
        else
        // printed side: the label (the cover's art, inside the case-coloured border the shelf picture carries), the clear
        // hub ring, the hole
        {
            const float ex = op.edge, ey = op.edge * op.ratio;
            uvrect_[0] = ex, uvrect_[1] = ey, uvrect_[2] = 1.0f - ex, uvrect_[3] = 1.0f - ey;
            circle(discM, 0, 0, TH * .5f, R, op.front, op.front ? 1.0f : .8f, op.front ? 1.0f : .82f, op.front ? 1.0f : .88f,
                   1.0f, false);
            uvrect_[0] = uvrect_[1] = 0.0f, uvrect_[2] = uvrect_[3] = 1.0f;
        }
        circle(discM, 0, 0, TH * .5f + .3f, R * .22f, 0, .78f, .8f, .86f, .85f, false);
        circle(discM, 0, 0, TH * .5f + .6f, R * .14f, 0, op.inner_r * .5f, op.inner_g * .5f, op.inner_b * .5f, 1.0f, false);
        // data side: silver with the bands of a real disc
        if (!op.cart)
        {
        sheen_ = .5f, spin_ = op.spin;
        circle(discM, 0, 0, -TH * .5f, R * .985f, 0, .82f, .84f, .9f, 1.0f, true);
        sheen_ = 0.0f;
        circle(discM, 0, 0, -TH * .5f - .3f, R * .14f, 0, op.inner_r * .5f, op.inner_g * .5f, op.inner_b * .5f, 1.0f, true);
        }
    }

    ambient_ = .62f;
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    // resolve the samples into the texture
    glBindFramebuffer(GL_READ_FRAMEBUFFER, isp_fbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, isp_resolve_);
    glBlitFramebuffer(0, 0, isp_w_, isp_h_, 0, 0, isp_w_, isp_h_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
    glViewport(0, 0, surface_width, surface_height);
    glBindVertexArray(0);
}
} // namespace boxshelf
