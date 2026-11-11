// Boxshelf PS5 - box art fetcher. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/art_fetcher.hpp"
#include "app/store.hpp"

#include "art_codec.h"
#include "data/art_match.hpp"
#include "data/cover_index.hpp"
#include "net/https.hpp"
#include "platform/ps5/system.hpp"

#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#include "../third_party/miniz/miniz.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>

namespace boxshelf::app
{
namespace
{
constexpr long kIndexDays = 7, kMissDays = 7;

// the store (store.hpp): /data/vitrine on the console
std::string read_text(const std::string &path)
{
    std::string out;
    store_read(path, out);
    return out;
}

bool write_atomic(const std::string &path, const std::string &data)
{
    return store_write(path, data);
}

bool fresh(const std::string &path, long days)
{
    long long mtime = 0;
    return store_stat(path, nullptr, &mtime) && std::time(nullptr) - mtime < days * 86400;
}

bool present(const std::string &path)
{
    long long size = 0;
    return store_stat(path, &size) && size > 0;
}

void append_jpeg(void *user, void *data, int size)
{
    static_cast<std::string *>(user)->append(static_cast<const char *>(data), static_cast<std::size_t>(size));
}

// GameTDB keys GameCube under its Wii art; the regions a disc was sold in, from its id (covers.js TDB_REGIONS).
std::vector<std::string> tdb_regions(const std::string &id)
{
    switch (id.size() >= 4 ? id[3] : '?')
    {
    case 'E':
    case 'N':
        return {"US"};
    case 'J':
        return {"JA"};
    case 'K':
        return {"KO"};
    case 'P':
        return {"EN", "FR", "DE", "ES", "IT", "NL"};
    case 'D':
        return {"DE", "EN"};
    case 'F':
        return {"FR", "EN"};
    case 'S':
        return {"ES", "EN"};
    case 'I':
        return {"IT", "EN"};
    case 'H':
        return {"NL", "EN"};
    default:
        return {"US", "EN"};
    }
}
} // namespace

std::string storage_root()
{
    return store_root();
}

std::string downloaded_cover_path(const std::string &game_id)
{
    return storage_root() + "/covers/" + data::cover_key(game_id) + ".jpg";
}

std::string downloaded_back_path(const std::string &game_id)
{
    return storage_root() + "/covers/" + data::cover_key(game_id) + "-back.jpg";
}

bool back_source(std::string_view key)
{
    return key == "gc" || key == "wii" || key == "sw" || key == "n3ds";
}

std::string info_path()
{
    return storage_root() + "/info.tsv";
}

std::vector<std::pair<std::string, data::Game::Info>> read_info()
{
    std::vector<std::pair<std::string, data::Game::Info>> out;
    const std::string text = read_text(info_path());
    std::size_t start = 0;
    while (start < text.size())
    {
        auto nl = text.find('\n', start);
        if (nl == std::string::npos)
            nl = text.size();
        const std::string line = text.substr(start, nl - start);
        start = nl + 1;
        std::vector<std::string> f;
        std::size_t s = 0;
        for (;;)
        {
            const auto t = line.find('\t', s);
            f.push_back(line.substr(s, t == std::string::npos ? std::string::npos : t - s));
            if (t == std::string::npos)
                break;
            s = t + 1;
        }
        if (f.size() < 11)
            continue;
        data::Game::Info in;
        in.known = true;
        in.year = f[1], in.genres = f[2], in.developer = f[3], in.publisher = f[4], in.players = f[5], in.rating = f[6];
        in.esrb = f[7], in.coop = f[8] == "1";
        for (std::size_t i = 0; i < f[9].size(); ++i)
            if (f[9][i] == '\\' && i + 1 < f[9].size() && f[9][i + 1] == 'n')
                in.overview += '\n', ++i;
            else
                in.overview += f[9][i];
        out.emplace_back(f[0], std::move(in));
    }
    return out;
}

void apply_info(std::vector<data::Game> &games)
{
    const auto all = read_info();
    std::map<std::string, const data::Game::Info *> by_id;
    for (const auto &[id, in] : all)
        by_id[id] = &in;
    for (auto &g : games)
        if (const auto it = by_id.find(g.id); it != by_id.end())
            g.info = *it->second;
}

void apply_downloaded_art(std::vector<data::Game> &games)
{
    for (auto &g : games)
    {
        const std::string front = downloaded_cover_path(g.id), back = downloaded_back_path(g.id);
        if (g.cover_back.empty() && present(back))
            g.cover_back = back;
        if (g.cover_front.rfind("/data/boxshelf/covers/", 0) != 0 && present(front))
        {
            if (!g.cover_front.empty() && g.cover_front != front)
                g.cover_fallback = g.cover_front, g.fallback_square = g.cover_square;
            g.cover_front = front;
            g.cover_square = false;
        }
    }
}

std::vector<ArtJob> art_jobs(const std::vector<data::Game> &games, bool everything, bool backs)
{
    std::vector<ArtJob> jobs;
    auto exists = [](const std::string &path) { return present(path); };
    for (const auto &g : games)
    {
        const std::string key(data::style(g.system).key);
        const bool installed = g.path.rfind("/appdb/", 0) == 0;
        const bool launchbox = !data::launchbox_platform(key).empty();
        // installed apps and ports keep their store art; installed PS5 / PS4 games get their box from LaunchBox
        if (installed && g.system != data::System::ps5 && g.system != data::System::ps4)
            continue;
        const bool source = launchbox || !data::libretro_system(key).empty() || !data::gametdb_art(key).empty() ||
                            g.system == data::System::gc || g.system == data::System::wii;
        if (!source)
            continue;
        ArtJob j{g.id, key, g.title, g.region, g.disc_id};
        if (g.system == data::System::wiiu)
        {
            // a Wii U game folder carries its product code: "New Super Mario Bros. U + New Super Luigi U [ATWP01]"
            j.disc_id.clear();
            const auto open = g.path.rfind('[');
            if (open != std::string::npos && open + 7 < g.path.size() && g.path[open + 7] == ']')
            {
                const std::string id = g.path.substr(open + 1, 6);
                if (std::all_of(id.begin(), id.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)); }))
                    j.disc_id = id;
            }
        }
        const bool own = g.cover_front.rfind("/data/boxshelf/covers/", 0) == 0;
        j.front = everything ? !own : !own && !exists(downloaded_cover_path(g.id));
        const bool back_here = !g.cover_back.empty() && g.cover_back != downloaded_back_path(g.id);
        j.back = backs && (back_source(key) || launchbox) && !back_here && (everything || !exists(downloaded_back_path(g.id)));
        j.info = launchbox && (everything || !g.info.known);
        if (j.front || j.back || j.info)
            jobs.push_back(std::move(j));
    }
    return jobs;
}

