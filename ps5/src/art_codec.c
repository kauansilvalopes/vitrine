/* Boxshelf PS5 - art codec. stb_image (decode; its implementation lives in cover_decode.c) and stb_image_write
 * (encode) are public domain / MIT. SPDX-License-Identifier: GPL-3.0-or-later */
#include "art_codec.h"

#include <stdlib.h>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_NO_STDIO
#include "../third_party/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../third_party/stb/stb_image_write.h"

/* Box-filters the columns x0 .. x0 + cw of an RGBA picture so its long side is at most max_side, flattens
 * transparency on dark grey and writes a JPEG (quality 90). */
static int encode_region(const unsigned char *rgba, int w, int h, int x0, int cw, int max_side,
                         void (*write)(void *user, void *data, int size), void *user)
{
    int dw = cw, dh = h;
    if (cw > max_side || h > max_side)
    {
        if (cw >= h)
            dw = max_side, dh = (int)((long long)h * max_side / cw);
        else
            dh = max_side, dw = (int)((long long)cw * max_side / h);
        if (dw < 1)
            dw = 1;
        if (dh < 1)
            dh = 1;
    }
    unsigned char *rgb = (unsigned char *)malloc((size_t)dw * (size_t)dh * 3);
    if (!rgb)
        return 0;
    for (int y = 0; y < dh; ++y)
    {
        int y0 = (int)((long long)y * h / dh), y1 = (int)((long long)(y + 1) * h / dh);
        if (y1 <= y0)
            y1 = y0 + 1;
        for (int x = 0; x < dw; ++x)
        {
            int sx0 = x0 + (int)((long long)x * cw / dw), sx1 = x0 + (int)((long long)(x + 1) * cw / dw);
            if (sx1 <= sx0)
                sx1 = sx0 + 1;
            unsigned r = 0, g = 0, b = 0, n = 0;
            for (int sy = y0; sy < y1; ++sy)
                for (int sx = sx0; sx < sx1; ++sx)
                {
                    const unsigned char *p = rgba + ((size_t)sy * (size_t)w + (size_t)sx) * 4;
                    const unsigned a = p[3], inv = 255 - a;
                    r += (p[0] * a + 22 * inv) / 255;
                    g += (p[1] * a + 26 * inv) / 255;
                    b += (p[2] * a + 32 * inv) / 255;
                    ++n;
                }
            unsigned char *o = rgb + ((size_t)y * (size_t)dw + (size_t)x) * 3;
            o[0] = (unsigned char)(r / n);
            o[1] = (unsigned char)(g / n);
            o[2] = (unsigned char)(b / n);
        }
    }
    const int ok = stbi_write_jpg_to_func(write, user, dw, dh, 3, rgb, 90);
    free(rgb);
    return ok != 0;
}

static unsigned char *decode(const unsigned char *bytes, size_t length, int *w, int *h)
{
    int channels = 0;
    if (!bytes || length == 0 || length > (12u << 20))
        return 0;
    // The app heap is 128 MB: a decode takes about 7 bytes a pixel while it runs (the RGBA result and stb's
    // component buffers), so pictures over 6 megapixels are left alone (the next source is tried instead).
    if (!stbi_info_from_memory(bytes, (int)length, w, h, &channels) || *w <= 0 || *h <= 0 || *w > 4096 || *h > 4096 ||
        (long long)*w * *h > 7500000LL)
        return 0;
    return stbi_load_from_memory(bytes, (int)length, w, h, &channels, 4);
}

int art_shrink_to_jpeg(const unsigned char *bytes, size_t length, int max_side,
                       void (*write)(void *user, void *data, int size), void *user)
{
    int w = 0, h = 0;
    if (max_side < 16 || !write)
        return 0;
    unsigned char *rgba = decode(bytes, length, &w, &h);
    if (!rgba)
        return 0;
    const int ok = encode_region(rgba, w, h, 0, w, max_side, write, user);
    stbi_image_free(rgba);
    return ok;
}

int art_back_to_jpeg(const unsigned char *bytes, size_t length, float front, float spine, int max_side,
                     void (*write)(void *user, void *data, int size), void *user)
{
    int W = 0, H = 0;
    if (max_side < 16 || !write || front <= 0.0f || spine <= 0.0f)
        return 0;
    unsigned char *rgba = decode(bytes, length, &W, &H);
    if (!rgba)
        return 0;
    int ok = 0;
    if (W >= H * 1.15f) /* a wrap: back | spine | front */
    {
        float s0 = W - 2.0f * front * H;
        if (!(s0 > spine * H * .5f && s0 < spine * H * 2.0f))
            s0 = spine * H;
        const float L0 = (W - s0) / 2.0f, R0 = (W + s0) / 2.0f;
        /* column edge strength on a copy 400 px tall (sampled), between 8 % and 92 % of the height */
        const float k = H > 400 ? 400.0f / (float)H : 1.0f;
        int w = (int)(W * k + .5f), h = (int)(H * k + .5f);
        if (w < 2)
            w = 2;
        if (h < 2)
            h = 2;
        float *edge = (float *)calloc((size_t)w, sizeof(float));
        if (edge)
        {
            const int y0 = (int)(h * .08f), y1 = (int)(h * .92f + .999f);
            double sum = 0.0;
            for (int x = 1; x < w; ++x)
            {
                const int sx = (int)((long long)x * W / w), px = (int)((long long)(x - 1) * W / w);
                double e = 0.0;
                for (int y = y0; y < y1; ++y)
                {
                    const int sy = (int)((long long)y * H / h);
                    const unsigned char *a = rgba + ((size_t)sy * (size_t)W + (size_t)sx) * 4;
                    const unsigned char *b = rgba + ((size_t)sy * (size_t)W + (size_t)px) * 4;
                    e += abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2]);
                }
                edge[x] = (float)(e / (y1 > y0 ? y1 - y0 : 1));
                sum += edge[x];
            }
            const float avg = (float)(sum / w), r = s0 * k * .3f;
            int lx = -1, rx = -1;
            float lv = 0.0f, rv = 0.0f;
            for (int x = (int)(L0 * k - r + .5f); x <= (int)(L0 * k + r + .5f); ++x)
                if (x >= 1 && x < w && edge[x] > lv)
                    lv = edge[x], lx = x;
            for (int x = (int)(R0 * k - r + .5f); x <= (int)(R0 * k + r + .5f); ++x)
                if (x >= 1 && x < w && edge[x] > rv)
                    rv = edge[x], rx = x;
            float L = L0, R = R0;
            if (lx > 0 && rx > lx && lv > avg * 2.0f && rv > avg * 2.0f)
            {
                const float s1 = (rx - lx) / k;
                if (s1 > s0 * .7f && s1 < s0 * 1.35f)
                    L = lx / k, R = rx / k;
            }
            free(edge);
            const int inset = (int)((R - L) * .03f + .5f); /* stay inside the fold lines */
            const int bw = (int)(L + .5f) - inset;
            /* about a front's width; much narrower means a cropped wrap or not back | spine | front */
            if (bw >= front * H * .75f)
            {
                const float take = bw < front * H * 1.08f ? (float)bw : front * H * 1.08f;
                int x0 = (int)(L - take + .5f);
                if (x0 < 0)
                    x0 = 0;
                const int cw = (int)(L + .5f) - inset - x0;
                if (cw > 16)
                    ok = encode_region(rgba, W, H, x0, cw, max_side, write, user);
            }
        }
    }
    stbi_image_free(rgba);
    return ok;
}
