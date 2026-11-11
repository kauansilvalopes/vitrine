// Boxshelf PS5 - LaunchBox Games Database, read while it downloads. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/launchbox.hpp"

#include "data/art_match.hpp"

#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#include "../third_party/miniz/miniz.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <cstring>

namespace boxshelf::data
{
namespace
{
constexpr std::pair<const char *, const char *> kPlatforms[] = {
    {"gc", "Nintendo GameCube"},   {"wii", "Nintendo Wii"},          {"psp", "Sony PSP"},
    {"wiiu", "Nintendo Wii U"},    {"sw", "Nintendo Switch"},        {"sw2", "Nintendo Switch 2"},
    {"ps3", "Sony Playstation 3"}, {"ps4", "Sony Playstation 4"},    {"ps5", "Sony Playstation 5"},
    {"ps2", "Sony Playstation 2"}, {"x360", "Microsoft Xbox 360"},   {"n3ds", "Nintendo 3DS"},
    {"psx", "Sony Playstation"},   {"nds", "Nintendo DS"},           {"n64", "Nintendo 64"}};
// (no "custom": LaunchBox files the ports under Windows, 39 000 games and none of them the PS5 ports)

// covers.js: the front types in order, North American art first; backs prefer English-only regions
constexpr const char *kFrontTypes[] = {"Box - Front", "Box - Front - Reconstructed", "Fanart - Box - Front", "Steam Poster"};
constexpr const char *kFrontRegions[] = {"North America", "United States", "World", "", "Canada", "Europe", "United Kingdom", "Japan"};
constexpr const char *kBackTypes[] = {"Box - Back", "Box - Back - Reconstructed"};
constexpr const char *kBackRegions[] = {"United Kingdom", "Australia", "Europe", "World", "North America", "United States", "Canada", ""};

std::uint32_t le32(const unsigned char *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
std::uint16_t le16(const unsigned char *p)
{
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::string unescape(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] != '&')
        {
            out += s[i];
            continue;
        }
        const auto semi = s.find(';', i);
        if (semi == std::string_view::npos || semi - i > 10)
        {
            out += s[i];
            continue;
        }
        const std::string_view e = s.substr(i + 1, semi - i - 1);
        if (e == "amp")
            out += '&';
        else if (e == "lt")
            out += '<';
        else if (e == "gt")
            out += '>';
        else if (e == "quot")
            out += '"';
        else if (e == "apos")
            out += '\'';
        else if (!e.empty() && e[0] == '#')
        {
            unsigned long c = e.size() > 1 && (e[1] == 'x' || e[1] == 'X') ? std::strtoul(std::string(e.substr(2)).c_str(), nullptr, 16)
                                                                           : std::strtoul(std::string(e.substr(1)).c_str(), nullptr, 10);
            if (c < 0x80)
                out += static_cast<char>(c);
            else if (c < 0x800)
                out += static_cast<char>(0xc0 | (c >> 6)), out += static_cast<char>(0x80 | (c & 0x3f));
            else
                out += static_cast<char>(0xe0 | (c >> 12)), out += static_cast<char>(0x80 | ((c >> 6) & 0x3f)),
                    out += static_cast<char>(0x80 | (c & 0x3f));
        }
        else
        {
            out += s.substr(i, semi - i + 1);
        }
        i = semi;
    }
    // trim
    const auto a = out.find_first_not_of(" \t\r\n"), b = out.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : out.substr(a, b - a + 1);
}

std::string tag(std::string_view block, const char *name)
{
    const std::string open = std::string("<") + name + ">", close = std::string("</") + name + ">";
    const auto a = block.find(open);
    if (a == std::string_view::npos)
        return {};
    const auto b = block.find(close, a + open.size());
    if (b == std::string_view::npos)
        return {};
    return unescape(block.substr(a + open.size(), b - a - open.size()));
}

template <std::size_t N> int rank_of(const char *const (&list)[N], std::string_view v)
{
    for (std::size_t i = 0; i < N; ++i)
        if (v == list[i])
            return static_cast<int>(i);
    return -1;
}
} // namespace

std::string_view launchbox_platform(std::string_view key)
{
    for (const auto &[k, v] : kPlatforms)
        if (key == k)
            return v;
    return {};
}

struct LaunchBoxReader::Inflate
{
    tinfl_decompressor state;
    unsigned char dict[TINFL_LZ_DICT_SIZE];
    std::size_t dict_ofs = 0;
};

LaunchBoxReader::LaunchBoxReader(std::vector<std::string> platforms) : inf_(new Inflate), platforms_(std::move(platforms))
{
    tinfl_init(&inf_->state);
    for (const auto &p : platforms_)
        games_[p];
}

LaunchBoxReader::~LaunchBoxReader() = default;

bool LaunchBoxReader::feed(const char *data, std::size_t size)
{
    if (broken_ || done_)
        return !broken_;
    return feed_zip(reinterpret_cast<const unsigned char *>(data), size);
}