ArtFetcher::ArtFetcher()
{
    pthread_mutex_init(&lock_, nullptr);
}

void ArtFetcher::start(std::vector<ArtJob> jobs)
{
    pthread_mutex_lock(&lock_);
    if (started_)
    {
        pthread_mutex_unlock(&lock_);
        return;
    }
    started_ = true;
    running_ = true;
    jobs_ = std::move(jobs);
    pthread_mutex_unlock(&lock_);
    launch();
}

bool ArtFetcher::restart(std::vector<ArtJob> jobs)
{
    pthread_mutex_lock(&lock_);
    if (running_)
    {
        pthread_mutex_unlock(&lock_);
        return false;
    }
    started_ = running_ = true;
    retry_misses_ = true;
    jobs_ = std::move(jobs);
    pthread_mutex_unlock(&lock_);
    launch();
    return true;
}

bool ArtFetcher::running()
{
    pthread_mutex_lock(&lock_);
    const bool r = running_;
    pthread_mutex_unlock(&lock_);
    return r;
}

void ArtFetcher::launch()
{
    pthread_t t;
    if (pthread_create(&t, nullptr, &ArtFetcher::run, this) == 0)
        pthread_detach(t);
    else
    {
        pthread_mutex_lock(&lock_);
        running_ = false;
        pthread_mutex_unlock(&lock_);
    }
}

namespace
{
struct LbFeed
{
    data::LaunchBoxReader *reader;
    bool ok = true;
};
bool lb_feed(void *user, const char *data, std::size_t size)
{
    auto *f = static_cast<LbFeed *>(user);
    f->ok = f->reader->feed(data, size);
    return f->ok && !f->reader->done(); // stop once Metadata.xml has been read (the rest of the zip isn't needed)
}
constexpr long kLaunchBoxDays = 60;
} // namespace

