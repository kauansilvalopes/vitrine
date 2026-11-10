// Boxshelf PS5 - folder listings to games. SPDX-License-Identifier: GPL-3.0-or-later
#include "data/scan.hpp"

#include "cJSON.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <initializer_list>

namespace boxshelf::data
{
namespace
{
char lower(char c)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool iequals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (lower(a[i]) != lower(b[i]))
            return false;
    return true;
}

bool icontains(std::string_view hay, std::string_view needle)
{
    if (needle.size() > hay.size())
        return false;
    for (std::size_t i = 0; i + needle.size() <= hay.size(); ++i)
        if (iequals(hay.substr(i, needle.size()), needle))
            return true;
    return false;
}

std::string_view extension(std::string_view name)
{
    const auto dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0)
        return {};
    return name.substr(dot + 1);
}

bool has_ext(std::string_view name, std::initializer_list<std::string_view> list)
{
    const auto ext = extension(name);
    for (auto e : list)
        if (iequals(ext, e))
            return true;
    return false;
}

std::string join(std::string_view dir, std::string_view leaf)
{
    std::string out(dir);
    while (out.size() > 1 && out.back() == '/')
        out.pop_back();
    out += '/';
    out += leaf;
    return out;
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

// Value of `key=` in an ini text, optionally only inside [section]. Empty when absent.
std::string ini_value(std::string_view ini, std::string_view section, std::string_view key)
{
    std::string_view current;
    while (!ini.empty())
    {
        const auto end = ini.find('\n');
        std::string_view line = trim(ini.substr(0, end));
        ini = end == std::string_view::npos ? std::string_view{} : ini.substr(end + 1);
        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            current = line.substr(1, line.size() - 2);
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string_view::npos)
            continue;
        if ((section.empty() || iequals(current, section)) && iequals(trim(line.substr(0, eq)), key))
            return std::string(trim(line.substr(eq + 1)));
    }
    return {};
}

bool is_hex(char c)
{
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}
} // namespace

// ---------------------------------------------------------------- rules

bool is_game_file(System system, std::string_view name) noexcept
{
    if (name.empty() || name[0] == '.')
        return false;
    switch (system)
    {
    case System::ps2: // PS5SX2 opens these (Estante ps2_image_name); main/systems.js adds gz/bin, PS5SX2 does not
        return has_ext(name, {"iso", "chd", "cso", "zso"});
    case System::psx: // main/systems.js psx; a loose .bin belongs to its .cue
        return has_ext(name, {"cue", "chd", "m3u", "pbp", "iso", "ecm", "mds", "img", "ccd"});
    case System::gc:
        return has_ext(name, {"rvz", "iso", "gcm", "ciso", "gcz", "dol", "elf"});
    case System::wii:
        return has_ext(name, {"rvz", "iso", "wbfs", "wia", "ciso", "gcz", "wad"});
    case System::wiiu: // .rpx / .tmd live inside extracted folders, which are listed as folders
        return has_ext(name, {"wua", "wud", "wux"});
    case System::sw:
        return has_ext(name, {"nsp", "xci", "nsz", "xcz"}) && is_switch_base(name);
    case System::n3ds:
        return has_ext(name, {"3ds", "cci", "cxi", "3dsx", "zcci", "zcxi", "z3dsx", "cia"});
    case System::nds:
        return has_ext(name, {"nds", "dsi"});
    default:
        return false;
    }
}

bool is_switch_base(std::string_view name) noexcept
{
    // A 16-hex title id in brackets decides: base games end in 000, updates in 800, DLC anything else.
    for (std::size_t i = 0; i + 18 <= name.size(); ++i)
    {
        if (name[i] != '[' || name[i + 17] != ']')
            continue;
        bool hex = true;
        for (std::size_t k = 1; k <= 16; ++k)
            hex = hex && is_hex(name[i + k]);
        if (hex)
            return name.substr(i + 14, 3) == "000";
    }
    return !icontains(name, "[UPD]") && !icontains(name, "update") && !icontains(name, "[DLC]") &&
           !icontains(name, " DLC");
}

