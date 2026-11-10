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
#ifdef __cplusplus
}
#endif