bool ArtFetcher::launchbox_ready(const std::vector<std::string> &systems)
{
    std::vector<std::string> missing;
    for (const auto &k : systems)
    {
        if (lb_.count(k) || data::launchbox_platform(k).empty())
            continue;
        const std::string cache = storage_root() + "/launchbox2-" + k + ".tsv"; // 2: "best|next" pictures
        if (fresh(cache, kLaunchBoxDays))
        {
            lb_[k] = data::lb_from_tsv(read_text(cache));
            if (!lb_[k].empty())
                continue;
            lb_.erase(k);
        }
        missing.push_back(k);
    }
    if (missing.empty())
        return true;
    if (lb_tried_)
        return false;
    lb_tried_ = true;
    // covers.js: the LaunchBox Games Database, read as it downloads (~110 MB, never kept)
    std::vector<std::string> platforms;
    for (const auto &k : missing)
        platforms.emplace_back(data::launchbox_platform(k));
    pthread_mutex_lock(&lock_);
    status_ = "LaunchBox";
    pthread_mutex_unlock(&lock_);
    data::LaunchBoxReader reader(platforms);
    LbFeed feed{&reader};
    std::string error;
    long status = 0;
    const bool got = net::https_stream("https://gamesdb.launchbox-app.com/Metadata.zip", lb_feed, &feed, &status, &error,
                                       900000);
    if (!got || !feed.ok || !reader.done())
    {
        hui::sys::log("[BOXSHELF] art: LaunchBox read failed: %s", error.empty() ? "incomplete" : error.c_str());
        return false;
    }
    auto read = reader.take();
    for (const auto &k : missing)
    {
        auto &list = read[std::string(data::launchbox_platform(k))];
        write_atomic(storage_root() + "/launchbox2-" + k + ".tsv", data::lb_to_tsv(list));
        hui::sys::log("[BOXSHELF] art: LaunchBox %s: %zu games", k.c_str(), list.size());
        lb_[k] = std::move(list);
    }
    return true;
}

const data::LbGame *ArtFetcher::launchbox_game(const ArtJob &job)
{
    const auto it = lb_.find(job.system);
    return it == lb_.end() ? nullptr : data::launchbox_match(it->second, job.title);
}

namespace
{
// "best|next" -> {"best", "next"}
std::vector<std::string> split_bar(const std::string &files)
{
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start < files.size())
    {
        auto bar = files.find('|', start);
        if (bar == std::string::npos)
            bar = files.size();
        if (bar > start)
            out.push_back(files.substr(start, bar - start));
        start = bar + 1;
    }
    return out;
}
} // namespace