bool LaunchBoxReader::feed_zip(const unsigned char *data, std::size_t size)
{
    // (with all of Metadata.xml's bytes in, the inflater may still hold output: one more turn with no input flushes it)
    while ((size > 0 || (stage_ == 1 && left_ == 0)) && !done_)
    {
        if (stage_ == 0)
        {
            // a local file header: 30 bytes, then its name and extra field
            const std::size_t need = 30 - std::min<std::size_t>(30, head_.size());
            if (need > 0)
            {
                const std::size_t n = std::min(need, size);
                head_.append(reinterpret_cast<const char *>(data), n);
                data += n, size -= n;
                if (head_.size() < 30)
                    continue;
            }
            const auto *h = reinterpret_cast<const unsigned char *>(head_.data());
            if (le32(h) != 0x04034b50)
                return broken_ = true, false;
            const std::size_t total = 30 + le16(h + 26) + le16(h + 28);
            if (head_.size() < total)
            {
                const std::size_t n = std::min(total - head_.size(), size);
                head_.append(reinterpret_cast<const char *>(data), n);
                data += n, size -= n;
                if (head_.size() < total)
                    continue;
                h = reinterpret_cast<const unsigned char *>(head_.data());
            }
            const std::uint16_t flags = le16(h + 6), method = le16(h + 8);
            left_ = le32(h + 18);
            const std::string name = head_.substr(30, le16(h + 26));
            head_.clear();
            if (flags & 8)
                return broken_ = true, false; // sizes after the data: not what Metadata.zip uses
            if (name == "Metadata.xml")
            {
                if (method != 8 && method != 0)
                    return broken_ = true, false;
                stage_ = method == 8 ? 1 : 4; // 4: stored
            }
            else
                stage_ = 2;
            continue;
        }
        if (stage_ == 2) // another entry before Metadata.xml: skip it
        {
            const std::size_t n = std::min<std::size_t>(left_, size);
            data += n, size -= n, left_ -= n;
            if (left_ == 0)
                stage_ = 0;
            continue;
        }
        if (stage_ == 4) // stored
        {
            const std::size_t n = std::min<std::size_t>(left_, size);
            feed_xml(reinterpret_cast<const char *>(data), n);
            data += n, size -= n, left_ -= n;
            if (left_ == 0)
                done_ = true;
            continue;
        }
        // deflated: inflate what came into the 32 KB wrapping dictionary, handing the text on
        const std::size_t avail = std::min<std::size_t>(left_, size);
        std::size_t in_bytes = avail;
        std::size_t out_bytes = TINFL_LZ_DICT_SIZE - inf_->dict_ofs;
        const tinfl_status st = tinfl_decompress(&inf_->state, data, &in_bytes, inf_->dict, inf_->dict + inf_->dict_ofs,
                                                 &out_bytes, left_ > in_bytes ? TINFL_FLAG_HAS_MORE_INPUT : 0);
        data += in_bytes, size -= in_bytes, left_ -= in_bytes;
        if (out_bytes)
            feed_xml(reinterpret_cast<const char *>(inf_->dict + inf_->dict_ofs), out_bytes);
        inf_->dict_ofs = (inf_->dict_ofs + out_bytes) & (TINFL_LZ_DICT_SIZE - 1);
        if (st == TINFL_STATUS_DONE)
        {
            done_ = true;
            break;
        }
        if (st < 0)
            return broken_ = true, false;
        if (in_bytes == 0 && out_bytes == 0 && st == TINFL_STATUS_NEEDS_MORE_INPUT)
            break; // wait for the next piece
    }
    if (done_)
    {
        // whatever is left of the last block
        feed_xml("", 0);
    }
    return true;
}

void LaunchBoxReader::feed_xml(const char *data, std::size_t size)
{
    xml_.append(data, size);
    std::size_t pos = 0;
    for (;;)
    {
        const auto i = xml_.find("<Game", pos);
        if (i == std::string::npos)
        {
            pos = xml_.size() > 16 ? xml_.size() - 16 : 0;
            break;
        }
        const bool game = xml_.compare(i, 6, "<Game>") == 0, image = xml_.compare(i, 11, "<GameImage>") == 0;
        if (xml_.size() - i < 11)
        {
            pos = i;
            break;
        }
        if (!game && !image)
        {
            pos = i + 5;
            continue;
        }
        const char *close = game ? "</Game>" : "</GameImage>";
        const auto end = xml_.find(close, i);
        if (end == std::string::npos)
        {
            pos = i;
            break;
        }
        block(std::string_view(xml_).substr(i, end - i), image);
        pos = end + std::strlen(close);
    }
    xml_.erase(0, std::min(pos, xml_.size()));
}

