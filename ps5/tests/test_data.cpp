// Boxshelf PS5 - host unit tests for the data layer (src/data). SPDX-License-Identifier: GPL-3.0-or-later
// Game names below are made up; no real titles are needed to test the rules.
#include "data/cover_index.hpp"
#include "data/covers.hpp"
#include "data/scan.hpp"
#include "data/systems.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace boxshelf::data;

static int failures = 0, checks = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            ++failures;                                                                                                \
            std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond);                              \
        }                                                                                                              \
    } while (0)
#define CHECK_EQ(a, b) CHECK((a) == (b))

static void test_styles()
{
    // Every shelf has a style whose key round-trips, and the numbers match Boxshelf's app.js.
    for (int i = 0; i < kSystemCount; ++i)
    {
        const auto s = static_cast<System>(i);
        CHECK_EQ(style(s).id, s);
        CHECK_EQ(system_from_key(style(s).key), s);
        CHECK(style(s).ratio > 0.5f && style(s).ratio < 1.2f);
        CHECK(!style(s).spine_band.label.empty());
    }
    CHECK(std::fabs(style(System::n3ds).ratio - 1.127f) < 1e-6f); // the 3DS case is landscape
    CHECK(style(System::n3ds).ratio > 1.0f);
    CHECK(std::fabs(style(System::psx).ratio - 1.136f) < 1e-6f); // PS1 jewel case, landscape too
    CHECK(!style(System::psx).has_band);
    CHECK(std::fabs(style(System::sw).ratio - 0.612f) < 1e-6f);
    CHECK_EQ(style(System::ps5).case_color, 0x2b62d9u);
    CHECK_EQ(style(System::ps5).spine_band.label, std::string_view("PS5"));
    CHECK(style(System::x360).wrap);
    CHECK_EQ(system_from_key("nope"), System::count);
    // Not one shape for everything (PS5-PORT.md section 6, lesson 1).
    CHECK(style(System::sw).ratio != style(System::ps5).ratio);
    // Spines are thin: depth * ratio (thickness / height) stays near 6-7 %, never Estante's old 10 %.
    for (System s : {System::ps5, System::ps4, System::ps2, System::gc, System::wii, System::wiiu, System::sw})
        CHECK(style(s).depth * style(s).ratio < 0.08f);
    // place(): bh = min(vh*.4, vw*.22/ratio, 620) on 1920x1080.
    const auto ps5 = case_size(style(System::ps5), 1920, 1080);
    CHECK(std::fabs(ps5.h - 432.0f) < 0.01f); // 1080*.4 = 432 < 1920*.22/.787 = 536.7
    CHECK(std::fabs(ps5.w - 432.0f * 0.787f) < 0.01f);
    const auto ds3 = case_size(style(System::n3ds), 1920, 1080);
    CHECK(std::fabs(ds3.h - 1920.0f * 0.22f / 1.127f) < 0.01f); // wide case: width-limited
}

