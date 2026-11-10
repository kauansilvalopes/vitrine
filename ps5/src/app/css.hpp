// Boxshelf PS5 - the bits of CSS / Boxshelf JS the port reproduces exactly. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gfx/draw_list.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace boxshelf::app
{
// covers.js `hsl(h, s, l, a)` -> colour (h in degrees, any range; s, l in percent).
inline hui::gfx::Color hsl(float h, float s, float l, float a = 1.0f)
{
    h = std::fmod(std::fmod(h, 360.0f) + 360.0f, 360.0f) / 360.0f;
    s /= 100.0f;
    l /= 100.0f;
    auto channel = [&](float n)
    {
        const float k = std::fmod(n + h * 12.0f, 12.0f);
        const float a2 = s * std::fmin(l, 1.0f - l);
        return l - a2 * std::max(-1.0f, std::min({k - 3.0f, 9.0f - k, 1.0f}));
    };
    return {channel(0.0f), channel(8.0f), channel(4.0f), a};
}

inline hui::gfx::Color rgb(std::uint32_t hex, float alpha = 1.0f)
{
    return hui::gfx::Color::rgb(hex, alpha);
}

// covers.js `hash(s)`: FNV-1a over the string's UTF-16 code units (JS charCodeAt), unsigned 32-bit.
inline std::uint32_t title_hash(std::string_view utf8)
{
    std::uint32_t h = 2166136261u;
    auto mix = [&](std::uint32_t unit) { h = (h ^ unit) * 16777619u; };
    for (std::size_t i = 0; i < utf8.size();)
    {
        const auto c = static_cast<unsigned char>(utf8[i]);
        std::uint32_t cp = c;
        int extra = 0;
        if (c >= 0xf0)
            cp = c & 0x07, extra = 3;
        else if (c >= 0xe0)
            cp = c & 0x0f, extra = 2;
        else if (c >= 0xc0)
            cp = c & 0x1f, extra = 1;
        ++i;
        for (int k = 0; k < extra && i < utf8.size(); ++k, ++i)
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i]) & 0x3f);
        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            mix(0xd800 + (cp >> 10));
            mix(0xdc00 + (cp & 0x3ff));
        }
        else
            mix(cp);
    }
    return h;
}

// CSS cubic-bezier(x1, y1, x2, y2) timing function.
struct Bezier
{
    float x1, y1, x2, y2;
    float operator()(float t) const
    {
        if (t <= 0.0f)
            return 0.0f;
        if (t >= 1.0f)
            return 1.0f;
        auto bx = [&](float u) { return ((1 - 3 * x2 + 3 * x1) * u + (3 * x2 - 6 * x1)) * u * u + 3 * x1 * u; };
        auto by = [&](float u) { return ((1 - 3 * y2 + 3 * y1) * u + (3 * y2 - 6 * y1)) * u * u + 3 * y1 * u; };
        auto dx = [&](float u) { return 3 * (1 - 3 * x2 + 3 * x1) * u * u + 2 * (3 * x2 - 6 * x1) * u + 3 * x1; };
        float u = t;
        for (int i = 0; i < 8; ++i) // Newton, then bisection if the slope is flat
        {
            const float e = bx(u) - t, d = dx(u);
            if (std::fabs(e) < 1e-5f)
                return by(u);
            if (std::fabs(d) < 1e-6f)
                break;
            u -= e / d;
        }
        float lo = 0.0f, hi = 1.0f;
        u = t;
        for (int i = 0; i < 30; ++i)
        {
            const float x = bx(u);
            if (std::fabs(x - t) < 1e-5f)
                break;
            (x < t ? lo : hi) = u;
            u = 0.5f * (lo + hi);
        }
        return by(u);
    }
};
// The shelf's slide: style.css `.slot{transition:transform .55s cubic-bezier(.22,.9,.25,1)}`.
constexpr Bezier kSlide{0.22f, 0.9f, 0.25f, 1.0f};
// The tab pill: `.tab-pill i{transition:transform .45s cubic-bezier(.3,.9,.25,1)}`.
constexpr Bezier kPill{0.3f, 0.9f, 0.25f, 1.0f};

// A value that glides to each new target along a CSS timing function (transition semantics: a new target
// starts from wherever the value is now).
struct Glide
{
    float from = 0.0f, to = 0.0f;
    double start = -1.0; // < 0: at rest on `to`
    float duration = 0.55f;
    Bezier ease = kSlide;
    bool placed = false; // the first target is taken at once (an element appears in place)

    float at(double now) const
    {
        if (start < 0.0)
            return to;
        const float t = static_cast<float>((now - start) / duration);
        return from + (to - from) * ease(t);
    }
    void set(float target, double now, bool instant = false)
    {
        if (instant || !placed)
        {
            from = to = target;
            start = -1.0;
            placed = true;
            return;
        }
        if (target == to)
            return;
        from = at(now);
        to = target;
        start = now;
    }
    bool moving(double now) const
    {
        return start >= 0.0 && now - start < duration;
    }
};
} // namespace boxshelf::app
