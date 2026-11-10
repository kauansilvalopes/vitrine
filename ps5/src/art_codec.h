/* Boxshelf PS5 - downloaded box art -> a small JPEG for the console's cache. SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Decodes a PNG / JPEG (up to 4096 px a side), shrinks it (box filter) so its long side is at most max_side,
 * flattens transparency on dark grey and encodes a JPEG (quality 90) through write(). 1 on success. */
int art_shrink_to_jpeg(const unsigned char *bytes, size_t length, int max_side,
                       void (*write)(void *user, void *data, int size), void *user);
/* The back of a box from a full cover scan (back | spine | front), as Boxshelf cuts it (img-tools.js spine(), part
 * 'back'): the spine's folds are looked for as the strongest vertical lines near where the case's shape puts them
 * (front = front width / height, spine = spine width / height), and the panel left of the spine is encoded like
 * art_shrink_to_jpeg. 0 when the picture is not such a wrap. */
int art_back_to_jpeg(const unsigned char *bytes, size_t length, float front, float spine, int max_side,
                     void (*write)(void *user, void *data, int size), void *user);
#ifdef __cplusplus
}
#endif
