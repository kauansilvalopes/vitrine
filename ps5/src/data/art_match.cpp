// Boxshelf PS5 - art name matching. SPDX-License-Identifier: GPL-3.0-or-later
#include "data/art_match.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>

namespace boxshelf::data
{
namespace
{
bool is_word_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

std::vector<std::string> words(std::string_view s)
{
    std::vector<std::string> out;
    std::string w;
    for (char c : s)
    {
        if (c == ' ')
        {
            if (!w.empty())
                out.push_back(w), w.clear();
        }
        else
            w += c;
    }
    if (!w.empty())
        out.push_back(w);
    return out;
}

std::string join(const std::vector<std::string> &w)
{
    std::string out;
    for (const auto &x : w)
    {
        if (!out.empty())
            out += ' ';
        out += x;
    }
    return out;
}

// Latin-1 / Latin Extended-A letters with a diacritic -> the base letter (NFD then drop the marks).
char fold(std::uint32_t cp)
{
    // U+00C0..U+00FF: only letters that NFD splits into a base and a mark fold (as lbindex.js norm does);
    // Æ Ð × Ø Þ ß and their lower cases do not decompose and become spaces.
    static const char *const table = "AAAAAA CEEEEIIII NOOOOO  UUUUY  aaaaaa ceeeeiiii nooooo  uuuuy y";
    static_assert(sizeof("AAAAAA CEEEEIIII NOOOOO  UUUUY  aaaaaa ceeeeiiii nooooo  uuuuy y") == 65);
    if (cp >= 0xC0 && cp <= 0xFF)
        return table[cp - 0xC0];
    // Latin Extended-A (U+0100..U+017F) comes in upper/lower pairs of mostly the same base letters
    struct Range
    {
        std::uint32_t first, last;
        char base;
    };
    // (Đ ħ ı ĸ Ŀ Ł ŉ Ŋ Œ Ŧ ſ do not decompose: left out)
    static const Range ext[] = {{0x100, 0x105, 'a'}, {0x106, 0x10D, 'c'}, {0x10E, 0x10F, 'd'}, {0x112, 0x11B, 'e'},
                                {0x11C, 0x123, 'g'}, {0x124, 0x125, 'h'}, {0x128, 0x130, 'i'}, {0x134, 0x135, 'j'},
                                {0x136, 0x137, 'k'}, {0x139, 0x13E, 'l'}, {0x143, 0x148, 'n'}, {0x14C, 0x151, 'o'},
                                {0x154, 0x159, 'r'}, {0x15A, 0x161, 's'}, {0x162, 0x165, 't'}, {0x168, 0x173, 'u'},
                                {0x174, 0x175, 'w'}, {0x176, 0x178, 'y'}, {0x179, 0x17E, 'z'}};
    for (const auto &r : ext)
        if (cp >= r.first && cp <= r.last)
            return r.base;
    return 0;
}

std::uint32_t next_cp(std::string_view s, std::size_t &i)
{
    const auto c = static_cast<unsigned char>(s[i]);
    std::uint32_t cp = c;
    int extra = 0;
    if (c >= 0xf0)
        cp = c & 0x07, extra = 3;
    else if (c >= 0xe0)
        cp = c & 0x0f, extra = 2;
    else if (c >= 0xc0)
        cp = c & 0x1f, extra = 1;
    ++i;
    for (int k = 0; k < extra && i < s.size(); ++k, ++i)
        cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3f);
    return cp;
}

bool is_roman_or_number(const std::string &w)
{
    if (w.empty())
        return false;
    if (std::all_of(w.begin(), w.end(), [](char c) { return c >= '0' && c <= '9'; }))
        return true;
    return std::all_of(w.begin(), w.end(), [](char c) { return c == 'i' || c == 'v' || c == 'x'; });
}

std::string unbrand(std::string s)
{
    static const char *const brands[] = {"disney s ", "disneys ", "disney pixar ", "disney ", "pixar ", "nickelodeon ",
                                         "nick ", "mtv ", "cartoon network ", "dreamworks ", "activision ",
                                         "ea sports ", "ea ", "sega ", "capcom ", "konami ", "ubisoft ", "thq ", "namco "};
    for (int round = 0; round < 2; ++round)
        for (const char *b : brands)
            if (s.rfind(b, 0) == 0)
            {
                s = s.substr(std::char_traits<char>::length(b));
                break;
            }
    return s;
}

std::string no_and(const std::string &s)
{
    std::vector<std::string> w;
    for (auto &x : words(s))
        if (x != "and")
            w.push_back(x);
    return join(w);
}

std::string no_spaces(const std::string &s)
{
    std::string out;
    for (char c : s)
        if (c != ' ')
            out += c;
    return out;
}
} // namespace