bool ArtFetcher::fetch_back(const ArtJob &job)
{
    // GameTDB's full cover scan (back | spine | front), the back cut by the spine's folds (Boxshelf's cutBack). The
    // case shapes are main.js CASE: front width and spine thickness relative to the height.
    float front = .711f, spine = .074f;
    std::vector<std::string> urls;
    if ((job.system == "gc" || job.system == "wii") && job.disc_id.size() == 6)
    {
        for (const auto &r : tdb_regions(job.disc_id))
            for (const char *kind : {"coverfullHQ", "coverfull"})
                urls.push_back(std::string("https://art.gametdb.com/wii/") + kind + "/" + r + "/" + job.disc_id + ".png");
    }
    else if (job.system == "wiiu" && job.disc_id.size() == 6)
    {
        for (const auto &r : tdb_regions(job.disc_id))
            for (const char *kind : {"coverfullHQ", "coverfull"})
                urls.push_back(std::string("https://art.gametdb.com/wiiu/") + kind + "/" + r + "/" + job.disc_id + ".jpg");
    }
    else if ((job.system == "sw" || job.system == "n3ds") && gametdb_ready(job.system))
    {
        if (job.system == "sw")
            front = .618f, spine = .059f;
        else
            front = 1.127f, spine = .112f;
        for (const auto &[k, db] : tdb_)
            if (k == job.system)
                urls = data::gametdb_full_urls(db, job.system, job.title);
    }
    for (const auto &url : urls)
    {
        std::string body, error, jpeg;
        long status = 0;
        if (!net::https_get(url, body, 12u << 20, &status, &error, 60000))
            continue;
        if (!art_back_to_jpeg(reinterpret_cast<const unsigned char *>(body.data()), body.size(), front, spine, 1200,
                              append_jpeg, &jpeg))
            continue;
        const std::string path = downloaded_back_path(job.id);
        if (!write_atomic(path, jpeg))
            return false;
        pthread_mutex_lock(&lock_);
        done_.emplace_back(job.id + "|back", path);
        pthread_mutex_unlock(&lock_);
        return true;
    }
    // LaunchBox's Box - Back (English-only regions first, as covers.js does)
    const data::LbGame *lb = launchbox_game(job);
    for (const std::string &file : lb ? split_bar(lb->back) : std::vector<std::string>{})
    {
        std::string body, error, jpeg;
        long status = 0;
        if (net::https_get(data::launchbox_image_url(file), body, 12u << 20, &status, &error, 60000) &&
            art_shrink_to_jpeg(reinterpret_cast<const unsigned char *>(body.data()), body.size(), 1200, append_jpeg, &jpeg))
        {
            const std::string path = downloaded_back_path(job.id);
            if (!write_atomic(path, jpeg))
                return false;
            pthread_mutex_lock(&lock_);
            done_.emplace_back(job.id + "|back", path);
            pthread_mutex_unlock(&lock_);
            return true;
        }
    }
    return false;
}

std::vector<std::pair<std::string, std::string>> ArtFetcher::take()
{
    pthread_mutex_lock(&lock_);
    auto out = std::move(done_);
    done_.clear();
    pthread_mutex_unlock(&lock_);
    return out;
}

std::string ArtFetcher::status()
{
    pthread_mutex_lock(&lock_);
    std::string s = status_;
    pthread_mutex_unlock(&lock_);
    return s;
}

void *ArtFetcher::run(void *self)
{
    static_cast<ArtFetcher *>(self)->work();
    return nullptr;
}

std::vector<std::string> ArtFetcher::libretro_names(const std::string &system)
{
    const std::string cache = storage_root() + "/libretro-" + system + ".txt";
    std::vector<std::string> names;
    if (fresh(cache, kIndexDays))
    {
        const std::string text = read_text(cache);
        std::size_t start = 0;
        for (std::size_t nl = text.find('\n'); nl != std::string::npos; start = nl + 1, nl = text.find('\n', start))
            if (nl > start)
                names.push_back(text.substr(start, nl - start));
        if (!names.empty())
            return names;
    }
    std::string html, error;
    const std::string url = data::libretro_url(system, "x");
    const std::string index = url.substr(0, url.rfind('/') + 1); // ".../Named_Boxarts/"
    if (!net::https_get(index, html, 12u << 20, nullptr, &error, 60000))
    {
        hui::sys::log("[BOXSHELF] art: libretro %s index failed: %s", system.c_str(), error.c_str());
        return names;
    }
    names = data::parse_libretro_index(html);
    std::string text;
    for (const auto &n : names)
        text += n + "\n";
    if (!names.empty())
        write_atomic(cache, text);
    hui::sys::log("[BOXSHELF] art: libretro %s index: %zu names", system.c_str(), names.size());
    return names;
}

