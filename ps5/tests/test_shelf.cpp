// Boxshelf PS5 - host unit tests for the shelf model (src/app/shelf, src/app/css). SPDX-License-Identifier: GPL-3.0-or-later
#include "app/clock.hpp"
#include "app/css.hpp"
#include "app/i18n.hpp"
#include "app/shelf.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace boxshelf;
using namespace boxshelf::app;
using data::System;

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
#define NEAR(a, b) CHECK(std::fabs((a) - (b)) < 1e-3f)

static data::Shelf shelf_of(System s, std::initializer_list<const char *> titles)
{
    data::Shelf out{s, {}};
    for (const char *t : titles)
    {
        data::Game g;
        g.system = s;
        g.title = t;
        g.path = std::string("/r/") + t;
        out.games.push_back(g);
    }
    return out;
}

static void test_place()
{
    // app.js place(): selected x 0, z bw*1.05, ry 22; others x = sg*(bw*1.22+(ad-1)*(bw*.22+bd*.9)), z = -bw*.7-ad*6, ry -sg*60
    const float bw = 300, bd = 30;
    const Pose s = place(5, 5, bw, bd);
    NEAR(s.x, 0.0f);
    NEAR(s.z, 315.0f);
    NEAR(s.ry_deg, 22.0f);
    const Pose r1 = place(6, 5, bw, bd), r3 = place(8, 5, bw, bd), l2 = place(3, 5, bw, bd);
    NEAR(r1.x, 366.0f);
    NEAR(r1.z, -216.0f);
    NEAR(r1.ry_deg, -60.0f);
    NEAR(r3.x, 366.0f + 2 * (66.0f + 27.0f));
    NEAR(r3.z, -228.0f);
    NEAR(l2.x, -(366.0f + 93.0f));
    NEAR(l2.ry_deg, 60.0f);
}

static void test_prepare()
{
    // owner's decision: only the games on a console shelf (no Surprise me, no Open <emulator>)
    const Shelf gc = prepare_shelf(shelf_of(System::gc, {"Alpha", "Beta"}), true);
    CHECK(gc.items.size() == 2);
    CHECK(gc.items[0].kind == ItemKind::game && gc.items[0].title == "Alpha");
    CHECK(gc.game_count == 2);
    const Shelf one = prepare_shelf(shelf_of(System::ps2, {"Solo"}), false);
    CHECK(one.items.size() == 1 && one.items[0].kind == ItemKind::game);
    // empty console shelf: [Add games]
    const Shelf none = prepare_shelf(shelf_of(System::nds, {}), true);
    CHECK(none.items.size() == 1 && none.items[0].kind == ItemKind::setup && none.items[0].hue == 210);
    // a game's hue is covers.js hash(title) % 360
    CHECK(gc.items[0].hue == static_cast<int>(title_hash("Alpha") % 360));
    // the PS5 shelf (no emulator) gets no tiles
    const Shelf ps5 = prepare_shelf(shelf_of(System::ps5, {"A", "B", "C"}), true);
    CHECK(ps5.items.size() == 3);
}

static void test_model()
{
    std::vector<Shelf> shelves;
    shelves.push_back(prepare_shelf(shelf_of(System::gc, {"Apple", "Avocado", "Banana", "Cherry", "2 Fast"}), true));
    shelves.push_back(prepare_shelf(shelf_of(System::wii, {"Wii One", "Wii Two"}), false));
    ShelfModel m;
    m.set(shelves);
    CHECK(m.current() == 0 && m.selected() == 0);
    CHECK(!m.go(0));
    CHECK(m.step(1) && m.selected() == 1);
    CHECK(!m.go(-5) || m.selected() == 0);
    CHECK(m.go(100) && m.selected() == 4); // clamped to the last item
    // sections: [A A] [B] [C] [#]  (digits "#")
    m.go(0);
    std::string label;
    CHECK(m.letter(1, &label) && m.selected() == 2 && label == "B");
    CHECK(m.letter(1, &label) && m.selected() == 3 && label == "C");
    m.go(3);                                                  // in "C"
    CHECK(m.letter(-1, &label) && m.selected() == 2);         // previous section's start
    m.go(1);                                                  // second "A"
    CHECK(m.letter(-1, &label) && m.selected() == 0);         // back to the start of its own section
    m.go(4);
    CHECK(m.letter(1, &label) && m.selected() == 0 && label == "A"); // wraps to the start
    // each shelf keeps its own place; switching wraps
    m.go(3);
    CHECK(m.switch_shelf(1) && m.current() == 1 && m.selected() == 0);
    CHECK(m.switch_shelf(1) && m.current() == 0 && m.selected() == 3);
    CHECK(m.switch_shelf(-1) && m.current() == 1);
    NEAR(m.position(), 0.0f);
    m.go(1); // [Wii One, Wii Two]
    NEAR(m.position(), 1.0f);
}