std::string art_norm(std::string_view s)
{
    // 1. lower, accents folded, & -> and, apostrophes dropped, other punctuation -> space
    std::string t;
    for (std::size_t i = 0; i < s.size();)
    {
        const std::uint32_t cp = next_cp(s, i);
        char c = 0;
        if (cp < 0x80)
            c = static_cast<char>(std::tolower(static_cast<int>(cp)));
        else if (const char f = fold(cp))
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(f)));
        else if (cp == 0x2019)
            continue; // ’
        if (c == '&')
        {
            t += " and ";
            continue;
        }
        if (c == '\'')
            continue;
        t += is_word_char(c) ? c : ' ';
    }
    // 2. words: drop the/a/an, roman numerals -> numbers
    static const char *const roman[] = {"ii", "iii", "iv", "v", "vi", "vii", "viii", "ix", "xi", "xii", "xiii",
                                        "xiv", "xv", "xvi", "xvii", "xviii", "xix", "xx"};
    static const int value[] = {2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    std::vector<std::string> out;
    for (auto &w : words(t))
    {
        if (w == "the" || w == "a" || w == "an")
            continue;
        for (int k = 0; k < 18; ++k)
            if (w == roman[k])
            {
                w = std::to_string(value[k]);
                break;
            }
        out.push_back(w);
    }
    return join(out);
}

std::vector<std::string> lookup_names(std::string_view title)
{
    std::vector<std::string> out;
    auto trim_tail = [](std::string t)
    {
        while (!t.empty() && (t.back() == ' ' || t.back() == '-' || t.back() == ':' || t.back() == '+' || t.back() == '&'))
            t.pop_back();
        while (!t.empty() && t.front() == ' ')
            t.erase(t.begin());
        return t;
    };
    auto add = [&](const std::string &raw)
    {
        const std::string t = trim_tail(raw);
        if (t.empty())
            return;
        const std::string n = art_norm(t);
        for (const auto &x : out)
            if (art_norm(x) == n)
                return;
        out.push_back(t);
    };
    add(std::string(title));
    // EDITION_RE: an optional separator, then one of these phrases, then anything
    static const char *const editions[] = {"nintendo switch 2 edition", "switch 2 edition", "ns2 edition",
                                           "definitive edition", "complete edition", "deluxe edition",
                                           "game of the year edition", "game of the year", "goty edition", "goty",
                                           "all dlc", "with dlc", "dlc"};
    std::string t(title);
    for (int round = 0; round < 3; ++round)
    {
        std::string lower;
        for (char c : t)
            lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::size_t cut = std::string::npos;
        for (const char *e : editions)
            for (std::size_t p = lower.find(e); p != std::string::npos; p = lower.find(e, p + 1))
            {
                const std::size_t end = p + std::char_traits<char>::length(e);
                const bool start_ok = p == 0 || !std::isalnum(static_cast<unsigned char>(lower[p - 1]));
                const bool end_ok = end == lower.size() || !std::isalnum(static_cast<unsigned char>(lower[end]));
                if (start_ok && end_ok && p < cut)
                    cut = p;
            }
        if (cut == std::string::npos)
            break;
        std::string u = t.substr(0, cut);
        if (u.find_first_not_of(" -:+&\xe2\x80\x93\xe2\x80\x94") == std::string::npos)
            break;
        t = u;
        add(t);
    }
    return out;
}

