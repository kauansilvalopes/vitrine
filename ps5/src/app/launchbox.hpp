// Boxshelf PS5 - the LaunchBox Games Database (gamesdb.launchbox-app.com/Metadata.zip), read the way Boxshelf's
// covers.js / lbindex.js use it: box art (fronts, backs) and game info (overview, year, genres, players...).
// The 110 MB zip is never kept: it is read as it downloads, its Metadata.xml inflated piece by piece and only the
// games of the platforms in the library kept (a few MB). SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace boxshelf::data
{
// covers.js LAUNCHBOX: a Boxshelf system key -> the LaunchBox platform ("" when there is none)
std::string_view launchbox_platform(std::string_view system_key);

struct LbGame
{
    std::string id, name, norm;
    std::string front, back;  // image file names (images.launchbox-app.com/<file>), the best region first
    std::string year, genres, developer, publisher, players, rating, esrb, overview;
    bool coop = false;
    int front_rank = 1 << 20, back_rank = 1 << 20; // internal: the best picture seen so far
};

// Metadata.zip, fed in order as it arrives. Keeps the games of `platforms` (LaunchBox names).
class LaunchBoxReader
{
  public:
    explicit LaunchBoxReader(std::vector<std::string> platforms);
    ~LaunchBoxReader();
    LaunchBoxReader(const LaunchBoxReader &) = delete;
    LaunchBoxReader &operator=(const LaunchBoxReader &) = delete;
    // false when the data is not what was expected (the download can stop)
    bool feed(const char *data, std::size_t size);
    // Metadata.xml has been read to its end (the rest of the zip is not needed)
    bool done() const
    {
        return done_;
    }
    // platform -> its games (call once done)
    std::map<std::string, std::vector<LbGame>> take();

  private:
    bool feed_zip(const unsigned char *data, std::size_t size);
    void feed_xml(const char *data, std::size_t size);
    void block(std::string_view text, bool image);

    struct Inflate;
    std::unique_ptr<Inflate> inf_;
    std::vector<std::string> platforms_;
    std::map<std::string, std::vector<LbGame>> games_;
    std::map<std::string, std::pair<std::string, std::size_t>> by_id_; // id -> (platform, index)
    std::string head_, xml_;
    std::size_t left_ = 0; // compressed bytes of Metadata.xml still to come
    int stage_ = 0;        // 0 the local header, 1 Metadata.xml's data, 2 skipping another entry, 3 done
    bool done_ = false, broken_ = false;
};

// The cache file of one platform: one game per line, tab separated, the overview's newlines kept as "\n".
std::string lb_to_tsv(const std::vector<LbGame> &games);
std::vector<LbGame> lb_from_tsv(std::string_view text);
// covers.js launchboxMatch(): the entry for a title (covers.js matchName on the names, then the part before " and "
// exactly, as tools/boxshelf_covers.py does), or null.
const LbGame *launchbox_match(const std::vector<LbGame> &games, std::string_view title);
// images.launchbox-app.com/<file>, percent-encoded
std::string launchbox_image_url(std::string_view file);
} // namespace boxshelf::data