System gc_or_wii_from_disc_id(std::string_view id) noexcept
{
    if (id.empty())
        return System::count;
    switch (std::toupper(static_cast<unsigned char>(id[0])))
    {
    case 'G':
    case 'D':
    case 'P':
        return System::gc;
    case 'R':
    case 'S':
    case 'H':
    case 'W':
        return System::wii;
    default:
        return System::count;
    }
}

std::string clean_title(std::string_view file, bool is_file)
{
    // Port of Estante's clean_rom_title (src/emulator_import.hpp). No-Intro writes an apostrophe as "_"
    // ("Luigi_s Mansion"): a "_s" after a letter and before a space, a bracket or the end becomes "'s".
    std::string fixed(file);
    for (std::size_t i = 1; i + 1 < fixed.size(); ++i)
        if (fixed[i] == '_' && fixed[i + 1] == 's' && std::isalpha(static_cast<unsigned char>(fixed[i - 1])) &&
            (i + 2 == fixed.size() || fixed[i + 2] == ' ' || fixed[i + 2] == '(' || fixed[i + 2] == '.'))
            fixed[i] = '\'';
    file = fixed;
    std::string_view stem = file;
    const auto dot = stem.rfind('.');
    if (is_file && dot != std::string_view::npos && dot != 0 && stem.size() - dot <= 6 &&
        stem.substr(dot).find(' ') == std::string_view::npos)
        stem = stem.substr(0, dot);
    std::string text;
    int depth = 0;
    bool space = false;
    for (char c : stem)
    {
        if (c == '(' || c == '[')
        {
            ++depth;
            continue;
        }
        if ((c == ')' || c == ']') && depth > 0)
        {
            --depth;
            continue;
        }
        if (depth > 0)
            continue;
        if (c == ' ' || c == '\t' || c == '_')
        {
            space = !text.empty();
            continue;
        }
        if (space)
            text += ' ';
        space = false;
        text += c;
    }
    while (!text.empty() && (text.back() == '-' || text.back() == ' '))
        text.pop_back();
    for (const char *article : {"The", "A", "An"})
    {
        const std::string marker = std::string(", ") + article;
        const auto at = text.find(marker);
        if (at == std::string::npos || at == 0)
            continue;
        const auto after = at + marker.size();
        if (after != text.size() && text[after] != ' ')
            continue;
        text = std::string(article) + " " + text.substr(0, at) + text.substr(after);
        break;
    }
    return text.empty() ? std::string(file) : text;
}

std::string make_game_id(System system, std::string_view path)
{
    std::uint32_t hash = 2166136261u;
    for (char c : path)
        hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "%.*s:%08x", static_cast<int>(style(system).key.size()),
                  style(system).key.data(), hash);
    return buffer;
}

bool same_title(std::string_view a, std::string_view b)
{
    std::size_t i = 0, j = 0;
    auto next = [](std::string_view s, std::size_t &k) -> int
    {
        while (k < s.size())
        {
            const auto c = static_cast<unsigned char>(s[k++]);
            if (std::isalnum(c))
                return std::tolower(c);
            if (c >= 0x80)
                return c;
        }
        return -1;
    };
    for (;;)
    {
        const int x = next(a, i), y = next(b, j);
        if (x != y)
            return false;
        if (x < 0)
            return true;
    }
}

bool meaningless(std::string_view title)
{
    std::size_t hex = 0;
    for (char c : title)
        hex += std::isxdigit(static_cast<unsigned char>(c)) != 0;
    return title.size() < 3 || (title.size() >= 8 && hex == title.size());
}