bool ArtFetcher::gametdb_ready(const std::string &system)
{
    for (const auto &[k, v] : tdb_)
        if (k == system)
            return !v.empty();
    std::vector<data::TdbGame> db;
    const std::string cache = storage_root() + "/gametdb-" + system + ".txt";
    if (fresh(cache, kIndexDays))
    {
        const std::string text = read_text(cache);
        std::size_t start = 0;
        for (std::size_t nl = text.find('\n'); nl != std::string::npos; start = nl + 1, nl = text.find('\n', start))
        {
            const std::string line = text.substr(start, nl - start);
            const auto t1 = line.find('\t'), t2 = line.find('\t', t1 + 1);
            if (t1 != std::string::npos && t2 != std::string::npos)
                db.push_back({line.substr(0, t1), line.substr(t1 + 1, t2 - t1 - 1), line.substr(t2 + 1)});
        }
    }
    if (db.empty())
    {
        // gametdb.js DB: the zipped title database, at most once a week
        const std::string url = system == "sw" ? "https://www.gametdb.com/switchtdb.zip" : "https://www.gametdb.com/3dstdb.zip";
        const std::vector<std::string> types = system == "sw" ? std::vector<std::string>{"Switch"}
                                                              : std::vector<std::string>{"3DS", "New3DS"};
        std::string zip, error;
        // gametdb.com can be slow (3.8 MB took 81 s on 2026-11-10): a generous limit, once a week
        if (net::https_get(url, zip, 32u << 20, nullptr, &error, 300000))
        {
            mz_zip_archive archive{};
            if (mz_zip_reader_init_mem(&archive, zip.data(), zip.size(), 0))
            {
                for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&archive); ++i)
                {
                    char name[256];
                    mz_zip_reader_get_filename(&archive, i, name, sizeof name);
                    const std::string n(name);
                    if (n.size() < 4 || n.compare(n.size() - 4, 4, ".xml") != 0)
                        continue;
                    // read as it inflates, a few games at a time: the whole XML (25 MB for Switch) does not fit
                    // beside the rest in the app's 128 MB heap
                    struct Feed
                    {
                        std::string buf;
                        const std::vector<std::string> *types;
                        std::vector<data::TdbGame> *db;
                    } feed{{}, &types, &db};
                    auto take = [](void *user, mz_uint64, const void *data, size_t n) -> size_t
                    {
                        auto *f = static_cast<Feed *>(user);
                        f->buf.append(static_cast<const char *>(data), n);
                        const auto last = f->buf.rfind("</game>");
                        if (last != std::string::npos)
                        {
                            const std::size_t end = last + 7;
                            for (auto &g : data::parse_gametdb(std::string_view(f->buf.data(), end), *f->types))
                                f->db->push_back(std::move(g));
                            f->buf.erase(0, end);
                        }
                        return n;
                    };
                    mz_zip_reader_extract_to_callback(&archive, i, take, &feed, 0);
                    break;
                }
                mz_zip_reader_end(&archive);
            }
        }
        else
            hui::sys::log("[BOXSHELF] art: GameTDB %s failed: %s", system.c_str(), error.c_str());
        if (db.size() >= 100)
        {
            std::string text;
            for (const auto &g : db)
                text += g.norm + "\t" + g.id + "\t" + g.region + "\n";
            write_atomic(cache, text);
        }
        hui::sys::log("[BOXSHELF] art: GameTDB %s: %zu boxed games", system.c_str(), db.size());
    }
    tdb_.emplace_back(system, std::move(db));
    return !tdb_.back().second.empty();
}

bool ArtFetcher::fetch_one(const ArtJob &job, const std::vector<std::string> &names)
{
    std::vector<std::string> urls;
    // 1. libretro (clean fronts, matched by name)
    if (!names.empty())
    {
        const std::string hit = data::libretro_match(names, job.title, job.region);
        if (!hit.empty())
            urls.push_back(data::libretro_url(job.system, hit));
    }
    // 2. LaunchBox's Box - Front (the only source with PS5 boxes)
    if (const data::LbGame *lb = launchbox_game(job))
        for (const auto &file : split_bar(lb->front))
            urls.push_back(data::launchbox_image_url(file));
    // 3. GameTDB HD cover by disc id (GameCube / Wii)
    if ((job.system == "gc" || job.system == "wii") && job.disc_id.size() == 6)
        for (const auto &r : tdb_regions(job.disc_id))
            urls.push_back("https://art.gametdb.com/wii/coverHQ/" + r + "/" + job.disc_id + ".png");
    if (job.system == "wiiu" && job.disc_id.size() == 6)
        for (const auto &r : tdb_regions(job.disc_id))
            for (const char *kind : {"coverHQ", "cover"})
                urls.push_back(std::string("https://art.gametdb.com/wiiu/") + kind + "/" + r + "/" + job.disc_id + ".jpg");
    // 3. GameTDB by title (Switch, 3DS: gametdb.js)
    if (!data::gametdb_art(job.system).empty() && gametdb_ready(job.system))
        for (const auto &[k, db] : tdb_)
            if (k == job.system)
                for (auto &u : data::gametdb_front_urls(db, job.system, job.title))
                    urls.push_back(std::move(u));
    for (const auto &url : urls)
    {
        std::string body, error, jpeg;
        long status = 0;
        if (!net::https_get(url, body, 12u << 20, &status, &error, 60000))
            continue;
        if (!art_shrink_to_jpeg(reinterpret_cast<const unsigned char *>(body.data()), body.size(), 1200, append_jpeg,
                                &jpeg))
            continue;
        const std::string path = downloaded_cover_path(job.id);
        if (!write_atomic(path, jpeg))
        {
            hui::sys::log("[BOXSHELF] art: cannot write %s", path.c_str());
            return false;
        }
        pthread_mutex_lock(&lock_);
        done_.emplace_back(job.id, path);
        pthread_mutex_unlock(&lock_);
        return true;
    }
    return false;
}