static void test_rules()
{
    CHECK(is_game_file(System::ps2, "Some Game (USA).iso"));
    CHECK(is_game_file(System::ps2, "x.CHD"));
    CHECK(!is_game_file(System::ps2, "x.bin"));
    CHECK(!is_game_file(System::ps2, ".hidden.iso"));
    CHECK(is_game_file(System::psx, "Game (Disc 1).cue"));
    CHECK(!is_game_file(System::psx, "Game (Track 1).bin"));
    CHECK(is_game_file(System::wii, "game.wbfs"));
    CHECK(!is_game_file(System::gc, "game.wbfs"));
    CHECK(is_game_file(System::n3ds, "game.cia"));
    CHECK(is_game_file(System::nds, "game.nds"));
    CHECK(is_game_file(System::wiiu, "game.wua"));

    CHECK(is_switch_base("Game Name [0100ABCD12340000][v0].nsp"));
    CHECK(!is_switch_base("Game Name [0100ABCD12340800][v131072].nsp")); // update
    CHECK(!is_switch_base("Game Name [0100ABCD12341001][v0].nsp"));      // DLC
    CHECK(!is_switch_base("Game Name [UPD].nsp"));
    CHECK(!is_switch_base("Game Name Update 1.2.nsp"));
    CHECK(is_switch_base("Game Name.xci"));
    CHECK(!is_game_file(System::sw, "Game [0100ABCD12340800].nsp"));

    CHECK_EQ(gc_or_wii_from_disc_id("GALE01"), System::gc);
    CHECK_EQ(gc_or_wii_from_disc_id("RMGE01"), System::wii);
    CHECK_EQ(gc_or_wii_from_disc_id(""), System::count);

    CHECK_EQ(clean_title("Simpsons, The - Hit (USA) [!].iso"), "The Simpsons - Hit");
    CHECK_EQ(clean_title("Some_Game_Name (Europe) (En,Fr).chd"), "Some Game Name");
    CHECK_EQ(clean_title("Space  Game  (v1.0.1)", false), "Space Game");
    CHECK_EQ(clean_title("Dr. Robot 2.5", false), "Dr. Robot 2.5"); // a folder's dot is not an extension
    CHECK_EQ(clean_title("Dr. Robot 2.iso"), "Dr. Robot 2");
    CHECK_EQ(clean_title("(only tags).iso"), "(only tags).iso"); // nothing readable left: the raw name
    CHECK_EQ(clean_title("Luigi_s Mansion (USA).3ds"), "Luigi's Mansion");
    CHECK_EQ(clean_title("Legend of Zelda, The - Majora_s Mask 3D (Europe).3ds"), "The Legend of Zelda - Majora's Mask 3D");
    CHECK_EQ(clean_title("Some_Game_Name (Europe).chd"), "Some Game Name");

    CHECK_EQ(make_game_id(System::ps2, "/a/b.iso"), make_game_id(System::ps2, "/a/b.iso"));
    CHECK(make_game_id(System::ps2, "/a/b.iso") != make_game_id(System::ps2, "/a/c.iso"));
    CHECK_EQ(make_game_id(System::ps2, "/a").substr(0, 4), "ps2:");
    CHECK_EQ(emulator_app(System::sw), std::string_view("PPSA99008"));
    CHECK(emulator_app(System::ps5).empty());
}

static void test_roots()
{
    const auto roots = default_roots();
    auto has = [&](const char *path, System s)
    {
        for (const auto &r : roots)
            if (r.path == path && r.system == s)
                return true;
        return false;
    };
    CHECK(has("/mnt/usb0/Emulation/roms/ps2", System::ps2));
    CHECK(has("/mnt/usb0/Emulation/roms/3ds", System::n3ds));
    CHECK(has("/mnt/usb2/Emulation/eden/roms", System::sw));
    CHECK(has("/data/SwanStationPS5/games", System::psx));
    CHECK(has("/mnt/ext1/Dolphin/gamecube", System::gc));

    const auto ecam = roots_from_ps5cemu_json(
        R"({"gamesFolder":"/mnt/usb0/Emulation/roms/wiiu","n3ds":{"gamesFolder":"/mnt/usb0/x/3ds"},"nds":{"gamesFolder":"relative"}})");
    CHECK_EQ(ecam.size(), 2u);
    CHECK(ecam.size() == 2 && ecam[0].system == System::wiiu && ecam[1].system == System::n3ds);
    CHECK(roots_from_ps5cemu_json("not json").empty());

    const auto sx2 = roots_from_ps5sx2_ini("[Other]\nGameFolders=/no\n[PS5SX2]\nGameFolders = /mnt/usb0/a; /data/b ;rel\n");
    CHECK_EQ(sx2.size(), 2u);
    CHECK(sx2.size() == 2 && sx2[0].path == "/mnt/usb0/a" && sx2[1].path == "/data/b");

    const auto swan = roots_from_swanstation_ini("[x]\nusb_folder=PS1\n");
    CHECK_EQ(swan.size(), 3u);
    CHECK(!swan.empty() && swan[0].path == "/mnt/usb0/PS1" && swan[0].kind == RootKind::folders);
    CHECK(roots_from_swanstation_ini("usb_folder=../etc\n").empty());
    CHECK(roots_from_swanstation_ini("usb_folder=/abs\n").empty());
    const auto sub = roots_from_swanstation_ini("usb_folder=Emulation/roms/psx\n"); // the owner's real setting
    CHECK(!sub.empty() && sub[0].path == "/mnt/usb0/Emulation/roms/psx");
}

