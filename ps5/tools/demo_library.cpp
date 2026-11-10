// Boxshelf PS5 - the demo shelves (PC tools only: the tour video, tools/demo-check.sh). SPDX-License-Identifier: GPL-3.0-or-later
#include "demo_library.hpp"

namespace boxshelf::app
{
namespace
{
struct DemoGame
{
    data::System system;
    const char *title;
};
using S = data::System;
// Titles as the libretro thumbnail sets name them (checked with the app's own matcher: tools/demo-check.sh). libretro
// has few PS3 / PS4 / Xbox 360 boxes, so those shelves use games it has; Switch boxes come from GameTDB's title list.
constexpr DemoGame kDemo[] = {
    {S::gc, "Super Mario Sunshine"}, {S::gc, "The Legend of Zelda - The Wind Waker"}, {S::gc, "Metroid Prime"},
    {S::gc, "Super Smash Bros. Melee"}, {S::gc, "Mario Kart - Double Dash!!"}, {S::gc, "Luigi's Mansion"},
    {S::gc, "Pikmin"}, {S::gc, "F-Zero GX"}, {S::gc, "Paper Mario - The Thousand-Year Door"},
    {S::gc, "Sonic Adventure 2 - Battle"}, {S::gc, "Resident Evil 4"}, {S::gc, "Animal Crossing"},

    {S::wii, "Super Mario Galaxy"}, {S::wii, "Mario Kart Wii"}, {S::wii, "The Legend of Zelda - Twilight Princess"},
    {S::wii, "Super Smash Bros. Brawl"}, {S::wii, "Wii Sports"}, {S::wii, "Donkey Kong Country Returns"},
    {S::wii, "Metroid Prime 3 - Corruption"}, {S::wii, "Xenoblade Chronicles"},
    {S::wii, "Kirby's Return to Dream Land"}, {S::wii, "New Super Mario Bros. Wii"},

    {S::wiiu, "Mario Kart 8"}, {S::wiiu, "Super Mario 3D World"}, {S::wiiu, "Splatoon"},
    {S::wiiu, "Super Smash Bros. for Wii U"}, {S::wiiu, "The Legend of Zelda - The Wind Waker HD"},
    {S::wiiu, "Bayonetta 2"}, {S::wiiu, "Pikmin 3"}, {S::wiiu, "Donkey Kong Country - Tropical Freeze"},

    {S::sw, "The Legend of Zelda - Breath of the Wild"}, {S::sw, "Super Mario Odyssey"},
    {S::sw, "Mario Kart 8 Deluxe"}, {S::sw, "Animal Crossing - New Horizons"}, {S::sw, "Super Smash Bros. Ultimate"},
    {S::sw, "Splatoon 2"}, {S::sw, "Metroid Dread"}, {S::sw, "Kirby and the Forgotten Land"},
    {S::sw, "Xenoblade Chronicles 3"}, {S::sw, "Super Mario Bros. Wonder"},

    {S::psp, "God of War - Chains of Olympus"}, {S::psp, "Crisis Core - Final Fantasy VII"},
    {S::psp, "Monster Hunter Freedom Unite"}, {S::psp, "Grand Theft Auto - Liberty City Stories"},
    {S::psp, "Patapon"}, {S::psp, "LocoRoco"}, {S::psp, "Daxter"}, {S::psp, "Metal Gear Solid - Peace Walker"},

    {S::ps2, "Shadow of the Colossus"}, {S::ps2, "God of War"}, {S::ps2, "Grand Theft Auto - San Andreas"},
    {S::ps2, "Final Fantasy X"}, {S::ps2, "Kingdom Hearts"}, {S::ps2, "Metal Gear Solid 3 - Snake Eater"},
    {S::ps2, "Ratchet & Clank"}, {S::ps2, "Jak and Daxter - The Precursor Legacy"}, {S::ps2, "Ookami"},
    {S::ps2, "Gran Turismo 4"}, {S::ps2, "Silent Hill 2"}, {S::ps2, "Ico"},

    {S::ps3, "Demon's Souls"}, {S::ps3, "Grand Theft Auto V"}, {S::ps3, "Heavy Rain"},
    {S::ps3, "Final Fantasy XIII"}, {S::ps3, "Killzone 2"}, {S::ps3, "Resident Evil 5"},
    {S::ps3, "Valkyria Chronicles"}, {S::ps3, "Castlevania - Lords of Shadow"},
    {S::ps3, "Metal Gear Solid V - Ground Zeroes"}, {S::ps3, "Catherine"},

    {S::ps4, "Persona 5"}, {S::ps4, "Final Fantasy Type-0 HD"}, {S::ps4, "Final Fantasy X & X-2 HD Remaster"},
    {S::ps4, "Gravity Rush Remastered"}, {S::ps4, "NieR Automata"}, {S::ps4, "Dark Souls III"},
    {S::ps4, "Final Fantasy XV"}, {S::ps4, "Nioh"},

    {S::x360, "Bayonetta"}, {S::x360, "Portal 2"}, {S::x360, "Dead Rising"}, {S::x360, "Final Fantasy XIII"},
    {S::x360, "Final Fantasy XIII-2"}, {S::x360, "Catherine"}, {S::x360, "Dead or Alive 5"},
    {S::x360, "Super Street Fighter IV"},

    {S::n3ds, "Super Mario 3D Land"}, {S::n3ds, "The Legend of Zelda - A Link Between Worlds"},
    {S::n3ds, "Pokemon X"}, {S::n3ds, "Fire Emblem Awakening"}, {S::n3ds, "Animal Crossing - New Leaf"},
    {S::n3ds, "Kirby - Planet Robobot"}, {S::n3ds, "Mario Kart 7"}, {S::n3ds, "Luigi's Mansion - Dark Moon"},

    {S::psx, "Final Fantasy VII"}, {S::psx, "Metal Gear Solid"}, {S::psx, "Crash Bandicoot"},
    {S::psx, "Spyro the Dragon"}, {S::psx, "Castlevania - Symphony of the Night"}, {S::psx, "Resident Evil 2"},
    {S::psx, "Tony Hawk's Pro Skater 2"}, {S::psx, "Gran Turismo 2"}, {S::psx, "Silent Hill"}, {S::psx, "Tekken 3"},

    {S::nds, "New Super Mario Bros."}, {S::nds, "Mario Kart DS"}, {S::nds, "Pokemon HeartGold Version"},
    {S::nds, "The World Ends with You"}, {S::nds, "Phoenix Wright - Ace Attorney"},
    {S::nds, "Castlevania - Dawn of Sorrow"}, {S::nds, "Professor Layton and the Curious Village"},
    {S::nds, "Kirby Super Star Ultra"},

    {S::n64, "Super Mario 64"}, {S::n64, "The Legend of Zelda - Ocarina of Time"}, {S::n64, "GoldenEye 007"},
    {S::n64, "Mario Kart 64"}, {S::n64, "Banjo-Kazooie"}, {S::n64, "Star Fox 64"}, {S::n64, "Super Smash Bros."},
    {S::n64, "Paper Mario"},
};
} // namespace

std::vector<data::Game> demo_games()
{
    std::vector<data::Game> out;
    for (const auto &d : kDemo)
    {
        data::Game g;
        g.system = d.system;
        g.title = d.title;
        g.region = "USA";
        g.path = std::string("/demo/") + std::string(data::style(d.system).key) + "/" + d.title;
        g.id = "demo-" + data::make_game_id(d.system, g.path);
        out.push_back(std::move(g));
    }
    return out;
}

bool is_demo(const data::Game &game)
{
    return game.path.rfind("/demo/", 0) == 0;
}
} // namespace boxshelf::app