static void test_css()
{
    // hsl(220, 55%, 22%) = rgb(25, 41, 87) (rounded)
    const auto c = hsl(220, 55, 22);
    CHECK(std::lround(c.r * 255) == 25 && std::lround(c.g * 255) == 46 && std::lround(c.b * 255) == 87);
    CHECK(std::fabs(hsl(-140, 55, 22).r - c.r) < 1e-5f); // hue wraps like CSS
    // FNV-1a over UTF-16: ASCII equals the byte hash; a non-BMP character counts as two units
    CHECK(title_hash("") == 2166136261u);
    CHECK(title_hash("a") == ((2166136261u ^ 'a') * 16777619u));
    CHECK(title_hash("\xc3\xa9") == ((2166136261u ^ 0xe9u) * 16777619u)); // é = U+00E9, one unit
    std::uint32_t h = 2166136261u;
    h = (h ^ 0xd83cu) * 16777619u;
    h = (h ^ 0xdfaeu) * 16777619u;
    CHECK(title_hash("\xf0\x9f\x8e\xae") == h); // U+1F3AE
    // cubic-bezier(.22,.9,.25,1): ends fixed, monotonic, fast start
    NEAR(kSlide(0.0f), 0.0f);
    NEAR(kSlide(1.0f), 1.0f);
    CHECK(kSlide(0.25f) > 0.6f && kSlide(0.5f) > kSlide(0.25f));
    // Glide: a new target starts from where the value is
    Glide g;
    g.set(0.0f, 0.0, true);
    g.set(100.0f, 1.0);
    NEAR(g.at(1.0), 0.0f);
    NEAR(g.at(1.55), 100.0f);
    const float mid = g.at(1.2);
    g.set(-50.0f, 1.2);
    NEAR(g.at(1.2), mid);
    CHECK(g.moving(1.3) && !g.moving(2.0));
    // natural, case-insensitive order (Boxshelf's scanner collator)
    CHECK(data::title_less("Game 2", "Game 10"));
    CHECK(!data::title_less("Game 10", "Game 2"));
    CHECK(data::title_less("alpha", "Beta"));
    CHECK(data::title_less("Zelda", "zelda 2"));
    CHECK(!data::title_less("abc", "ABC") && !data::title_less("ABC", "abc"));
}

static void test_clock()
{
    // 2026-11-10 13:15 (a Tuesday) = 1794316500 s; 1970-01-01 00:00 was a Thursday; 2024-02-29 a leap Thursday
    set_language(Lang::en);
    CHECK(format_clock(1794316500LL).date == "Tue, Nov 10");
    set_language(Lang::es);
    CHECK(format_clock(1794316500LL).date == "mar, 10 nov");
    set_language(Lang::pt);
    const Clock a = format_clock(1794316500LL);
    CHECK(a.date == "ter., 10 de nov." && a.time == "13:15");
    const Clock b = format_clock(0);
    CHECK(b.date == "qui., 1 de jan." && b.time == "00:00");
    const Clock c = format_clock(1709208000LL); // 2024-02-29 12:00
    CHECK(c.date == "qui., 29 de fev." && c.time == "12:00");
    const Clock d = format_clock(-60); // 1969-12-31 23:59, a Wednesday
    CHECK(d.date == "qua., 31 de dez." && d.time == "23:59");
    set_language(Lang::en);
}

static void test_move()
{
    // the owner's "Move to shelf": a recomp from Others to a new N64 shelf, then to Switch
    auto with_ids = [](data::Shelf s)
    {
        for (auto &g : s.games)
            g.id = std::string(data::style(s.system).key) + ":" + g.title;
        return s;
    };
    std::vector<Shelf> shelves;
    shelves.push_back(prepare_shelf(with_ids(shelf_of(System::sw, {"Mario", "Zelda TotK"})), true));
    shelves.push_back(prepare_shelf(with_ids(shelf_of(System::custom, {"Breath of the Wild", "Ocarina 4k"})), false));
    ShelfModel m;
    m.set(shelves, 1);
    CHECK(m.move_item("custom:Ocarina 4k", System::n64));
    CHECK(m.shelf_count() == 3 && m.shelf(1).system == System::n64 && m.shelf(2).system == System::custom); // N64 before Others
    CHECK(m.cur().system == System::custom && m.cur().game_count == 1);
    CHECK(m.shelf(1).items.size() == 1 && m.shelf(1).items[0].system == System::n64);
    CHECK(!m.move_item("custom:Ocarina 4k", System::n64)); // already there
    CHECK(m.move_item("custom:Breath of the Wild", System::sw));
    // Others emptied and went; the view follows the game
    CHECK(m.shelf_count() == 2 && m.cur().system == System::sw);
    CHECK(m.shelf(0).items.size() == 3 && m.shelf(0).items[0].title == "Breath of the Wild" && m.shelf(0).game_count == 3);
}

static void test_home()
{
    // app.js buildHome(): Home goes in front when it has games, and the view stays on its shelf
    std::vector<Shelf> shelves;
    shelves.push_back(prepare_shelf(shelf_of(System::gc, {"A", "B", "C"}), true));
    shelves.push_back(prepare_shelf(shelf_of(System::ps2, {"X", "Y"}), true));
    ShelfModel m;
    m.set(shelves, 1);
    m.go(1);
    std::vector<Item> home{m.shelf(0).items[2], m.shelf(1).items[0]};
    m.set_home(home);
    CHECK(m.shelf_count() == 3 && m.shelf(0).system == System::home && m.shelf(0).game_count == 2);
    CHECK(m.cur().system == System::ps2 && m.selected() == 1);
    CHECK(m.all_games().size() == 5); // Home's copies are not counted
    m.set_home({});
    CHECK(m.shelf_count() == 2 && m.cur().system == System::ps2 && m.selected() == 1);
}

int main()
{
    test_home();
    test_move();
    test_clock();
    test_place();
    test_prepare();
    test_model();
    test_css();
    std::printf("shelf tests: %d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