int match_title(const std::vector<ArtEntry> &entries, std::string_view title, bool exact_only)
{
    if (entries.empty() || title.empty())
        return -1;
    {
        std::string lt;
        for (char c : title)
            lt += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lt == "dvd" || lt == "disc" || lt == "game" || lt == "image" || lt == "iso" || lt == "default")
            return -1;
    }
    const std::string want = art_norm(title);
    if (want.empty())
        return -1;
    const std::string tight = no_spaces(want);
    auto richest = [&](auto pred)
    {
        int best = -1;
        for (int i = 0; i < static_cast<int>(entries.size()); ++i)
            if (pred(entries[static_cast<std::size_t>(i)]) &&
                (best < 0 || entries[static_cast<std::size_t>(i)].art > entries[static_cast<std::size_t>(best)].art))
                best = i;
        return best;
    };
    int exact = richest([&](const ArtEntry &e) { return e.norm == want; });
    if (exact < 0)
        exact = richest([&](const ArtEntry &e) { return no_spaces(e.norm) == tight; });
    if (exact >= 0 || exact_only)
        return exact;
    const std::string want2 = unbrand(want);
    if (want2 != want)
    {
        const int e2 = richest([&](const ArtEntry &e) { return e.norm == want2 || unbrand(e.norm) == want2; });
        if (e2 >= 0)
            return e2;
    }
    const std::string na = no_and(want);
    if (na != want)
    {
        const int e3 = richest([&](const ArtEntry &e) { return no_and(e.norm) == na; });
        if (e3 >= 0)
            return e3;
    }
    const std::vector<std::string> want_w = words(want2);
    const std::set<std::string> W(want_w.begin(), want_w.end());
    int best = -1;
    double best_score = 0.0;
    for (int i = 0; i < static_cast<int>(entries.size()); ++i)
    {
        const ArtEntry &g = entries[static_cast<std::size_t>(i)];
        double score = 0.0;
        if (g.norm.rfind(want2 + " ", 0) == 0 || want2.rfind(g.norm + " ", 0) == 0)
        {
            const std::string extra = g.norm.size() > want2.size() ? g.norm.substr(want2.size()) : want2.substr(g.norm.size());
            bool numbered = false;
            for (const auto &w : words(extra))
                numbered = numbered || is_roman_or_number(w);
            if (!numbered)
                score = .9 - std::fabs(static_cast<double>(g.norm.size()) - static_cast<double>(want2.size())) / 200.0;
        }
        else
        {
            const std::vector<std::string> G = words(g.norm);
            if (!G.empty())
            {
                int hit = 0;
                for (const auto &w : G)
                    hit += W.count(w) ? 1 : 0;
                std::set<std::string> uni(W);
                uni.insert(G.begin(), G.end());
                const double j = static_cast<double>(hit) / static_cast<double>(uni.size());
                if (j >= .75)
                    score = j;
                else if (W.size() >= 3)
                {
                    std::vector<std::string> extra;
                    for (const auto &w : G)
                        if (!W.count(w))
                            extra.push_back(w);
                    std::size_t k = 0;
                    for (const auto &w : G)
                        if (k < want_w.size() && w == want_w[k])
                            ++k;
                    const bool numbered = std::any_of(extra.begin(), extra.end(), is_roman_or_number);
                    if (k == want_w.size() && extra.size() <= 3 && !numbered)
                        score = .8 - static_cast<double>(extra.size()) * .03;
                }
            }
        }
        if (score > 0 && g.art == 0)
            score -= .05;
        if (score > best_score)
            best_score = score, best = i;
    }
    return best;
}

int match_name(const std::vector<ArtEntry> &entries, std::string_view title)
{
    const auto names = lookup_names(title);
    if (names.empty())
        return -1;
    int hit = match_title(entries, names[0]);
    for (std::size_t i = 1; hit < 0 && i < names.size(); ++i)
        hit = match_title(entries, names[i], true);
    return hit;
}