void ArtFetcher::work()
{
    const std::string root = storage_root();
    // mkdir -p: every level of root/covers
    const std::string covers = root + "/covers";
    store_mkdirs(covers);
    // Games tried recently and not found: "<key> <unix time>" lines.
    // The list is dropped when the set of sources changes (a game missed before may be found now).
    static const std::string kMissesHeader = "# boxshelf art misses v2 (libretro, gametdb-disc, gametdb-title)\n";
    std::map<std::string, long> misses;
    {
        std::string text = read_text(root + "/misses.txt");
        if (text.rfind(kMissesHeader, 0) != 0)
            text.clear();
        else
            text.erase(0, kMissesHeader.size());
        std::size_t start = 0;
        for (std::size_t nl = text.find('\n'); nl != std::string::npos; start = nl + 1, nl = text.find('\n', start))
        {
            const std::string line = text.substr(start, nl - start);
            const auto sp = line.rfind(' ');
            if (sp != std::string::npos)
                misses[line.substr(0, sp)] = std::atol(line.c_str() + sp + 1);
        }
    }
    const long now = static_cast<long>(std::time(nullptr));
    pthread_mutex_lock(&lock_);
    const bool retry = retry_misses_;
    std::vector<ArtJob> all = jobs_;
    pthread_mutex_unlock(&lock_);
    auto missed = [&](const std::string &key)
    {
        const auto m = misses.find(key);
        return !retry && m != misses.end() && now - m->second <= kMissDays * 86400;
    };
    std::vector<ArtJob> jobs;
    for (auto j : all)
    {
        if (j.front && missed(data::cover_key(j.id)))
            j.front = false;
        if (j.back && missed(data::cover_key(j.id) + "-back"))
            j.back = false;
        if (j.front || j.back)
            jobs.push_back(std::move(j));
    }
    hui::sys::log("[BOXSHELF] art: %zu games to look for (%zu skipped, not found this week)", jobs.size(),
                  all.size() - jobs.size());
    net::https_init();
    std::map<std::string, std::vector<std::string>> lists;
    int found = 0, done = 0;
    {
        std::vector<std::string> systems;
        for (const auto &j : jobs)
            if (!data::launchbox_platform(j.system).empty() &&
                std::find(systems.begin(), systems.end(), j.system) == systems.end())
                systems.push_back(j.system);
        if (!systems.empty())
            launchbox_ready(systems);
        // Memory: the whole platforms are ~28,000 games with their overviews (tens of MB of a 128 MB heap); only the
        // ones matching this library are kept (each job finds the same entry in the smaller list)
        std::map<std::string, std::vector<data::LbGame>> kept;
        std::size_t before = 0;
        for (const auto &[k, list] : lb_)
            before += list.size(), kept[k]; // every platform stays known, even with none of its games here
        for (const auto &j : all)
            if (const data::LbGame *lb = launchbox_game(j))
            {
                auto &v = kept[j.system];
                if (std::none_of(v.begin(), v.end(), [&](const data::LbGame &g) { return g.id == lb->id; }))
                    v.push_back(*lb);
            }
        std::size_t after = 0;
        for (const auto &[k, list] : kept)
            after += list.size();
        lb_.clear();
        for (auto &[k, list] : kept)
            lb_[k] = std::move(list);
        hui::sys::log("[BOXSHELF] art: LaunchBox kept %zu of %zu games", after, before);
    }
    // the info file, kept whole: what is there, plus what this run finds
    std::map<std::string, std::string> info_lines;
    {
        const std::string text = read_text(info_path());
        std::size_t start = 0;
        while (start < text.size())
        {
            auto nl = text.find('\n', start);
            if (nl == std::string::npos)
                nl = text.size();
            const std::string line = text.substr(start, nl - start);
            if (const auto tab = line.find('\t'); tab != std::string::npos)
                info_lines[line.substr(0, tab)] = line;
            start = nl + 1;
        }
    }
    bool info_changed = false;
    auto clean = [](const std::string &v)
    {
        std::string o;
        for (char c : v)
            if (c == '\t')
                o += ' ';
            else if (c == '\n')
                o += "\\n";
            else if (c != '\r')
                o += c;
        return o;
    };
    for (const auto &job : jobs)
    {
        ++done;
        if (job.info && lb_.count(job.system))
        {
            // the game's info: found or not, it is written so the next start doesn't ask again
            const data::LbGame *lb = launchbox_game(job);
            std::string line = job.id;
            const std::string fields[] = {lb ? lb->year : "",     lb ? lb->genres : "",  lb ? lb->developer : "",
                                          lb ? lb->publisher : "", lb ? lb->players : "", lb ? lb->rating : "",
                                          lb ? lb->esrb : "",     lb && lb->coop ? "1" : "0", lb ? lb->overview : "",
                                          lb ? lb->id : ""};
            for (const auto &f : fields)
                line += "\t" + clean(f);
            info_lines[job.id] = line;
            info_changed = true;
        }
        pthread_mutex_lock(&lock_);
        status_ = std::to_string(done) + " / " + std::to_string(jobs.size());
        pthread_mutex_unlock(&lock_);
        const bool uses_libretro = !data::libretro_system(job.system).empty();
        const bool uses_tdb = !data::gametdb_art(job.system).empty();
        if (uses_libretro && !lists.count(job.system))
            lists[job.system] = libretro_names(job.system);
        if (job.back)
        {
            if (fetch_back(job))
                ++found;
            else
            {
                hui::sys::log("[BOXSHELF] art: no back for %s (%s)", job.title.c_str(), job.system.c_str());
                if (job.system == "gc" || job.system == "wii" || lb_.count(job.system) ||
                    (!data::gametdb_art(job.system).empty() && gametdb_ready(job.system)))
                    misses[data::cover_key(job.id) + "-back"] = now; // only when a source answered
            }
        }
        if (job.front && fetch_one(job, lists[job.system]))
            ++found;
        else if (job.front)
        {
            hui::sys::log("[BOXSHELF] art: no box art for %s (%s)", job.title.c_str(), job.system.c_str());
            // a "not found" only counts when every source this system uses could be asked: a list that failed
            // to download (network, a slow site) must not keep the game waiting a week
            const bool sources_up = (!uses_libretro || !lists[job.system].empty()) || (uses_tdb && gametdb_ready(job.system));
            const bool tdb_up = !uses_tdb || gametdb_ready(job.system);
            if (sources_up && tdb_up)
                misses[data::cover_key(job.id)] = now;
        }
        if (job.front || job.back)
            usleep(100000); // gentle with the free services
    }
    if (info_changed)
    {
        std::string all;
        for (const auto &[id, line] : info_lines)
            all += line + "\n";
        if (write_atomic(info_path(), all))
        {
            pthread_mutex_lock(&lock_);
            done_.emplace_back("|info", info_path()); // the shelves take the new info
            pthread_mutex_unlock(&lock_);
        }
    }
    std::string text = kMissesHeader;
    for (const auto &[key, when] : misses)
        text += key + " " + std::to_string(when) + "\n";
    write_atomic(root + "/misses.txt", text);
    hui::sys::log("[BOXSHELF] art: %d pictures downloaded for %zu games", found, jobs.size());
    pthread_mutex_lock(&lock_);
    status_.clear();
    found_ = found;
    running_ = false;
    pthread_mutex_unlock(&lock_);
}
} // namespace boxshelf::app
