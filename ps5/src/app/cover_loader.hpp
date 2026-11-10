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
    std::string fallback; // a square icon tried when `path` cannot be read
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

// A game whose only art is square (Switch icons, PS5/PS4 icon0): the system band on top, the art at the full width
// of the face right under it, and the rest of the face continuing the art's own colours (its last rows, blurred and
// darkened), so the picture fills the case instead of sitting small in the middle (PS5-PORT.md lesson 2).
void compose_square_cover(const std::uint32_t *icon, int edge, data::System system, int width, int height,
                          std::uint32_t *out);

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
