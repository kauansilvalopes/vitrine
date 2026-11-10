// Boxshelf PS5 - cover geometry per system and the cover texture cache. SPDX-License-Identifier: GPL-3.0-or-later
//
// Geometry from Boxshelf renderer/covers.js (drawFront: W=420, H=round(W/ratio), band = round(H*.085)) and
// renderer/style.css (.f-front: the cover sits inside a frame `edge` wide, edge being a fraction of the case width).
#pragma once

#include "data/systems.hpp"

#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>

namespace boxshelf::data
{
struct CoverSpec
{
    int width, height; // the picture the case face shows (covers.js: the whole case ratio)
    int band_height;   // the system band across the top of a composed cover (0 when the style has none)
    float face_aspect; // width / height of the face inside the frame: what real art is cropped to
};
// For a given picture width (covers.js uses 420; the console can ask for more on the selected case).
CoverSpec cover_spec(const CaseStyle &style, int width = 420) noexcept;

// Least-recently-used set of decoded covers kept as GPU textures. Bounded by count and by bytes: the PS5 has
// little texture memory for a homebrew title (Estante keeps about 24 covers). The cache only does the
// bookkeeping; the caller frees what evict() hands back.
class CoverCache
{
  public:
    CoverCache(std::size_t max_items, std::size_t max_bytes) noexcept : max_items_(max_items), max_bytes_(max_bytes)
    {
    }
    // Marks a cover as used now. Returns its texture, or 0 when it is not cached.
    std::uint32_t touch(const std::string &key);
    // Adds a decoded cover. Afterwards the caller must drain evict() until it returns false.
    void put(const std::string &key, std::uint32_t texture, std::size_t bytes);
    // Oldest entry over the limits: its texture (to free) and key. False when within limits.
    bool evict(std::uint32_t &texture, std::string &key);
    std::size_t size() const noexcept
    {
        return order_.size();
    }
    std::size_t bytes() const noexcept
    {
        return bytes_;
    }

  private:
    struct Entry
    {
        std::string key;
        std::uint32_t texture;
        std::size_t bytes;
    };
    std::size_t max_items_, max_bytes_, bytes_ = 0;
    std::list<Entry> order_; // front = most recent
    std::unordered_map<std::string, std::list<Entry>::iterator> index_;
};
} // namespace boxshelf::data