std::string_view emulator_app(System system) noexcept
{
    switch (system)
    {
    case System::ps2:
        return "PPSA99203"; // PS5SX2
    case System::psx:
        return "PPSA98510"; // SwanStationPS5
    case System::gc:
    case System::wii:
        return "PPSA99764"; // Porpoise
    case System::wiiu:
    case System::n3ds:
    case System::nds:
        return "PPSA99360"; // PS5-ECAM
    case System::sw:
        return "PPSA99008"; // ProsperoEden
    default:
        return {};
    }
}

// ---------------------------------------------------------------- roots

std::vector<Root> default_roots()
{
    std::vector<Root> roots;
    // The owner's external HD (EmuDeck layout shared with the Linux side). The usb number can change.
    struct Hd
    {
        const char *folder;
        System system;
        RootKind kind;
    };
    static constexpr Hd hd[] = {
        {"roms/ps2", System::ps2, RootKind::files},     {"roms/psx", System::psx, RootKind::files_folders},
        {"roms/gc", System::gc, RootKind::files},       {"roms/wii", System::wii, RootKind::files},
        {"roms/wiiu", System::wiiu, RootKind::files_folders},
        {"roms/3ds", System::n3ds, RootKind::files},    {"roms/nds", System::nds, RootKind::files},
        {"eden/roms", System::sw, RootKind::files_folders},
    };
    for (int usb = 0; usb <= 2; ++usb)
        for (const auto &h : hd)
        {
            char path[96];
            std::snprintf(path, sizeof path, "/mnt/usb%d/Emulation/%s", usb, h.folder);
            roots.push_back({path, h.system, h.kind, false});
        }
    // The emulators' own folders (Estante src/main.cpp *_roots).
    for (const char *p : {"/data/PCSX2/games", "/mnt/ext1/PCSX2/games", "/mnt/ext0/PCSX2/games"})
        roots.push_back({p, System::ps2, RootKind::files, false});
    for (const char *p :
         {"/data/SwanStationPS5/games", "/mnt/ext1/SwanStationPS5/games", "/mnt/ext0/SwanStationPS5/games"})
        roots.push_back({p, System::psx, RootKind::folders, false});
    roots.push_back({"/data/porpoise/games", System::count, RootKind::files, true});
    for (const char *ext : {"/mnt/ext1", "/mnt/ext0"})
    {
        roots.push_back({std::string(ext) + "/Dolphin/wii", System::wii, RootKind::files, false});
        roots.push_back({std::string(ext) + "/Dolphin/gamecube", System::gc, RootKind::files, false});
        roots.push_back({std::string(ext) + "/games/wii", System::wii, RootKind::files, false});
        roots.push_back({std::string(ext) + "/games/gamecube", System::gc, RootKind::files, false});
    }
    for (const char *p : {"/data/prosperoeden/roms", "/mnt/ext1/emulators/prosperoeden/roms",
                          "/mnt/ext0/emulators/prosperoeden/roms"})
        roots.push_back({p, System::sw, RootKind::files_folders, false});
    return roots;
}

std::vector<Root> roots_from_ps5cemu_json(std::string_view json)
{
    std::vector<Root> roots;
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root)
        return roots;
    auto add = [&](const cJSON *object, System system, RootKind kind)
    {
        const cJSON *folder = cJSON_GetObjectItemCaseSensitive(object, "gamesFolder");
        if (cJSON_IsString(folder) && folder->valuestring && folder->valuestring[0] == '/')
            roots.push_back({folder->valuestring, system, kind, false});
    };
    add(root, System::wiiu, RootKind::files_folders);
    add(cJSON_GetObjectItemCaseSensitive(root, "n3ds"), System::n3ds, RootKind::files);
    add(cJSON_GetObjectItemCaseSensitive(root, "nds"), System::nds, RootKind::files);
    add(cJSON_GetObjectItemCaseSensitive(root, "switch"), System::sw, RootKind::files_folders);
    cJSON_Delete(root);
    return roots;
}

