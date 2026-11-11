// Boxshelf PS5 - cover loader. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/cover_loader.hpp"
#include "app/store.hpp"

#include "app/art_fetcher.hpp"
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

void frame_inner(data::System system, int width, int height, int &x, int &y, int &inner_w, int &inner_h)
{
    const auto &st = data::style(system);
    // .f-front{padding:var(--edge)} with edge = bw*style.edge; .f-front{padding-left:var(--edge-in)}: 0 when wrap
    const int e = std::max(1, static_cast<int>(std::lround(width * st.edge)));
    const int left = st.wrap ? 0 : e;
    x = left;
    y = e;
    inner_w = std::max(1, width - left - e);
    inner_h = std::max(1, height - 2 * e);
}

void frame_cover(data::System system, const std::uint32_t *art, int inner_w, int inner_h, std::uint32_t *out,
                 int width, int height)
{
    const auto &st = data::style(system);
    int ix, iy, iw, ih;
    frame_inner(system, width, height, ix, iy, iw, ih);
    const float cr = static_cast<float>((st.case_color >> 16) & 0xff), cg = static_cast<float>((st.case_color >> 8) & 0xff),
                cb = static_cast<float>(st.case_color & 0xff);
    // The ::after film (top rim light, darker left and right edges) is drawn by the 3D pass (Gl3D u_film), at full
    // strength on the selected case and .55 on the others, as style.css does.
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const int ax = x - ix, ay = y - iy;
            if (ax >= 0 && ay >= 0 && ax < inner_w && ay < inner_h && ax < iw && ay < ih)
                out[y * width + x] = art[ay * inner_w + ax] | 0xff000000u;
            else
                out[y * width + x] = pack(cr, cg, cb);
        }
}

void fill_square(const std::uint32_t *icon, int edge, int width, int height, std::uint32_t *out)
{
    // object-fit: cover of a square into width x height (centre crop), box-filtered
    const float scale = std::max(static_cast<float>(width), static_cast<float>(height)) / static_cast<float>(edge);
    const float ox = (static_cast<float>(edge) * scale - width) * .5f, oy = (static_cast<float>(edge) * scale - height) * .5f;
    for (int y = 0; y < height; ++y)
    {
        const int sy0 = std::clamp(static_cast<int>((y + oy) / scale), 0, edge - 1);
        const int sy1 = std::clamp(static_cast<int>((y + 1 + oy) / scale), sy0 + 1, edge);
        for (int x = 0; x < width; ++x)
        {
            const int sx0 = std::clamp(static_cast<int>((x + ox) / scale), 0, edge - 1);
            const int sx1 = std::clamp(static_cast<int>((x + 1 + ox) / scale), sx0 + 1, edge);
            float r = 0, g = 0, b = 0;
            int n = 0;
            for (int sy = sy0; sy < sy1; ++sy)
                for (int sx = sx0; sx < sx1; ++sx)
                {
                    const Rgb c = unpack(icon[sy * edge + sx]);
                    r += c.r, g += c.g, b += c.b, ++n;
                }
            out[y * width + x] = pack(r / n, g / n, b / n);
        }
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
        result->path = req.path;
        result->width = req.width;
        result->height = req.height;
        // The console's 3840x2160 key art (pic0.png, up to ~16 MB) needs the large decoder (Estante reads it the
        // same way, one at a time); everything else is a small cover file.
        auto large = [](const std::string &p) { return p.size() > 8 && p.compare(p.size() - 8, 8, "pic0.png") == 0; };
        // The app's own storage (downloaded art) is read directly; everything else through Elf Arsenal.
        auto fetch = [](const std::string &path, std::string &out, std::size_t max)
        {
            const std::string root = storage_root();
            if (path.compare(0, root.size(), root) == 0)
                return store_read(path, out, max) && !out.empty();
            return elf_get(fs_url("/api/fs/download?path=", path), out, max) && !out.empty();
        };
        std::string bytes, used = req.path;
        bool got = fetch(req.path, bytes, large(req.path) ? 32u << 20 : 12u << 20);
        if (!got && !req.fallback.empty())
        {
            used = req.fallback;
            got = fetch(req.fallback, bytes, 12u << 20);
            req.square = req.fallback_square;
        }
        int ix, iy, iw, ih;
        frame_inner(req.system, req.width, req.height, ix, iy, iw, ih);
        std::vector<std::uint32_t> art(static_cast<std::size_t>(iw) * static_cast<std::size_t>(ih));
        bool ok = false;
        if (got)
        {
            const auto *data = reinterpret_cast<const unsigned char *>(bytes.data());
            if (large(used))
                ok = decode_cover_rect_large(data, bytes.size(), art.data(), static_cast<unsigned>(iw),
                                             static_cast<unsigned>(ih));
            else if (req.square)
            {
                constexpr int kEdge = 512;
                std::vector<std::uint32_t> icon(kEdge * kEdge);
                ok = decode_cover_rect(data, bytes.size(), icon.data(), kEdge, kEdge);
                if (ok && req.system == data::System::media) // apps: covers.js iconFront
                    compose_icon_cover(icon.data(), kEdge, req.system, iw, ih, art.data());
                else if (ok) // a game with only square art: it fills the face (lesson 2: never small)
                    fill_square(icon.data(), kEdge, iw, ih, art.data());
            }
            else
                ok = decode_cover_rect(data, bytes.size(), art.data(), static_cast<unsigned>(iw), static_cast<unsigned>(ih));
        }
        if (ok)
        {
            result->pixels.resize(static_cast<std::size_t>(req.width) * static_cast<std::size_t>(req.height));
            frame_cover(req.system, art.data(), iw, ih, result->pixels.data(), req.width, req.height);
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
