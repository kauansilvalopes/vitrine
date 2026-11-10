// Boxshelf PS5 - PC preview: renders the real shelf screen through the kit's GL renderer on Mesa's surfaceless EGL
// (llvmpipe) into 1920x1080 PPMs. Adapted from Estante's tools/preview-estante.cpp. SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: preview-boxshelf-gl <out.ppm> <mode>
// The library is made up (the same invented titles as Boxshelf's renderer/mock.js), so no real game is needed.
#include "app/scene.hpp"
#include "app/ui_fonts.hpp"
#include "core/save_file.hpp"
#include "gfx/gl_program.hpp"
#include "gfx/renderer.hpp"
#include "gl_main.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

#include "data/cover_index.hpp"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr int kWidth = 1920, kHeight = 1080;

struct GlHost
{
    hui::gfx::Renderer renderer;
    hui::gfx::Font font;
    boxshelf::GlCanvas canvas;
    std::uint32_t font_texture = 0;
    GLuint fbo = 0, color = 0;

    bool init()
    {
        auto get_display = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
            eglGetProcAddress("eglGetPlatformDisplayEXT"));
        EGLDisplay display = get_display ? get_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                                         : eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major = 0, minor = 0;
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor) || !eglBindAPI(EGL_OPENGL_API))
            return false;
        const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
                                     EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
        EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
        if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context))
            return false;
        hui::gfx::set_glsl_prefix("#version 450 core\n");
        if (!renderer.init())
            return false;
        std::string bytes;
        if (!hui::save::read_file("assets/fonts/inter.huifont", &bytes) || !font.load(bytes))
            return false;
        font_texture = renderer.batch().create_font_texture(font);
        boxshelf::app::load_ui_fonts(renderer.batch(), "assets/fonts", &font, font_texture);
        glGenTextures(1, &color);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }

    void render(boxshelf::app::ShelfScreen &screen, double now)
    {
        canvas.begin(&renderer.batch(), &font, font_texture);
        screen.draw(canvas, now, {"ter., 10 de nov.", "13:15"});
        renderer.begin();
        canvas.submit(renderer);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        renderer.present(fbo, kWidth, kHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    bool write_ppm(const char *path)
    {
        FILE *f = std::fopen(path, "wb");
        if (!f)
            return false;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(kWidth) * kHeight * 4u);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        std::fprintf(f, "P6\n%d %d\n255\n", kWidth, kHeight);
        for (int y = kHeight - 1; y >= 0; y--)
            for (int x = 0; x < kWidth; x++)
                std::fwrite(&pixels[(static_cast<std::size_t>(y) * kWidth + x) * 4], 1, 3, f);
        return std::fclose(f) == 0;
    }
};

// Boxshelf's mock.js titles (made up), on the shelves a PS5 fills.
boxshelf::app::ShelfModel demo_library(bool with_empty)
{
    using boxshelf::data::Game;
    using boxshelf::data::System;
    struct Seed
    {
        System system;
        std::vector<const char *> titles;
        const char *ext;
    };
    const std::vector<Seed> seeds = {
        {System::gc, {"Aether Drift", "Bramblewood Tales", "Cinder Circuit", "Coral Kingdom", "Echo Hollow", "Ferrous Fist",
                      "Gale Riders", "Kettle Knight", "Lumen Isle", "Moonmallow", "Orbit Odyssey", "Sky Harbor", "Tidewalker"}, "rvz"},
        {System::wii, {"Bubble Bay Bowling", "Clockwork Carnival", "Felt Friends", "Jumble Jam", "Paper Pilots",
                       "Snowglobe Party", "Woolly Wonders"}, "wbfs"},
        {System::wiiu, {"Canopy Cup", "Glimmer Gorge", "Hollow Harbor Heroes", "Pixel Plaza", "Sunfield Saga", "Tumble Town"}, "wua"},
        {System::sw, {"Astra Vanguard", "Driftlight", "Ember Hollow", "Kitebound", "Mosslight", "Riftrunners", "Tanuki Trail"}, ""},
        {System::ps2, {"Ashen Blade", "Dusk Legion", "Gridlock GP", "Nova Ronin", "Shardfall"}, "chd"},
        {System::n3ds, {"Pocket Pilots", "Bramble Quest", "Starfold", "Chime Island"}, "3ds"},
        {System::psx, {"Crystal Cavern", "Neon Drift", "Polygon Knights"}, ""},
    };
    std::vector<Game> games;
    for (const auto &s : seeds)
        for (const char *t : s.titles)
        {
            Game g;
            g.system = s.system;
            g.title = t;
            g.folder = !*s.ext;
            g.path = std::string("/mnt/usb0/Emulation/roms/") + std::string(boxshelf::data::style(s.system).key) + "/" + t +
                     (*s.ext ? std::string(".") + s.ext : "");
            g.id = boxshelf::data::make_game_id(s.system, g.path);
            g.app = std::string(boxshelf::data::emulator_app(s.system));
            games.push_back(g);
        }
    for (const char *t : {"Iron Meridian", "Lanternfall", "Saltwind"})
    {
        Game g;
        g.system = System::ps5;
        g.title = t;
        g.app = "PPSA0" + std::to_string(1000 + games.size());
        g.path = "/appdb/" + g.app;
        g.id = boxshelf::data::make_game_id(System::ps5, g.path);
        games.push_back(g);
    }
    std::vector<boxshelf::app::Shelf> shelves;
    for (const auto &s : boxshelf::data::build_shelves(games))
    {
        if (s.system == System::home)
            continue;
        if (!with_empty && s.games.empty())
            continue;
        shelves.push_back(boxshelf::app::prepare_shelf(s, true));
    }
    boxshelf::app::ShelfModel model;
    model.set(std::move(shelves));
    return model;
}

