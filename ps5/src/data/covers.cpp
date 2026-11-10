// Boxshelf PS5 - cover geometry and cache. SPDX-License-Identifier: GPL-3.0-or-later
#include "data/covers.hpp"

#include <cmath>

namespace boxshelf::data
{
CoverSpec cover_spec(const CaseStyle &style, int width) noexcept
{
    const int height = static_cast<int>(std::lround(width / style.ratio));
    const int band = style.has_band ? static_cast<int>(std::lround(height * 0.085)) : 0;
    // Case of width 1 and height 1/ratio; the frame takes `edge` (of the width) on every side.
    const float face_w = 1.0f - 2.0f * style.edge;
    const float face_h = 1.0f / style.ratio - 2.0f * style.edge;
    return {width, height, band, face_w / face_h};
}

std::uint32_t CoverCache::touch(const std::string &key)
{
    const auto it = index_.find(key);
    if (it == index_.end())
        return 0;
    order_.splice(order_.begin(), order_, it->second);
    return it->second->texture;
}

void CoverCache::put(const std::string &key, std::uint32_t texture, std::size_t bytes)
{
    const auto it = index_.find(key);
    if (it != index_.end())
    {
        bytes_ -= it->second->bytes;
        order_.erase(it->second);
        index_.erase(it);
    }
    order_.push_front({key, texture, bytes});
    index_[key] = order_.begin();
    bytes_ += bytes;
}

bool CoverCache::evict(std::uint32_t &texture, std::string &key)
{
    // The newest entry always stays, even alone over the byte budget: the selected case must have its cover.
    if (order_.size() <= 1 || (order_.size() <= max_items_ && bytes_ <= max_bytes_))
        return false;
    const Entry &last = order_.back();
    texture = last.texture;
    key = last.key;
    bytes_ -= last.bytes;
    index_.erase(last.key);
    order_.pop_back();
    return true;
}
} // namespace boxshelf::data