std::vector<Root> roots_from_ps5sx2_ini(std::string_view ini)
{
    std::vector<Root> roots;
    const std::string value = ini_value(ini, "PS5SX2", "GameFolders");
    std::string_view rest = value;
    while (!rest.empty())
    {
        const auto semi = rest.find(';');
        const auto part = trim(rest.substr(0, semi));
        if (!part.empty() && part[0] == '/')
            roots.push_back({std::string(part), System::ps2, RootKind::files, false});
        rest = semi == std::string_view::npos ? std::string_view{} : rest.substr(semi + 1);
    }
    return roots;
}

std::vector<Root> roots_from_swanstation_ini(std::string_view ini)
{
    std::vector<Root> roots;
    // A folder name or a relative path under the drive ("Emulation/roms/psx" on the owner's console).
    std::string folder = ini_value(ini, {}, "usb_folder");
    while (!folder.empty() && folder.back() == '/')
        folder.pop_back();
    if (folder.empty() || folder[0] == '/' || folder == "." || folder.find("..") != std::string::npos)
        return roots;
    for (int usb = 0; usb <= 2; ++usb)
        roots.push_back({"/mnt/usb" + std::to_string(usb) + "/" + folder, System::psx, RootKind::folders, false});
    return roots;
}

// ---------------------------------------------------------------- listings

std::vector<Game> scan_listing(const Root &root, const std::vector<DirEntry> &entries)
{
    std::vector<Game> games;
    for (const auto &e : entries)
    {
        if (e.name.empty() || e.name[0] == '.')
            continue;
        System system = root.system;
        if (root.gc_or_wii)
        {
            // Without Porpoise's disc cache the extension is the only hint: .gcm is GameCube only, .wbfs/.wia/.wad
            // Wii only; anything else waits for the disc id (Wii until then).
            system = has_ext(e.name, {"gcm", "dol", "elf"}) ? System::gc : System::wii;
        }
        bool take = false;
        if (e.dir)
            take = root.kind != RootKind::files;
        else
            take = root.kind != RootKind::folders && is_game_file(system, e.name);
        if (!take)
            continue;
        Game g;
        g.system = system;
        g.folder = e.dir;
        g.path = join(root.path, e.name);
        g.title = clean_title(e.name, !e.dir);
        g.id = make_game_id(system, g.path);
        g.app = std::string(emulator_app(system));
        games.push_back(std::move(g));
    }
    return games;
}

bool parse_fs_list(std::string_view json, std::vector<DirEntry> &out)
{
    out.clear();
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root)
        return false;
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "entries");
    const bool ok = cJSON_IsArray(list);
    const cJSON *e = nullptr;
    cJSON_ArrayForEach(e, list)
    {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(e, "name");
        if (!cJSON_IsString(name) || !name->valuestring)
            continue;
        out.push_back({name->valuestring, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(e, "dir")) != 0});
    }
    cJSON_Delete(root);
    return ok;
}

bool is_homebrew_app(std::string_view tid) noexcept
{
    // Homebrew that is a tool or an emulator, not a game. Both these and the ports (the rest of PPSA9xxxx) go to the
    // Others shelf (owner, 2026-11-10); the flag still tells apps from games there.
    static constexpr std::string_view apps[] = {
        "PPSA99841", // Estante
        "PPSA99842", // Trophy Manager
        "PPSA99000", // ProsperoStore
        "PPSA99505", // Jelly5
        "PPSA99203", "PPSA98510", "PPSA99764", "PPSA99360", "PPSA99008", // the emulators (section 5)
    };
    for (auto a : apps)
        if (a == tid)
            return true;
    return false;
}

bool is_streaming_app(std::string_view tid) noexcept
{
    // Retail apps that are not games: Boxshelf's Media shelf holds the streaming apps.
    static constexpr std::string_view apps[] = {"PPSA01651" /* YouTube */};
    for (auto a : apps)
        if (a == tid)
            return true;
    return false;
}