static void test_listing()
{
    std::vector<DirEntry> entries;
    CHECK(parse_fs_list(R"({"entries":[{"name":"Alpha (USA).iso","dir":false},{"name":"notes.txt","dir":false},)"
                        R"({"name":"Sub","dir":true},{"name":".trash","dir":true},{"name":7}]})",
                        entries));
    CHECK_EQ(entries.size(), 4u);
    std::vector<DirEntry> broken{{"stale", false}};
    CHECK(!parse_fs_list("{", broken));
    CHECK(broken.empty());

    const Root ps2{"/mnt/usb0/Emulation/roms/ps2/", System::ps2, RootKind::files, false};
    const auto g = scan_listing(ps2, entries);
    CHECK_EQ(g.size(), 1u);
    CHECK(g.size() == 1 && g[0].title == "Alpha" && g[0].path == "/mnt/usb0/Emulation/roms/ps2/Alpha (USA).iso");
    CHECK(g.size() == 1 && g[0].app == "PPSA99203" && g[0].system == System::ps2 && !g[0].folder);

    // Switch on the HD: one folder per game with its update / DLC beside it (ProsperoEden's layout).
    const Root eden{"/mnt/usb0/Emulation/eden/roms", System::sw, RootKind::files_folders, false};
    const auto sw = scan_listing(eden, {{"Kart Racer Deluxe", true},
                                        {"Loose Game [0100AAAA00000000][v0].nsp", false},
                                        {"Loose Game [0100AAAA00000800][v65536].nsp", false}});
    CHECK_EQ(sw.size(), 2u);
    CHECK(sw.size() == 2 && sw[0].folder && sw[0].title == "Kart Racer Deluxe" && sw[1].title == "Loose Game");

    // PS1: SwanStation's folders are one game each; files are ignored there.
    const Root swan{"/data/SwanStationPS5/games", System::psx, RootKind::folders, false};
    CHECK_EQ(scan_listing(swan, {{"Racer", true}, {"x.cue", false}}).size(), 1u);

    // Porpoise's mixed folder: the extension decides until the disc id is known.
    const Root mixed{"/data/porpoise/games", System::count, RootKind::files, true};
    const auto m = scan_listing(mixed, {{"a.gcm", false}, {"b.wbfs", false}, {"c.txt", false}});
    CHECK(m.size() == 2 && m[0].system == System::gc && m[1].system == System::wii);
}

static void test_appdb_and_shelves()
{
    const auto apps = games_from_appdb(
        R"({"titles":[{"titleId":"PPSA01234","titleName":"Big Game"},{"titleId":"CUSA05678","titleName":"Old Game"},)"
        R"({"titleId":"PPSA99841","titleName":"Estante"},{"titleId":"PPSA99850","titleName":"Boxshelf"},)"
        R"({"titleId":"PPSA01234","titleName":"dup"},{"titleId":"NPXS12345"},{"titleId":"PPSA02222"}]})");
    CHECK_EQ(apps.size(), 4u);
    CHECK(apps.size() == 4 && apps[0].system == System::ps5 && apps[0].title == "Big Game");
    CHECK(apps.size() == 4 && apps[1].system == System::ps4);
    CHECK(apps.size() == 4 && apps[2].system == System::media && apps[2].app == "PPSA99841");
    CHECK(apps.size() == 4 && apps[3].title == "PPSA02222"); // no name: the id
    // homebrew ports -> Custom; streaming apps -> Media
    const auto more = games_from_appdb(
        R"({"titles":[{"titleId":"PPSA90212","titleName":"A Port"},{"titleId":"PPSA01651","titleName":"YouTube"}]})");
    CHECK(more.size() == 2 && more[0].system == System::custom && more[1].system == System::media);

    std::vector<Game> all = apps;
    const Root ps2{"/r", System::ps2, RootKind::files, false};
    for (auto &g : scan_listing(ps2, {{"zeta.iso", false}, {"Alpha.iso", false}, {"beta.iso", false}}))
        all.push_back(g);
    all.push_back(all.back()); // the same path twice (two roots pointing at one folder)
    const auto shelves = build_shelves(all);
    // Order: Home, then the console order of main/systems.js, Media last.
    CHECK(!shelves.empty() && shelves.front().system == System::home);
    CHECK(!shelves.empty() && shelves.back().system == System::media);
    const Shelf *p2 = nullptr;
    bool has_psp = false, has_ps4 = false;
    for (const auto &s : shelves)
    {
        if (s.system == System::ps2)
            p2 = &s;
        has_psp = has_psp || s.system == System::psp;
        has_ps4 = has_ps4 || s.system == System::ps4;
    }
    CHECK(p2 && p2->games.size() == 3);
    CHECK(p2 && p2->games[0].title == "Alpha" && p2->games[1].title == "beta" && p2->games[2].title == "zeta");
    CHECK(!has_psp); // no PS5 emulator, no games: no shelf
    CHECK(has_ps4);  // has a game
}

