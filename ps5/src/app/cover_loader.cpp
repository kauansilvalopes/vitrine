// Boxshelf PS5 - cover loader. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/cover_loader.hpp"

#include "app/elf_client.hpp"
#include "cover_decode.h"
#include "data/covers.hpp"
#include "platform/ps5/system.hpp"

#include <algorithm>
#include <cmath>

namespace boxshelf::app
{
namespace
{
struct Rgb
{
    float r, g, b;
};
Rgb unpack(std::uint32_t p)
{
    return {static_cast<float>(p & 0xff), static_cast<float>((p >> 8) & 0xff), static_cast<float>((p >> 16) & 0xff)};
}
std::uint32_t pack(float r, float g, float b)
{
    auto c = [](float v) { return static_cast<std::uint32_t>(std::clamp(v, 0.0f, 255.0f) + 0.5f); };
    return 0xff000000u | (c(b) << 16) | (c(g) << 8) | c(r);
}
} // namespace

void compose_icon_cover(const std::uint32_t *icon, int edge, data::System system, int width, int height,
                        std::uint32_t *out)
{
    const auto &st = data::style(system);
    // average colour of the icon (covers.js samples an 8x8 copy)
    float r = 0, g = 0, b = 0;
    int n = 0;
    for (int y = 0; y < edge; y += std::max(1, edge / 8))
        for (int x = 0; x < edge; x += std::max(1, edge / 8))
        {
            const Rgb c = unpack(icon[y * edge + x]);
            r += c.r, g += c.g, b += c.b, ++n;
        }
    if (n)
        r /= n, g /= n, b /= n;
    else
        r = g = b = 90;
    const int band = st.has_band ? static_cast<int>(std::lround(height * 0.085)) : 0;
    const float gx = width * 0.5f, gy = height * 0.42f, gr = width * 0.6f;
    // sleeve: vertical gradient avg*.35 -> avg*.12, plus radial glow rgba(avg,.45) at (W/2, H*.42) to W*.6
    for (int y = 0; y < height; ++y)
    {
        const float t = static_cast<float>(y) / static_cast<float>(height);
        const float k = 0.35f + (0.12f - 0.35f) * t;
        for (int x = 0; x < width; ++x)
        {
            const float d = std::hypot(x - gx, y - gy);
            const float glow = d < 10.0f ? 0.45f : 0.45f * std::max(0.0f, 1.0f - (d - 10.0f) / (gr - 10.0f));
            out[y * width + x] = pack(r * k * (1 - glow) + r * glow, g * k * (1 - glow) + g * glow,
                                      b * k * (1 - glow) + b * glow);
        }
    }
    // band: style.band.bg across the top (its label needs text: P3 draws it with the spine pass)
    if (band > 0)
    {
        const std::uint32_t c = pack(static_cast<float>((st.band.bg >> 16) & 0xff),
                                     static_cast<float>((st.band.bg >> 8) & 0xff), static_cast<float>(st.band.bg & 0xff));
        for (int y = 0; y < band; ++y)
            for (int x = 0; x < width; ++x)
                out[y * width + x] = c;
    }
    // the icon at 56 % of the width, centred at band + (H - band) * .42, with a soft shadow below
    const int size = static_cast<int>(width * 0.56f);
    const int ix = (width - size) / 2, iy = static_cast<int>(band + (height - band) * 0.42f) - size / 2;
    for (int y = 0; y < size + 24; ++y)
        for (int x = -12; x < size + 12; ++x)
        {
            const int px = ix + x, py = iy + y + 12;
            if (px < 0 || py < 0 || px >= width || py >= height)
                continue;
            const float dx = x < 0 ? -x : (x >= size ? x - size + 1 : 0), dy = y >= size ? y - size + 1 : 0;
            const float a = 0.55f * std::max(0.0f, 1.0f - std::hypot(dx, dy) / 24.0f) * (y < size ? 0.6f : 1.0f);
            const Rgb c = unpack(out[py * width + px]);
            out[py * width + px] = pack(c.r * (1 - a), c.g * (1 - a), c.b * (1 - a));
        }
    for (int y = 0; y < size; ++y)
    {
        const int py = iy + y;
        if (py < 0 || py >= height)
            continue;
        const int sy = y * edge / size;
        for (int x = 0; x < size; ++x)
            out[py * width + ix + x] = icon[sy * edge + x * edge / size];
    }
}

void compose_square_cover(const std::uint32_t *icon, int edge, data::System system, int width, int height,
                          std::uint32_t *out)
{
    const auto &st = data::style(system);
    const int band = st.has_band ? static_cast<int>(std::lround(height * 0.085)) : 0;
    const int art = std::min(width, height - band); // the square, as wide as the face
    const int ax = (width - art) / 2;
    // below the art: a soft version of its bottom rows (8 x 8 grid averaged, stretched), darkening to the bottom
    constexpr int G = 8;
    Rgb grid[G];
    for (int gx = 0; gx < G; ++gx)
    {
        float r = 0, g = 0, b = 0;
        int n = 0;
        for (int y = edge * 3 / 4; y < edge; y += std::max(1, edge / 32))
            for (int x = gx * edge / G; x < (gx + 1) * edge / G; x += std::max(1, edge / 64))
            {
                const Rgb c = unpack(icon[y * edge + x]);
                r += c.r, g += c.g, b += c.b, ++n;
            }
        grid[gx] = n ? Rgb{r / n, g / n, b / n} : Rgb{22, 26, 32};
    }
    const int below = band + art;
    for (int y = below; y < height; ++y)
    {
        const float t = static_cast<float>(y - below) / static_cast<float>(std::max(1, height - below));
        const float k = 0.62f * (1.0f - t) + 0.18f * t;
        for (int x = 0; x < width; ++x)
        {
            const float fx = (static_cast<float>(x) + 0.5f) * G / static_cast<float>(width) - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, G - 1), x1 = std::min(x0 + 1, G - 1);
            const float tx = std::clamp(fx - static_cast<float>(x0), 0.0f, 1.0f);
            const Rgb a = grid[x0], b = grid[x1];
            out[y * width + x] = pack((a.r + (b.r - a.r) * tx) * k, (a.g + (b.g - a.g) * tx) * k, (a.b + (b.b - a.b) * tx) * k);
        }
    }
    // the band
    if (band > 0)
    {
        const std::uint32_t c = pack(static_cast<float>((st.band.bg >> 16) & 0xff),
                                     static_cast<float>((st.band.bg >> 8) & 0xff), static_cast<float>(st.band.bg & 0xff));
        for (int y = 0; y < band; ++y)
            for (int x = 0; x < width; ++x)
                out[y * width + x] = c;
    }
    // the art, scaled with a box filter
    for (int y = 0; y < art; ++y)
    {
        const int sy0 = y * edge / art, sy1 = std::max(sy0 + 1, (y + 1) * edge / art);
        for (int x = 0; x < art; ++x)
        {
            const int sx0 = x * edge / art, sx1 = std::max(sx0 + 1, (x + 1) * edge / art);
            float r = 0, g = 0, b = 0;
            int n = 0;
            for (int sy = sy0; sy < sy1; ++sy)
                for (int sx = sx0; sx < sx1; ++sx)
                {
                    const Rgb c = unpack(icon[sy * edge + sx]);
                    r += c.r, g += c.g, b += c.b, ++n;
                }
            out[(band + y) * width + ax + x] = pack(r / n, g / n, b / n);
        }
    }
    // a narrow side gap (landscape faces): the art's edge colours
    for (int y = band; y < band + art; ++y)
        for (int x = 0; x < width; ++x)
            if (x < ax || x >= ax + art)
            {
                const Rgb c = unpack(out[y * width + (x < ax ? ax : ax + art - 1)]);
                out[y * width + x] = pack(c.r * .5f, c.g * .5f, c.b * .5f);
            }
}