std::string libretro_title(std::string_view name)
{
    std::string t;
    int depth = 0;
    for (char c : name)
    {
        if (c == '(' || c == '[')
            ++depth;
        else if ((c == ')' || c == ']') && depth > 0)
            --depth;
        else if (depth == 0)
            t += c;
    }
    std::string squeezed;
    for (char c : t)
        if (!(c == ' ' && (squeezed.empty() || squeezed.back() == ' ')))
            squeezed += c;
    while (!squeezed.empty() && squeezed.back() == ' ')
        squeezed.pop_back();
    // "Legend of Zelda, The - X" -> "The Legend of Zelda - X"
    for (const char *art : {", The", ", A", ", An"})
    {
        const auto at = squeezed.find(art);
        if (at == std::string::npos || at == 0)
            continue;
        const std::size_t after = at + std::char_traits<char>::length(art);
        if (after != squeezed.size() && squeezed.compare(after, 3, " - ") != 0)
            continue;
        squeezed = std::string(art + 2) + " " + squeezed.substr(0, at) + squeezed.substr(after);
        break;
    }
    return squeezed;
}

std::vector<std::string> parse_libretro_index(std::string_view html)
{
    std::vector<std::string> names;
    const std::string_view key = "href=\"";
    for (std::size_t p = html.find(key); p != std::string_view::npos; p = html.find(key, p + 1))
    {
        const std::size_t start = p + key.size(), end = html.find('"', start);
        if (end == std::string_view::npos)
            break;
        std::string_view href = html.substr(start, end - start);
        if (href.size() < 5 || href.substr(href.size() - 4) != ".png")
            continue;
        // percent-decode, then HTML entities (&amp;)
        href.remove_suffix(4); // ".png"
        std::string name;
        for (std::size_t i = 0; i < href.size(); ++i)
        {
            if (href[i] == '%' && i + 2 < href.size())
            {
                name += static_cast<char>(std::strtol(std::string(href.substr(i + 1, 2)).c_str(), nullptr, 16));
                i += 2;
            }
            else
                name += href[i];
        }
        for (std::size_t a = name.find("&amp;"); a != std::string::npos; a = name.find("&amp;", a + 1))
            name.replace(a, 5, "&");
        names.push_back(std::move(name));
    }
    return names;
}

