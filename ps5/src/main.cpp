// Boxshelf PS5 - console entry point. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/art_fetcher.hpp"
#include "app/clock.hpp"
#include "app/i18n.hpp"
#include "app/library_loader.hpp"
#include "app/scene.hpp"
#include "app/settings.hpp"
#include "core/input.hpp"
#include "gl_main.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/system.hpp"

#include <array>
#include <cstdio>
#include <ctime>

namespace
{
std::int64_t start_us = 0, last_us = 0;
boxshelf::app::ShelfScreen screen;
boxshelf::app::LibraryLoader loader;
boxshelf::app::ArtFetcher art;
hui::ps5::Pad pad;
hui::InputTracker tracker;
bool pad_tried = false;

// The console's time zone. The app runtime has time() but no localtime(): the offset comes from the system settings.
// Parameter ids 4 (time zone, minutes) and 5 (summer time) follow the PS4 SDK numbering; NOT yet confirmed on the
// PS5 (the values are logged at boot). Until confirmed, a failed read shows UTC.
extern "C" int sceSystemServiceParamGetInt(int, int *);
int utc_offset_minutes()
{
    static int offset = 0x7fffffff;
    if (offset == 0x7fffffff)
    {
        int zone = 0, summer = 0;
        const int rz = sceSystemServiceParamGetInt(4, &zone), rs = sceSystemServiceParamGetInt(5, &summer);
        hui::sys::log("[BOXSHELF] clock: time zone %d (ret %d), summer time %d (ret %d)", zone, rz, summer, rs);
        offset = (rz == 0 ? zone : 0) + (rs == 0 && summer ? 60 : 0);
    }
    return offset;
}

boxshelf::app::Clock clock_now()
{
    return boxshelf::app::format_clock(static_cast<long long>(std::time(nullptr)) + utc_offset_minutes() * 60LL);
}

void draw(boxshelf::GlCanvas &canvas) noexcept
{
    const std::int64_t now_us = hui::sys::monotonic_us();
    const double now = static_cast<double>(now_us - start_us) / 1e6;
    const float dt = static_cast<float>(now_us - last_us) / 1e6f;
    last_us = now_us;

    std::vector<boxshelf::app::Shelf> shelves;
    std::vector<boxshelf::app::ArtJob> jobs;
    if (loader.take(shelves, jobs))
    {
        boxshelf::app::ShelfModel model;
        model.set(std::move(shelves));
        screen.set_library(std::move(model), now);
        screen.set_status("");
        // Box art from the sources Boxshelf uses, downloaded in the background; covers appear as they land.
        art.start(std::move(jobs));
    }
    else if (screen.model().empty())
        screen.set_status(loader.status());
    for (const auto &[id, path] : art.take())
        screen.art_arrived(id, path);
    // Settings → Box art: find what is missing, or download everything again
    if (const int ask = screen.take_art_request())
        art.restart(boxshelf::app::art_jobs(screen.games(), ask == 2));
    screen.set_art_status(art.status(), art.running(), now);
    if (!pad_tried)
    {
        pad_tried = true;
        if (!pad.open())
            hui::sys::log("[BOXSHELF] pad open failed");
    }
    std::array<hui::PadSample, 64> samples;
    const std::size_t n = pad.read(samples);
    const hui::InputFrame in = tracker.update(std::span<const hui::PadSample>(samples.data(), n),
                                              static_cast<std::uint64_t>(now_us));
    screen.input(in, now, dt);
    screen.draw(canvas, now, clock_now());
}
} // namespace

int main()
{
    hui::sys::log("[BOXSHELF] boot PPSA99850");
    start_us = last_us = hui::sys::monotonic_us();
    // the owner's choices: the interface language (the console's when none was picked) and games moved to a shelf
    auto settings = boxshelf::app::load_settings();
    int system_language = 1;
    const int rl = sceSystemServiceParamGetInt(1, &system_language); // SCE_SYSTEM_SERVICE_PARAM_ID_LANG
    const auto fallback = boxshelf::app::language_from_system(rl == 0 ? system_language : 1);
    boxshelf::app::set_language(boxshelf::app::language_from_code(settings.language, fallback));
    hui::sys::log("[BOXSHELF] language: system %d (ret %d), using %s; %zu games moved", system_language, rl,
                  std::string(boxshelf::app::language_code(boxshelf::app::language())).c_str(), settings.moves.size());
    loader.start(settings.moves);
    screen.set_settings(std::move(settings));
    boxshelf::run_gl(draw);
}