std::vector<Game> games_from_appdb(std::string_view json)
{
    std::vector<Game> games;
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root)
        return games;
    const cJSON *t = nullptr;
    cJSON_ArrayForEach(t, cJSON_GetObjectItemCaseSensitive(root, "titles"))
    {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(t, "titleId");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(t, "titleName");
        if (!cJSON_IsString(id) || !id->valuestring)
            continue;
        const std::string_view tid = id->valuestring;
        if (tid.size() != 9 || tid == "PPSA99850")
            continue;
        System system = System::count;
        if (tid.substr(0, 4) == "CUSA")
            system = System::ps4;
        else if (tid.substr(0, 4) == "PPSA")
            system = tid[4] == '9' ? System::custom : System::ps5;
        if (is_streaming_app(tid))
            system = System::media;
        if (system == System::count)
            continue;
        if (std::any_of(games.begin(), games.end(), [&](const Game &g) { return g.app == tid; }))
            continue;
        Game g;
        g.system = system;
        g.app = std::string(tid);
        g.path = "/appdb/" + g.app;
        g.title = cJSON_IsString(name) && name->valuestring && name->valuestring[0] ? name->valuestring : g.app;
        g.id = make_game_id(system, g.path);
        games.push_back(std::move(g));
    }
    cJSON_Delete(root);
    return games;
}

// ---------------------------------------------------------------- library

bool title_less(std::string_view a, std::string_view b)
{
    // main/scanner.js: Intl.Collator(undefined, {sensitivity: 'base', numeric: true}) -> case-insensitive, runs of
    // digits compared as numbers ("Game 2" < "Game 10").
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size())
    {
        const auto ca = static_cast<unsigned char>(a[i]), cb = static_cast<unsigned char>(b[j]);
        if (std::isdigit(ca) && std::isdigit(cb))
        {
            std::size_t ei = i, ej = j;
            while (ei < a.size() && a[ei] == '0')
                ++ei;
            while (ej < b.size() && b[ej] == '0')
                ++ej;
            std::size_t ni = ei, nj = ej;
            while (ni < a.size() && std::isdigit(static_cast<unsigned char>(a[ni])))
                ++ni;
            while (nj < b.size() && std::isdigit(static_cast<unsigned char>(b[nj])))
                ++nj;
            if (ni - ei != nj - ej)
                return ni - ei < nj - ej;
            const int c = a.substr(ei, ni - ei).compare(b.substr(ej, nj - ej));
            if (c != 0)
                return c < 0;
            i = ni, j = nj;
            continue;
        }
        const char la = lower(static_cast<char>(ca)), lb = lower(static_cast<char>(cb));
        if (la != lb)
            return static_cast<unsigned char>(la) < static_cast<unsigned char>(lb);
        ++i, ++j;
    }
    return a.size() - i < b.size() - j;
}

std::vector<Shelf> build_shelves(const std::vector<Game> &games)
{
    std::vector<Shelf> shelves;
    for (int i = 0; i < kSystemCount; ++i)
        shelves.push_back({static_cast<System>(i), {}});
    for (const auto &g : games)
    {
        const int i = static_cast<int>(g.system);
        if (i < 0 || i >= kSystemCount)
            continue;
        auto &list = shelves[i].games;
        if (std::none_of(list.begin(), list.end(), [&](const Game &x) { return x.path == g.path; }))
            list.push_back(g);
    }
    for (auto &s : shelves)
        std::stable_sort(s.games.begin(), s.games.end(),
                         [&](const Game &a, const Game &b) { return title_less(a.title, b.title); });
    std::vector<Shelf> kept;
    for (auto &s : shelves)
    {
        // only the shelves that have games: a console with its emulator installed but no game shows no tab
        // (owner, 2026-11-10; Boxshelf keeps those with an "Add games" tile)
        if (!s.games.empty())
            kept.push_back(std::move(s));
    }
    return kept;
}
} // namespace boxshelf::data
