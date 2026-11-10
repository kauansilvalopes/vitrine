// Boxshelf PS5 - console entry point. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/clock.hpp"
#include "app/library_loader.hpp"
#include "app/scene.hpp"
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
    if (loader.take(shelves))
    {
        boxshelf::app::ShelfModel model;
        model.set(std::move(shelves));
        screen.set_library(std::move(model), now);
        screen.set_status("");
    }
    else if (screen.model().empty())
        screen.set_status(loader.status());
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
    loader.start();
    boxshelf::run_gl(draw);
}
