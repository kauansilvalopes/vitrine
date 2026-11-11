// Boxshelf PS5 - PC preview: renders the real shelf screen through the kit's GL renderer on Mesa's surfaceless EGL
// (llvmpipe) into 1920x1080 PPMs. Adapted from Estante's tools/preview-estante.cpp. SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: preview-boxshelf-gl <out.ppm> <mode>
// The library is made up (the same invented titles as Boxshelf's renderer/mock.js), so no real game is needed.
#include "app/clock.hpp"
#include "demo_library.hpp"
#include "app/i18n.hpp"
#include "app/scene.hpp"
#include "app/ui_fonts.hpp"
#include "core/save_file.hpp"
#include "gfx/gl_program.hpp"
#include "gfx/renderer.hpp"
#include "gl_main.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

#include "app/art_fetcher.hpp"
#include "data/cover_index.hpp"

#include <dirent.h>
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
        canvas.set_glass_texture(renderer.glass_texture());
        canvas.begin(&renderer.batch(), &font, font_texture);
        screen.draw(canvas, now, clock);
        renderer.begin();
        canvas.submit(renderer);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        renderer.present(fbo, kWidth, kHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    boxshelf::app::Clock clock{"ter., 10 de nov.", "13:15"};

    // The frame as 1920x1080 RGB, top row first (for the video encoder).
    void read_rgb(std::vector<unsigned char> &rgb)
    {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(kWidth) * kHeight * 4u);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        rgb.resize(static_cast<std::size_t>(kWidth) * kHeight * 3u);
        for (int y = 0; y < kHeight; y++)
            for (int x = 0; x < kWidth; x++)
                for (int c = 0; c < 3; c++)
                    rgb[(static_cast<std::size_t>(kHeight - 1 - y) * kWidth + x) * 3 + c] =
                        pixels[(static_cast<std::size_t>(y) * kWidth + x) * 4 + c];
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
    // Boxshelf's own covers as the console would list them (a copy of .local/boxshelf-covers under the console dir)
    {
        std::vector<std::string> own;
        const char *root = std::getenv("BOXSHELF_CONSOLE_DIR");
        if (root)
            if (DIR *d = opendir((std::string(root) + "/data/boxshelf/covers").c_str()))
            {
                while (dirent *e = readdir(d))
                    if (e->d_name[0] != '.')
                        own.emplace_back(e->d_name);
                closedir(d);
            }
        index.add_boxshelf_covers(own);
    }
    boxshelf::app::apply_info(games); // what the art fetcher learnt from LaunchBox ($BOXSHELF_DOWNLOAD0)
    for (auto &g : games)
    {
        index.resolve(g);
        // the covers the app's own fetcher downloaded (src/app/library_loader.cpp does the same on the console)
        const std::string local = boxshelf::app::downloaded_cover_path(g.id);
        if (FILE *f = std::fopen(local.c_str(), "rb"))
        {
            std::fclose(f);
            if (g.cover_front.rfind("/data/boxshelf/covers/", 0) != 0)
            {
                if (!g.cover_front.empty())
                    g.cover_fallback = g.cover_front, g.fallback_square = g.cover_square;
                g.cover_front = local;
                g.cover_square = false;
            }
        }
    }
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
    std::string mode = argv[2];
    // <mode>-flip: the selected case turned round (Triangle), <mode>-turn: caught mid-turn
    int flip = 0;
    for (const auto &[suffix, kind] : {std::pair<const char *, int>{"-flip", 2}, {"-turn", 1}, {"-play", 3}, {"-menu", 4}, {"-details", 5}, {"-inspect", 6}})
        if (mode.size() > std::strlen(suffix) && mode.compare(mode.size() - std::strlen(suffix), std::string::npos, suffix) == 0)
            flip = kind, mode.resize(mode.size() - std::strlen(suffix));
    // BOXSHELF_LANG=pt|es: the interface language
    if (const char *l = std::getenv("BOXSHELF_LANG"))
        boxshelf::app::set_language(boxshelf::app::language_from_code(l, boxshelf::app::Lang::en));
    auto press = [](boxshelf::app::ShelfScreen &screen, double now, hui::Action action)
    {
        hui::InputFrame f;
        f.pressed = f.held = hui::action_bit(action);
        screen.input(f, now, 0.016f);
    };
    auto press_triangle = [](boxshelf::app::ShelfScreen &screen, double now)
    {
        hui::InputFrame down;
        down.pressed = down.held = hui::action_bit(hui::Action::north);
        screen.input(down, now, 0.016f);
        hui::InputFrame up;
        up.released = hui::action_bit(hui::Action::north);
        screen.input(up, now + 0.1, 0.016f);
    };
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
    if (mode.rfind("intro-", 0) == 0)
    {
        // intro-<ms>: the real library arriving after the splash, caught <ms> into the ripple (app.js revealShelf)
        screen.set_status("Looking through your game folders");
        host.render(screen, 4.0);
        screen.set_library(real_library(".local/listings"), 5.0);
        for (int frame = 0; frame < 120 && (frame < 3 || screen.covers_pending() > 0); ++frame)
        {
            host.render(screen, 5.0);
            usleep(16000);
        }
        host.render(screen, 5.0 + std::atof(mode.c_str() + 6) / 1000.0);
        return host.write_ppm(argv[1]) ? 0 : 3;
    }
    if (mode == "video")
    {
        // A scripted tour of the demo shelves at 60 fps, piped to ffmpeg: argv[1] is the .mp4. Every frame waits
        // (at the same moment of the timeline) until the covers in view are decoded, so no placeholder is filmed.
        auto games = boxshelf::app::demo_games();
        boxshelf::app::apply_downloaded_art(games);
        std::vector<boxshelf::app::Shelf> shelves;
        for (const auto &sh : boxshelf::data::build_shelves(games))
            if (!sh.games.empty())
                shelves.push_back(boxshelf::app::prepare_shelf(sh, false));
        boxshelf::app::ShelfModel model;
        model.set(std::move(shelves));
        screen.set_library(std::move(model), 0.0);
        host.clock = boxshelf::app::format_clock(1794316500LL); // 2026-11-10 13:15, in the interface language
        auto &m = screen.model();
        m.go(4);
        // the timeline: (seconds, what happens)
        enum Ev { Right, Left, Next, Prev, Flip, Back, Menu, Down, Up };
        std::vector<std::pair<double, Ev>> script;
        double t = 2.4;
        auto at = [&](double dt, Ev e) { t += dt; script.push_back({t, e}); };
        // GameCube: a few steps
        at(0.0, Right), at(0.55, Right), at(0.55, Right), at(0.55, Right);
        // every other shelf: down to it, two steps along
        for (int shelf = 1; shelf < m.shelf_count(); ++shelf)
        {
            at(1.5, Down);
            at(1.35, Right), at(0.6, Right);
            // Switch and 3DS: turned round to their real backs (cut from GameTDB's full covers)
            const auto sys = m.shelf(shelf).system;
            if (sys == boxshelf::data::System::sw || sys == boxshelf::data::System::n3ds)
                at(1.2, Flip), at(2.6, Back);
        }
        // the quick menu: Box art, Box backs, Language
        at(1.6, Menu), at(1.0, Down), at(0.8, Down), at(0.8, Down), at(1.6, Menu);
        const double end = t + 2.2;
        const std::string cmd = std::string("ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s 1920x1080 -r 60 -i - ") +
                                "-c:v libx264 -preset slow -crf 16 -pix_fmt yuv420p -movflags +faststart '" + argv[1] + "'";
        FILE *ff = popen(cmd.c_str(), "w");
        if (!ff)
            return 6;
        std::vector<unsigned char> rgb;
        std::size_t next = 0;
        const double t0 = 5.0;
        screen.settle(t0);
        const int frames = static_cast<int>(end * 60.0);
        for (int f = 0; f < frames; ++f)
        {
            const double now = t0 + f / 60.0;
            while (next < script.size() && script[next].first <= f / 60.0)
            {
                hui::InputFrame in;
                switch (script[next].second)
                {
                case Right: in.nav = hui::Direction::right; break;
                case Left: in.nav = hui::Direction::left; break;
                case Down: in.nav = hui::Direction::down; break;
                case Up: in.nav = hui::Direction::up; break;
                case Next: in.pressed = hui::action_bit(hui::Action::jump_next); break;
                case Prev: in.pressed = hui::action_bit(hui::Action::jump_prev); break;
                case Back: in.pressed = hui::action_bit(hui::Action::back); break;
                case Menu:
                    in.pressed = in.held = hui::action_bit(hui::Action::menu);
                    screen.input(in, now, 1.0f / 60.0f);
                    in = hui::InputFrame{};
                    in.released = hui::action_bit(hui::Action::menu);
                    break;
                case Flip:
                    in.pressed = in.held = hui::action_bit(hui::Action::north);
                    screen.input(in, now, 1.0f / 60.0f);
                    in = hui::InputFrame{};
                    in.released = hui::action_bit(hui::Action::north);
                    break;
                }
                screen.input(in, now, 1.0f / 60.0f);
                ++next;
            }
            host.render(screen, now);
            for (int wait = 0; wait < 400 && screen.covers_pending() > 0; ++wait)
            {
                usleep(5000);
                host.render(screen, now);
            }
            host.read_rgb(rgb);
            if (std::fwrite(rgb.data(), 1, rgb.size(), ff) != rgb.size())
                break;
            if (f % 300 == 0)
                std::fprintf(stderr, "video: %d / %d frames\n", f, frames);
        }
        return pclose(ff) == 0 ? 0 : 7;
    }
    if (mode.rfind("demo-", 0) == 0)
    {
        // demo-<system key>[-<index>]: the demo shelves, with the art the fetcher downloaded into $BOXSHELF_DOWNLOAD0
        std::string key = mode.substr(5);
        int pick = 4;
        if (const auto dash = key.find('-'); dash != std::string::npos)
            pick = std::atoi(key.c_str() + dash + 1), key = key.substr(0, dash);
        auto games = boxshelf::app::demo_games();
        boxshelf::app::apply_downloaded_art(games);
        std::vector<boxshelf::app::Shelf> shelves;
        for (const auto &sh : boxshelf::data::build_shelves(games))
            if (!sh.games.empty())
                shelves.push_back(boxshelf::app::prepare_shelf(sh, false));
        if (key == "home" || std::getenv("BOXSHELF_FILTER") || std::getenv("BOXSHELF_SORT"))
        {
            // a few favourites and three recently played games, so Home shows up with mixed cases
            boxshelf::app::Settings with_home;
            for (std::size_t g = 0; g < games.size(); g += 9)
                with_home.favorites.push_back(games[g].id);
            with_home.played(games[50].id, 1794316500LL);
            with_home.played(games[3].id, 1794316500LL - 86400);
            with_home.played(games[95].id, 1794316500LL - 3 * 86400);
            if (const char *f = std::getenv("BOXSHELF_FILTER")) // fav, played, unplayed
                with_home.filter = f;
            if (const char *o = std::getenv("BOXSHELF_SORT")) // recent, most
                with_home.sort = o;
            screen.set_settings(with_home);
        }
        boxshelf::app::ShelfModel model;
        model.set(std::move(shelves));
        screen.set_library(std::move(model), 0.0);
        auto &m = screen.model();
        const System sys = boxshelf::data::system_from_key(key);
        for (int k = 0; k < m.shelf_count() && m.cur().system != sys; ++k)
            m.switch_shelf(1);
        m.go(pick);
        double now = 5.0;
        screen.settle(now);
        for (int frame = 0; frame < 240; ++frame)
        {
            host.render(screen, now);
            if (frame > 2 && screen.covers_pending() == 0)
                break;
            usleep(16000);
        }
        if (flip == 2)
        {
            press_triangle(screen, now);
            for (int frame = 0; frame < 120; ++frame)
            {
                host.render(screen, now + 0.1);
                usleep(16000);
            }
            now += 1.2;
        }
        else if (flip == 6)
        {
            // as the real-* inspect: R3, then Cross to open (BOXSHELF_ISP >= 1) and take the media out (>= 2)
            now += 1.7;
            host.render(screen, now);
            press(screen, now, hui::Action::r3);
            host.render(screen, now + 0.7);
            const char *steps = std::getenv("BOXSHELF_ISP");
            if (!steps || std::atoi(steps) >= 1)
                press(screen, now + 0.8, hui::Action::confirm);
            host.render(screen, now + 1.6);
            if (!steps || std::atoi(steps) >= 2)
                press(screen, now + 1.7, hui::Action::confirm);
            host.render(screen, now + 2.0);
            now += 2.8;
        }
        else
            now += 1.7;
        host.render(screen, now);
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
        if (flip == 3)
        {
            // Cross: the launch screen (on the PC the start answers at once: "needs the console")
            press(screen, now, hui::Action::confirm);
            for (int frame = 0; frame < 30; ++frame)
            {
                host.render(screen, now + frame * .02);
                usleep(10000);
            }
            host.render(screen, now + 1.4);
            return host.write_ppm(argv[1]) ? 0 : 3;
        }
        if (flip == 6)
        {
            // R3, then Cross twice: the case opens and its disc comes out
            press(screen, now, hui::Action::r3);
            host.render(screen, now + 0.7);
            const char *steps = std::getenv("BOXSHELF_ISP"); // 0: closed, 1: the case open, 2 (default): the disc out
            if (!steps || std::atoi(steps) >= 1)
                press(screen, now + 0.8, hui::Action::confirm);
            host.render(screen, now + 1.6);
            if (!steps || std::atoi(steps) >= 2)
                press(screen, now + 1.7, hui::Action::confirm);
            if (const char *turn = std::getenv("BOXSHELF_ISP_TURN")) // degrees, as the right stick would
            {
                hui::InputFrame t;
                t.stick2_x = 1.0f;
                screen.input(t, now + 1.75, static_cast<float>(std::atof(turn)) / 160.0f);
            }
            host.render(screen, now + 2.0);
            host.render(screen, now + 2.8);
            return host.write_ppm(argv[1]) ? 0 : 3;
        }
        if (flip == 5)
        {
            press(screen, now, hui::Action::west); // Square: Details
            host.render(screen, now + 0.2);
            host.render(screen, now + 1.0);
            return host.write_ppm(argv[1]) ? 0 : 3;
        }
        if (flip == 4)
        {
            press(screen, now, hui::Action::menu);
            {
                hui::InputFrame up; // the menu opens on release (a hold opens it on Sort)
                up.released = hui::action_bit(hui::Action::menu);
                screen.input(up, now + 0.05, 0.016f);
            }
            press(screen, now + 0.1, hui::Action::confirm); // opens "Move to shelf"
            host.render(screen, now + 0.2);
            host.render(screen, now + 1.0);
            return host.write_ppm(argv[1]) ? 0 : 3;
        }
        if (flip)
        {
            press_triangle(screen, now);
            for (int frame = 0; frame < 240; ++frame) // the back art, when the emulator keeps one
            {
                host.render(screen, now + 0.1);
                if (frame > 2 && screen.covers_pending() == 0)
                    break;
                usleep(16000);
            }
            now += flip == 2 ? 1.2 : 0.3;
            host.render(screen, now);
            return host.write_ppm(argv[1]) ? 0 : 3;
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
    else if (flip == 4)
    {
        press(screen, now, hui::Action::menu);
            {
                hui::InputFrame up; // the menu opens on release (a hold opens it on Sort)
                up.released = hui::action_bit(hui::Action::menu);
                screen.input(up, now + 0.05, 0.016f);
            }
        press(screen, now + 0.1, hui::Action::confirm);
        now += 1.0;
    }
    else if (flip == 3)
    {
        // Cross: the launch thread answers at once on the PC (no console); the toast is caught as it slides in
        hui::InputFrame x;
        x.pressed = x.held = hui::action_bit(hui::Action::confirm);
        screen.input(x, now, 0.016f);
        usleep(200000);
        host.render(screen, now + 0.05);
        now += 0.6;
    }
    else if (flip)
    {
        press_triangle(screen, now);
        now += flip == 2 ? 1.2 : 0.3;
    }
    else
        now += 1.7; // the halo's breath at full strength
    host.render(screen, now);
    return host.write_ppm(argv[1]) ? 0 : 3;
}
