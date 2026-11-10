// Boxshelf PS5 - loads box art in the background: download (Elf Arsenal), decode (stb), crop to the system's cover
// shape, or compose a square icon into the system's cover style. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "data/systems.hpp"

#include <pthread.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace boxshelf::app
{
struct CoverRequest
{
    std::string key;  // game id
    std::string path; // console file
    data::System system = data::System::count;
    bool square = false; // an icon: composed into the style (covers.js iconFront)
    int width = 0, height = 0;
    std::string fallback; // tried when `path` cannot be read
    bool fallback_square = false;
};

struct CoverPixels
{
    std::string key;
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels; // 0xAABBGGRR, top row first
    bool failed = false;
};

// Composes a square icon (RGBA, edge x edge) into a width x height cover in the system's style: dark sleeve tinted
// by the icon's average colour, a glow behind it, the system band on top, the icon at 56 % of the width centred at
// 42 % of the space under the band (covers.js iconFront, without the title line). Pure: tested on the PC.
void compose_icon_cover(const std::uint32_t *icon, int edge, data::System system, int width, int height,
                        std::uint32_t *out);

// The case's front as Boxshelf draws it (style.css .f-front): the plastic frame `edge` wide in the case colour (no
// frame on the spine side of a wrap-around cover), the picture inside it, and the clear plastic's fixed shading:
// a soft rim light along the top and a faint darkening at the far edges (.f-front::after). `art` is inner_w x
// inner_h (see frame_inner()); out is width x height.
void frame_cover(data::System system, const std::uint32_t *art, int inner_w, int inner_h, std::uint32_t *out,
                 int width, int height);
// The picture area inside the frame for a face of width x height.
void frame_inner(data::System system, int width, int height, int &x, int &y, int &inner_w, int &inner_h);
// Square art cropped to fill a face (no band, no blur): used when no box art exists (Switch icons, 3DS icons).
void fill_square(const std::uint32_t *icon, int edge, int width, int height, std::uint32_t *out);

class CoverLoader
{
  public:
    CoverLoader();
    ~CoverLoader();
    // The covers wanted now, most important first; replaces the previous wish list.
    void want(std::vector<CoverRequest> requests);
    // A cover dropped from memory may be asked for again.
    void forget(const std::string &key);
    // Finished covers since the last call.
    std::vector<std::unique_ptr<CoverPixels>> take();

  private:
    static void *run(void *self);
    void work();
    bool next(CoverRequest &out);

    pthread_mutex_t lock_;
    pthread_cond_t wake_;
    std::vector<CoverRequest> wanted_;
    std::vector<std::string> started_; // keys already done or in flight
    std::vector<std::unique_ptr<CoverPixels>> done_;
    bool thread_ = false;
};
} // namespace boxshelf::app
