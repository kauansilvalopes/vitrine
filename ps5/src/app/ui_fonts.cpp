// Boxshelf PS5 - UI faces. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/ui_fonts.hpp"

#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"

#include <string>

namespace boxshelf::app
{
namespace
{
constexpr int kCount = static_cast<int>(Face::count);
const char *const kFiles[kCount] = {
    "lexend-500", "lexend-600", "lexend-800", "chakrapetch-700", "syne-800",
    "unbounded-700", "michroma-400", "audiowide-400", "dmserifdisplay-400-italic", "bungee-400",
};
hui::gfx::Font fonts[kCount];
UiFont faces[kCount];
} // namespace

bool load_ui_fonts(hui::gfx::GlBatch &batch, const char *dir, const hui::gfx::Font *inter,
                   std::uint32_t inter_texture)
{
    bool all = true;
    for (int i = 0; i < kCount; ++i)
    {
        std::string bytes;
        const std::string path = std::string(dir) + "/" + kFiles[i] + ".huifont";
        if (!hui::save::read_file(path.c_str(), &bytes) || !fonts[i].load(bytes))
        {
            hui::sys::log("[BOXSHELF] font %s failed", path.c_str());
            all = false;
            continue;
        }
        if (inter)
            fonts[i].add_fallback(inter, inter_texture);
        faces[i] = {&fonts[i], batch.create_font_texture(fonts[i])};
    }
    return all;
}

UiFont face(Face f)
{
    const int i = static_cast<int>(f);
    if (i >= 0 && i < kCount && faces[i])
        return faces[i];
    return faces[0];
}
} // namespace boxshelf::app