void LaunchBoxReader::block(std::string_view text, bool image)
{
    if (!image)
    {
        const std::string plat = tag(text, "Platform");
        auto list = games_.find(plat);
        if (list == games_.end())
            return;
        LbGame g;
        g.id = tag(text, "DatabaseID");
        g.name = tag(text, "Name");
        if (g.id.empty() || g.name.empty())
            return;
        g.norm = art_norm(g.name);
        std::string date = tag(text, "ReleaseDate");
        g.year = date.size() >= 4 ? date.substr(0, 4) : tag(text, "ReleaseYear");
        g.genres = tag(text, "Genres");
        g.developer = tag(text, "Developer");
        g.publisher = tag(text, "Publisher");
        g.players = tag(text, "MaxPlayers");
        g.rating = tag(text, "CommunityRating");
        g.esrb = tag(text, "ESRB");
        g.overview = tag(text, "Overview");
        g.coop = tag(text, "Cooperative") == "true";
        by_id_[g.id] = {plat, list->second.size()};
        list->second.push_back(std::move(g));
        return;
    }
    const std::string id = tag(text, "DatabaseID");
    const auto at = by_id_.find(id);
    if (at == by_id_.end())
        return;
    LbGame &g = games_[at->second.first][at->second.second];
    const std::string type = tag(text, "Type"), region = tag(text, "Region"), file = tag(text, "FileName");
    if (file.empty())
        return;
    if (const int t = rank_of(kFrontTypes, type); t >= 0)
    {
        int r = rank_of(kFrontRegions, region);
        const int rank = t * 100 + (r < 0 ? 99 : r);
        if (rank < g.front_rank)
            g.front_rank = rank, g.front = file;
    }
    else if (const int b = rank_of(kBackTypes, type); b >= 0)
    {
        int r = rank_of(kBackRegions, region);
        const int rank = b * 100 + (r < 0 ? 99 : r);
        if (rank < g.back_rank)
            g.back_rank = rank, g.back = file;
    }
}

std::map<std::string, std::vector<LbGame>> LaunchBoxReader::take()
{
    by_id_.clear();
    return std::move(games_);
}

std::string lb_to_tsv(const std::vector<LbGame> &games)
{
    auto clean = [](const std::string &s)
    {
        std::string o;
        for (char c : s)
            if (c == '\t')
                o += ' ';
            else if (c == '\r')
                ;
            else if (c == '\n')
                o += "\\n";
            else
                o += c;
        return o;
    };
    std::string out;
    for (const auto &g : games)
    {
        const std::string fields[] = {g.id,       g.name,      g.front,   g.back,   g.year,
                                      g.genres,   g.developer, g.publisher, g.players, g.rating,
                                      g.esrb,     g.coop ? "1" : "0", g.overview};
        for (std::size_t i = 0; i < std::size(fields); ++i)
            out += (i ? "\t" : "") + clean(fields[i]);
        out += '\n';
    }
    return out;
}

std::vector<LbGame> lb_from_tsv(std::string_view text)
{
    std::vector<LbGame> out;
    std::size_t start = 0;
    while (start < text.size())
    {
        auto nl = text.find('\n', start);
        if (nl == std::string_view::npos)
            nl = text.size();
        const std::string_view line = text.substr(start, nl - start);
        start = nl + 1;
        std::vector<std::string> f;
        std::size_t s = 0;
        for (;;)
        {
            const auto t = line.find('\t', s);
            f.emplace_back(line.substr(s, t == std::string_view::npos ? std::string_view::npos : t - s));
            if (t == std::string_view::npos)
                break;
            s = t + 1;
        }
        if (f.size() < 13)
            continue;
        LbGame g;
        g.id = f[0], g.name = f[1], g.front = f[2], g.back = f[3], g.year = f[4], g.genres = f[5], g.developer = f[6];
        g.publisher = f[7], g.players = f[8], g.rating = f[9], g.esrb = f[10], g.coop = f[11] == "1";
        std::string ov;
        for (std::size_t i = 0; i < f[12].size(); ++i)
            if (f[12][i] == '\\' && i + 1 < f[12].size() && f[12][i + 1] == 'n')
                ov += '\n', ++i;
            else
                ov += f[12][i];
        g.overview = std::move(ov);
        g.norm = art_norm(g.name);
        out.push_back(std::move(g));
    }
    return out;
}

const LbGame *launchbox_match(const std::vector<LbGame> &games, std::string_view title)
{
    std::vector<ArtEntry> entries;
    entries.reserve(games.size());
    for (const auto &g : games)
        entries.push_back({g.norm, g.name, (g.front.empty() ? 0 : 1) + (g.back.empty() ? 0 : 1)});
    int i = match_name(entries, title);
    if (i < 0)
    {
        // the part before " + ", " & " or " and " must match exactly (tools/boxshelf_covers.py)
        std::string t(title);
        std::size_t cut = std::string::npos;
        for (const char *sep : {" + ", " & ", " and ", " And "})
            if (const auto p = t.find(sep); p != std::string::npos)
                cut = std::min(cut, p);
        if (cut != std::string::npos)
            i = match_title(entries, t.substr(0, cut), true);
    }
    return i < 0 ? nullptr : &games[static_cast<std::size_t>(i)];
}

std::string launchbox_image_url(std::string_view file)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string out = "https://images.launchbox-app.com/";
    for (unsigned char c : file)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/')
            out += static_cast<char>(c);
        else
            out += '%', out += hex[c >> 4], out += hex[c & 15];
    }
    return out;
}
} // namespace boxshelf::data