std::string libretro_match(const std::vector<std::string> &names, std::string_view title, std::string_view region)
{
    if (names.empty())
        return {};
    std::vector<ArtEntry> entries;
    entries.reserve(names.size());
    for (const auto &n : names)
        entries.push_back({art_norm(libretro_title(n)), n, 1});
    std::vector<std::string> hits;
    for (const auto &t : lookup_names(title))
    {
        const std::string want = art_norm(t);
        for (const auto &e : entries)
            if (e.norm == want)
                hits.push_back(e.name);
        if (!hits.empty())
            break;
    }
    if (hits.empty())
    {
        const int i = match_name(entries, title);
        return i >= 0 ? entries[static_cast<std::size_t>(i)].name : std::string();
    }
    std::string reg = region.empty() ? "usa" : std::string(region);
    for (char &c : reg)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto lower = [](std::string s)
    {
        for (char &c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    for (const auto &h : hits)
        if (lower(h).find("(" + reg) != std::string::npos)
            return h;
    for (const auto &h : hits)
        if (lower(h).find("(usa") != std::string::npos)
            return h;
    return hits.front();
}

std::string_view libretro_system(std::string_view key)
{
    static const std::pair<const char *, const char *> map[] = {
        {"gc", "Nintendo - GameCube"},          {"wii", "Nintendo - Wii"},
        {"psp", "Sony - PlayStation Portable"}, {"wiiu", "Nintendo - Wii U"},
        {"sw", "Nintendo - Switch"},            {"ps3", "Sony - PlayStation 3"},
        {"ps4", "Sony - PlayStation 4"},        {"ps2", "Sony - PlayStation 2"},
        {"x360", "Microsoft - Xbox 360"},       {"n3ds", "Nintendo - Nintendo 3DS"},
        {"psx", "Sony - PlayStation"},          {"nds", "Nintendo - Nintendo DS"},
    };
    for (const auto &[k, v] : map)
        if (key == k)
            return v;
    return {};
}

std::vector<TdbGame> parse_gametdb(std::string_view xml, const std::vector<std::string> &types)
{
    std::vector<TdbGame> out;
    auto inner = [](std::string_view block, std::string_view open, std::string_view close) -> std::string_view
    {
        const auto a = block.find(open);
        if (a == std::string_view::npos)
            return {};
        const auto b = block.find(close, a + open.size());
        return b == std::string_view::npos ? std::string_view{} : block.substr(a + open.size(), b - a - open.size());
    };
    auto unescape = [](std::string s)
    {
        static const std::pair<const char *, const char *> ents[] = {
            {"&amp;", "&"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"}, {"&gt;", ">"}};
        for (const auto &[from, to] : ents)
            for (std::size_t p = s.find(from); p != std::string::npos; p = s.find(from, p + 1))
                s.replace(p, std::char_traits<char>::length(from), to);
        return s;
    };
    for (std::size_t p = xml.find("<game name=\""); p != std::string_view::npos; p = xml.find("<game name=\"", p + 1))
    {
        const auto end = xml.find("</game>", p);
        if (end == std::string_view::npos)
            break;
        const std::string_view b = xml.substr(p, end - p);
        const std::string_view type = inner(b, "<type>", "</type>");
        if (std::find(types.begin(), types.end(), std::string(type)) == types.end())
            continue;
        const std::string_view id = inner(b, "<id>", "</id>");
        if (id.size() < 4 || id.size() > 6)
            continue;
        std::string_view region = inner(b, "<region>", "</region>");
        if (region.empty())
            region = "ALL";
        const auto en = b.find("<locale lang=\"EN\">");
        if (en == std::string_view::npos)
            continue;
        const std::string_view title = inner(b.substr(en), "<title>", "</title>");
        const std::string n = art_norm(unescape(std::string(title)));
        if (!n.empty())
            out.push_back({n, std::string(id), std::string(region)});
    }
    return out;
}

std::string_view gametdb_art(std::string_view key)
{
    return key == "sw" ? "switch" : key == "n3ds" ? "3ds" : std::string_view{};
}

std::vector<std::string> gametdb_front_urls(const std::vector<TdbGame> &db, std::string_view system_key,
                                            std::string_view title)
{
    std::vector<std::string> out;
    const std::string art(gametdb_art(system_key));
    if (art.empty())
        return out;
    std::vector<const TdbGame *> hits;
    for (const auto &t : lookup_names(title))
    {
        const std::string n = art_norm(t);
        for (const auto &g : db)
            if (g.norm == n)
                hits.push_back(&g);
        if (!hits.empty())
            break;
    }
    // gametdb.js REG: a GameTDB region -> the art regions its scans are filed under, best first
    auto regions = [](const std::string &r) -> std::vector<std::string>
    {
        if (r == "NTSC-U")
            return {"US"};
        if (r == "PAL")
            return {"EN", "AU", "FR", "DE", "ES", "IT", "NL"};
        if (r == "NTSC-J")
            return {"JA"};
        if (r == "KOR")
            return {"KO"};
        return {"US", "EN"};
    };
    // the region-free release first (it is the one the West uses), then the others
    std::stable_sort(hits.begin(), hits.end(), [](const TdbGame *a, const TdbGame *b) {
        auto rank = [](const std::string &r) { return r == "ALL" ? 0 : r == "NTSC-U" ? 1 : r == "PAL" ? 2 : 3; };
        return rank(a->region) < rank(b->region);
    });
    for (const TdbGame *g : hits)
        for (const auto &r : regions(g->region))
            for (const char *kind : {"coverHQ", "cover"})
            {
                std::string url = "https://art.gametdb.com/" + art + "/" + kind + "/" + r + "/" + g->id + ".jpg";
                if (std::find(out.begin(), out.end(), url) == out.end())
                    out.push_back(std::move(url));
            }
    if (out.size() > 8)
        out.resize(8);
    return out;
}

std::string libretro_url(std::string_view system_key, std::string_view name)
{
    auto encode = [](std::string_view s)
    {
        static const char hex[] = "0123456789ABCDEF";
        std::string o;
        for (unsigned char c : s)
        {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                o += static_cast<char>(c);
            else
                o += '%', o += hex[c >> 4], o += hex[c & 15];
        }
        return o;
    };
    return "https://thumbnails.libretro.com/" + encode(libretro_system(system_key)) + "/Named_Boxarts/" + encode(name) +
           ".png";
}
} // namespace boxshelf::data