CoverLoader::CoverLoader()
{
    pthread_mutex_init(&lock_, nullptr);
    pthread_cond_init(&wake_, nullptr);
}

CoverLoader::~CoverLoader() = default; // the worker lives as long as the app

void CoverLoader::want(std::vector<CoverRequest> requests)
{
    pthread_mutex_lock(&lock_);
    wanted_ = std::move(requests);
    if (!thread_)
    {
        pthread_t t;
        thread_ = pthread_create(&t, nullptr, &CoverLoader::run, this) == 0;
        if (thread_)
            pthread_detach(t);
    }
    pthread_cond_signal(&wake_);
    pthread_mutex_unlock(&lock_);
}

void CoverLoader::forget(const std::string &key)
{
    pthread_mutex_lock(&lock_);
    started_.erase(std::remove(started_.begin(), started_.end(), key), started_.end());
    pthread_mutex_unlock(&lock_);
}

std::vector<std::unique_ptr<CoverPixels>> CoverLoader::take()
{
    pthread_mutex_lock(&lock_);
    auto out = std::move(done_);
    done_.clear();
    pthread_mutex_unlock(&lock_);
    return out;
}

void *CoverLoader::run(void *self)
{
    static_cast<CoverLoader *>(self)->work();
    return nullptr;
}

bool CoverLoader::next(CoverRequest &out)
{
    for (const auto &r : wanted_)
        if (std::find(started_.begin(), started_.end(), r.key) == started_.end())
        {
            out = r;
            started_.push_back(r.key);
            return true;
        }
    return false;
}

void CoverLoader::work()
{
    for (;;)
    {
        CoverRequest req;
        pthread_mutex_lock(&lock_);
        while (!next(req))
            pthread_cond_wait(&wake_, &lock_);
        pthread_mutex_unlock(&lock_);

        auto result = std::make_unique<CoverPixels>();
        result->key = req.key;
        result->width = req.width;
        result->height = req.height;
        std::string bytes;
        bool got = elf_get(fs_url("/api/fs/download?path=", req.path), bytes, 12u << 20) && !bytes.empty();
        if (!got && !req.fallback.empty())
        {
            got = elf_get(fs_url("/api/fs/download?path=", req.fallback), bytes, 12u << 20) && !bytes.empty();
            req.square = true;
        }
        if (got)
        {
            const auto *data = reinterpret_cast<const unsigned char *>(bytes.data());
            result->pixels.resize(static_cast<std::size_t>(req.width) * static_cast<std::size_t>(req.height));
            if (req.square)
            {
                constexpr int kEdge = 512;
                std::vector<std::uint32_t> icon(kEdge * kEdge);
                if (!decode_cover_rect(data, bytes.size(), icon.data(), kEdge, kEdge))
                    result->failed = true;
                else if (req.system == data::System::media) // apps: covers.js iconFront
                    compose_icon_cover(icon.data(), kEdge, req.system, req.width, req.height, result->pixels.data());
                else
                    compose_square_cover(icon.data(), kEdge, req.system, req.width, req.height, result->pixels.data());
            }
            else if (!decode_cover_rect(data, bytes.size(), result->pixels.data(), static_cast<unsigned>(req.width),
                                        static_cast<unsigned>(req.height)))
                result->failed = true;
        }
        else
            result->failed = true;
        if (result->failed)
        {
            result->pixels.clear();
            hui::sys::log("[BOXSHELF] cover failed: %s", req.path.c_str());
        }
        pthread_mutex_lock(&lock_);
        done_.push_back(std::move(result));
        pthread_mutex_unlock(&lock_);
    }
}
} // namespace boxshelf::app
