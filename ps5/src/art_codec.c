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

int art_shrink_to_jpeg(const unsigned char *bytes, size_t length, int max_side,
                       void (*write)(void *user, void *data, int size), void *user)
{
    int w = 0, h = 0, channels = 0;
    if (!bytes || length == 0 || length > (24u << 20) || max_side < 16 || !write)
        return 0;
    if (!stbi_info_from_memory(bytes, (int)length, &w, &h, &channels) || w <= 0 || h <= 0 || w > 4096 || h > 4096)
        return 0;
    unsigned char *rgba = stbi_load_from_memory(bytes, (int)length, &w, &h, &channels, 4);
    if (!rgba)
        return 0;
    int dw = w, dh = h;
    if (w > max_side || h > max_side)
    {
        if (w >= h)
            dw = max_side, dh = (int)((long long)h * max_side / w);
        else
            dh = max_side, dw = (int)((long long)w * max_side / h);
        if (dw < 1)
            dw = 1;
        if (dh < 1)
            dh = 1;
    }
    unsigned char *rgb = (unsigned char *)malloc((size_t)dw * (size_t)dh * 3);
    if (!rgb)
    {
        stbi_image_free(rgba);
        return 0;
    }
    for (int y = 0; y < dh; ++y)
    {
        int y0 = (int)((long long)y * h / dh), y1 = (int)((long long)(y + 1) * h / dh);
        if (y1 <= y0)
            y1 = y0 + 1;
        for (int x = 0; x < dw; ++x)
        {
            int x0 = (int)((long long)x * w / dw), x1 = (int)((long long)(x + 1) * w / dw);
            if (x1 <= x0)
                x1 = x0 + 1;
            unsigned r = 0, g = 0, b = 0, n = 0;
            for (int sy = y0; sy < y1; ++sy)
                for (int sx = x0; sx < x1; ++sx)
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
    stbi_image_free(rgba);
    const int ok = stbi_write_jpg_to_func(write, user, dw, dh, 3, rgb, 90);
    free(rgb);
    return ok != 0;
}