std::string slurp(const std::string &path)
{
    std::string out;
    hui::save::read_file(path.c_str(), &out);
    return out;
}

std::string escape_path(const std::string &path) // tools/fetch-listings.py escape()
{
    std::string out;
    char hex[4];
    for (unsigned char c : path)
    {
        if (c == '/')
            out += '_';
        else if (std::isalnum(c) || c == '.' || c == '-')
            out += static_cast<char>(c);
        else
            std::snprintf(hex, sizeof hex, "%%%02X", c), out += hex;
    }
    return out;
}

// The owner's real library from the listings tools/fetch-listings.py saved (.local/listings), resolved with the
// same cover index the console uses. Only file names and paths; the covers themselves come from
// $BOXSHELF_CONSOLE_DIR when present.
boxshelf::app::ShelfModel real_library(const std::string &dir)
{
    using namespace boxshelf::data;
    std::vector<Game> games = games_from_appdb(slurp(dir + "/appdb.json"));
    auto roots = default_roots();
    for (auto &r : roots_from_ps5cemu_json(slurp(dir + "/config/ps5cemu.json")))
        roots.push_back(r);
    for (auto &r : roots_from_swanstation_ini(slurp(dir + "/config/SwanStationPS5.ini")))
        roots.push_back(r);
    std::vector<std::string> seen;
    for (const auto &root : roots)
    {
        if (std::find(seen.begin(), seen.end(), root.path) != seen.end())
            continue;
        seen.push_back(root.path);
        std::vector<DirEntry> entries;
        if (parse_fs_list(slurp(dir + "/fs/" + escape_path(root.path) + ".json"), entries))
            for (auto &g : scan_listing(root, entries))
                games.push_back(std::move(g));
    }
    CoverIndex index;
    index.add_swan_serials(slurp(dir + "/config/swan-serials.tsv"));
    index.add_ps2_serials(slurp(dir + "/config/ps2-serials.txt"));
    index.add_ps2_titles(slurp(dir + "/config/ps2-titles.tsv"));
    index.add_porpoise_cache(slurp(dir + "/config/porpoise-disc-cache.tsv"));
    index.add_ecam_library(slurp(dir + "/config/ecam-library.json"));
    std::vector<DirEntry> eden;
    parse_fs_list(slurp(dir + "/fs/" + escape_path("/data/prosperoeden/covers") + ".json"), eden);
    for (const auto &e : eden)
        if (e.name.size() > 5 && e.name.compare(e.name.size() - 5, 5, ".name") == 0)
            index.add_eden_name(e.name.substr(0, e.name.size() - 5), slurp(dir + "/eden/" + e.name));
    for (auto &g : games)
        index.resolve(g);
    std::vector<std::string> installed;
    for (const auto &g : games)
        if (!g.app.empty())
            installed.push_back(g.app);
    std::vector<boxshelf::app::Shelf> shelves;
    for (const auto &s : build_shelves(games))
    {
        if (s.system == System::home)
            continue;
        const auto app = emulator_app(s.system);
        shelves.push_back(boxshelf::app::prepare_shelf(
            s, !app.empty() && std::find(installed.begin(), installed.end(), app) != installed.end()));
    }
    boxshelf::app::ShelfModel model;
    model.set(std::move(shelves));
    return model;
}