static Game game(System s, const char *path, const char *title, bool folder = false)
{
    Game g;
    g.system = s;
    g.path = path;
    g.title = title;
    g.folder = folder;
    return g;
}

static void test_cover_index()
{
    CoverIndex ix;
    // the emulators' caches name older homes of the same files: games match on the file / folder name
    ix.add_swan_serials("/mnt/ext1/SwanStationPS5/games/Racer Two (CHD)/racer2.chd\t1\t2\tSLUS-01234\n"
                        "/x/bad line\n");
    ix.add_ps2_serials("/mnt/ext1/PCSX2/games/skateworld.chd\t1\t2\tSLUS-20999\n"
                       "/mnt/ext1/PCSX2/games/Nice Name.chd\t1\t2\tSLES-50001\n");
    ix.add_ps2_titles("SLUS-20999\tSkate World\nSLES-50001\tNICE NAME (EUROPE)\n");
    ix.add_porpoise_cache("# Porpoise's disc headers (v2)\n"
                          "/mnt/ext1/Dolphin/gamecube/Cube Game (USA).iso\t1\t2\t0\tGQQE01\t0\tCube Game\tISO\tGameCube\tUSA\t\t\n"
                          "/mnt/ext1/Dolphin/wii/Wand Game.iso\t1\t2\t0\tRWWE01\t0\tWand Game\tISO\tWii\tUSA\t\t\n");
    ix.add_eden_name("abcd1234", "Kart Racer: Deluxe\n");
    ix.add_ecam_library(R"({"3ds":[{"titleId":"0004000000011100","gameId":"ABCE","name":"Pocket",)"
                        R"("path":"/mnt/usb0/Emulation/roms/3ds/Pocket (USA).3ds"}],"wiiu":[]})");

    Game p1 = game(System::psx, "/mnt/usb0/Emulation/roms/psx/Racer Two (CHD)", "Racer Two", true);
    CHECK(ix.resolve(p1));
    CHECK(p1.disc_id == "SLUS-01234" && p1.cover_front == "/data/SwanStationPS5/covers/default/SLUS-01234.jpg");

    Game p2 = game(System::ps2, "/mnt/usb0/Emulation/roms/ps2/skateworld.chd", "skateworld");
    CHECK(ix.resolve(p2));
    CHECK(p2.title == "Skate World"); // squashed file name -> the GameDB title
    CHECK(p2.cover_front == "/data/PCSX2/covers/SLUS-20999.jpg" && p2.cover_spine == "/data/PCSX2/covers/SLUS-20999_spine.jpg");
    Game p2b = game(System::ps2, "/mnt/usb0/Emulation/roms/ps2/Nice Name.chd", "Nice Name");
    CHECK(ix.resolve(p2b) && p2b.title == "Nice Name"); // a readable file name stays

    // Porpoise's own folder mixes systems: the disc says which
    Game cube = game(System::wii, "/data/porpoise/games/Cube Game (USA).iso", "Cube Game");
    CHECK(ix.resolve(cube));
    CHECK(cube.system == System::gc && cube.disc_id == "GQQE01" && cube.region == "USA");
    CHECK(cube.cover_front == "/data/porpoise/covers/GQQE01.png" && cube.cover_back == "/data/porpoise/covers/GQQE01.back.png");
    Game wand = game(System::wii, "/mnt/usb0/Emulation/roms/wii/Wand Game.iso", "Wand Game");
    CHECK(ix.resolve(wand) && wand.system == System::wii);

    Game sw = game(System::sw, "/mnt/usb0/Emulation/eden/roms/Kart Racer Deluxe", "Kart Racer Deluxe", true);
    CHECK(ix.resolve(sw) && sw.cover_front == "/data/prosperoeden/covers/abcd1234.tga" && sw.cover_square);
    CHECK(sw.title == "Kart Racer: Deluxe");

    Game ds3 = game(System::n3ds, "/mnt/usb0/Emulation/roms/3ds/Pocket (USA).3ds", "Pocket");
    CHECK(ix.resolve(ds3) && ds3.cover_front == "/data/ps5cemu/covers/boxart/3ds/ABCE.tga" && !ds3.cover_square);
    CHECK(ds3.cover_fallback == "/data/ps5cemu/covers/3ds/0004000000011100.tga"); // the icon when the box is missing

    Game ps5 = game(System::ps5, "/appdb/PPSA01234", "Big Game");
    ps5.app = "PPSA01234";
    CHECK(ix.resolve(ps5) && ps5.cover_front == "/user/appmeta/PPSA01234/icon0.png" && ps5.cover_square);

    Game none = game(System::ps2, "/mnt/usb0/Emulation/roms/ps2/unknown.iso", "unknown");
    CHECK(!ix.resolve(none) && none.cover_front.empty());

    CHECK(leaf_key("/a/B c/") == "b c" && parent_key("/x/Games/Folder X/f.chd") == "folder x");
    CHECK(same_title("Bowsers Fury", "Bowser's Fury") && !same_title("Fury", "Fury 2"));
    CHECK(meaningless("ab") && meaningless("0123ABCD") && !meaningless("Zelda"));
}

static void test_covers()
{
    // covers.js drawFront: W=420, H=round(W/ratio), band = round(H*.085).
    const auto ps5 = cover_spec(style(System::ps5));
    CHECK_EQ(ps5.width, 420);
    CHECK_EQ(ps5.height, 534); // 420/.787 = 533.67
    CHECK_EQ(ps5.band_height, 45);
    const auto ds3 = cover_spec(style(System::n3ds));
    CHECK(ds3.width > ds3.height); // the 3DS cover is landscape: never cropped to 2:3
    CHECK(ds3.face_aspect > 1.0f);
    CHECK_EQ(cover_spec(style(System::psx)).band_height, 0); // band:null
    // Face inside the frame: PS5 (1-.06)/(1/.787-.06) = .7756.
    CHECK(std::fabs(ps5.face_aspect - 0.94f / (1.0f / 0.787f - 0.06f)) < 1e-4f);
    // Each system its own face shape (lesson 1).
    CHECK(std::fabs(cover_spec(style(System::sw)).face_aspect - ps5.face_aspect) > 0.1f);

    CoverCache cache(3, 1000);
    std::uint32_t tex = 0;
    std::string key;
    cache.put("a", 1, 100);
    cache.put("b", 2, 100);
    cache.put("c", 3, 100);
    CHECK(!cache.evict(tex, key));
    CHECK_EQ(cache.touch("a"), 1u); // a is now the newest
    cache.put("d", 4, 100);
    CHECK(cache.evict(tex, key));
    CHECK(tex == 2 && key == "b"); // b was the least recently used
    CHECK(!cache.evict(tex, key));
    CHECK_EQ(cache.touch("b"), 0u);
    CHECK_EQ(cache.size(), 3u);
    cache.put("big", 5, 950); // over the byte budget: older ones go, the newest stays
    int evicted = 0;
    while (cache.evict(tex, key))
        ++evicted;
    CHECK_EQ(evicted, 3);
    CHECK_EQ(cache.size(), 1u);
    CHECK_EQ(cache.touch("big"), 5u);
    cache.put("big", 6, 10); // replacing a key updates its texture and bytes
    CHECK_EQ(cache.bytes(), 10u);
    CHECK_EQ(cache.touch("big"), 6u);
}

int main()
{
    test_cover_index();
    test_covers();
    test_styles();
    test_rules();
    test_roots();
    test_listing();
    test_appdb_and_shelves();
    std::printf("data tests: %d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