int shelf_of(const boxshelf::app::ShelfModel &m, boxshelf::data::System s)
{
    for (int i = 0; i < m.shelf_count(); ++i)
        if (m.shelf(i).system == s)
            return i;
    return 0;
}
} // namespace

namespace boxshelf
{
[[noreturn]] void run_gl(GlDrawScene) noexcept
{
    std::abort(); // the preview draws frames itself
}
} // namespace boxshelf

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::fprintf(stderr, "usage: %s <out.ppm> <mode>\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[2];
    static GlHost host;
    if (!host.init())
    {
        std::fprintf(stderr, "no GL context or renderer (is Mesa EGL installed?)\n");
        return 4;
    }
    using boxshelf::data::System;
    boxshelf::app::ShelfScreen screen;
    if (mode == "loading")
    {
        screen.set_status("Looking through your game folders");
        host.render(screen, 5.0);
        return host.write_ppm(argv[1]) ? 0 : 3;
    }
    if (mode.rfind("real-", 0) == 0)
    {
        // real-<system key>[-<index>]: the owner's library (paths only), covers from $BOXSHELF_CONSOLE_DIR
        std::string key = mode.substr(5);
        int pick = 4;
        if (const auto dash = key.find('-'); dash != std::string::npos)
            pick = std::atoi(key.c_str() + dash + 1), key = key.substr(0, dash);
        const System sys = boxshelf::data::system_from_key(key);
        screen.set_library(real_library(".local/listings"), 0.0);
        auto &m = screen.model();
        if (m.empty())
            return 5;
        for (int k = 0; k < m.shelf_count() && m.cur().system != sys; ++k)
            m.switch_shelf(1);
        m.go(pick);
        double now = 5.0;
        screen.settle(now);
        // frames until the cover thread has brought every cover in view (or 4 s)
        for (int frame = 0; frame < 240; ++frame)
        {
            host.render(screen, now);
            if (frame > 2 && screen.covers_pending() == 0)
                break;
            usleep(16000);
        }
        host.render(screen, now + 1.7);
        return host.write_ppm(argv[1]) ? 0 : 3;
    }
    auto model = demo_library(mode == "empty-shelf");
    // Modes: the shelf a case is chosen on and how far along it; "anim" is caught mid-slide.
    struct Shot
    {
        const char *name;
        System system;
        int select;
    };
    const Shot shots[] = {{"gc", System::gc, 5},    {"gc-first", System::gc, 0}, {"n3ds", System::n3ds, 2},
                          {"sw", System::sw, 4},    {"ps2", System::ps2, 3},     {"psx", System::psx, 2},
                          {"wii", System::wii, 3},  {"wiiu", System::wiiu, 2},   {"ps5", System::ps5, 1},
                          {"anim", System::gc, 4},  {"empty-shelf", System::nds, 0}};
    const Shot *shot = nullptr;
    for (const auto &s : shots)
        if (mode == s.name)
            shot = &s;
    if (!shot)
    {
        std::fprintf(stderr, "unknown preview mode: %s\n", mode.c_str());
        return 2;
    }
    screen.set_library(std::move(model), 0.0);
    auto &m = screen.model();
    while (m.current() != shelf_of(m, shot->system))
        m.switch_shelf(1);
    m.go(shot->select);
    double now = 5.0;
    screen.settle(now);
    if (mode == "anim")
    {
        // one step right, caught 0.06 s into the 0.55 s slide
        hui::InputFrame in;
        in.nav = hui::Direction::right;
        screen.input(in, now, 0.016f);
        now += 0.06;
    }
    else
        now += 1.7; // the halo's breath at full strength
    host.render(screen, now);
    return host.write_ppm(argv[1]) ? 0 : 3;
}
