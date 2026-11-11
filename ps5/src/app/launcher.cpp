// Boxshelf PS5 - starts a game from the shelf (route from Estante's main.cpp). SPDX-License-Identifier: GPL-3.0-or-later
#include "app/launcher.hpp"

#include "app/elf_client.hpp"
#include "app/i18n.hpp"
#include "cJSON.h"

#include <strings.h>
#include <unistd.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef BOXSHELF_HOST_PREVIEW
extern "C"
{
    int sceNetSocket(const char *, int, int, int);
    int sceNetConnect(int, const void *, std::uint32_t);
    int sceNetSend(int, const void *, std::size_t, int);
    int sceNetRecv(int, void *, std::size_t, int);
    int sceNetSetsockopt(int, int, int, const void *, std::uint32_t);
    int sceNetSocketClose(int);
}
#endif

namespace boxshelf::app
{
namespace
{
#ifndef BOXSHELF_HOST_PREVIEW
struct NetAddress
{
    std::uint8_t length, family;
    std::uint16_t port;
    std::uint32_t address;
    std::uint16_t virtual_port;
    std::uint8_t zero[6];
};

int connect_local(unsigned port)
{
    const int s = sceNetSocket("boxshelf", 2, 1, 6);
    if (s < 0)
        return -1;
    const int timeout = 3000000;
    for (int opt : {0x1105, 0x1106, 0x1109})
        sceNetSetsockopt(s, 0xffff, opt, &timeout, sizeof timeout);
    NetAddress address{sizeof(NetAddress), 2, static_cast<std::uint16_t>((port << 8) | (port >> 8)), 0x0100007f, 0, {}};
    if (sceNetConnect(s, &address, sizeof address) < 0)
    {
        sceNetSocketClose(s);
        return -1;
    }
    return s;
}

bool send_all(int s, const char *data, std::size_t length)
{
    std::size_t off = 0;
    while (off < length)
    {
        const int n = sceNetSend(s, data + off, length - off, 0);
        if (n <= 0)
            return false;
        off += static_cast<std::size_t>(n);
    }
    return true;
}

// Sends one FTP command (or nothing) and reads the whole reply; its code, 0 when the line never came.
int ftp_reply(int control, const char *command, char *text, std::size_t cap)
{
    if (command && !send_all(control, command, std::strlen(command)))
        return 0;
    std::size_t used = 0;
    text[0] = 0;
    for (int guard = 0; guard < 40 && used + 1 < cap; guard++)
    {
        const int n = sceNetRecv(control, text + used, cap - 1 - used, 0);
        if (n <= 0)
            return 0;
        used += static_cast<std::size_t>(n);
        text[used] = 0;
        const char *line = text;
        while (*line)
        {
            const char *eol = std::strchr(line, '\n');
            if (!eol)
                break;
            if (std::isdigit(static_cast<unsigned char>(line[0])) && std::isdigit(static_cast<unsigned char>(line[1])) &&
                std::isdigit(static_cast<unsigned char>(line[2])) && line[3] == ' ')
                return std::atoi(line);
            line = eol + 1;
        }
    }
    return 0;
}

// Writes a small text file through Elf Arsenal's FTP server on this console (port 2121, anonymous).
bool ftp_store_text(const char *remote, const std::string &content)
{
    const int control = connect_local(2121);
    if (control < 0)
        return false;
    char text[512];
    bool ok = ftp_reply(control, nullptr, text, sizeof text) == 220;
    if (ok)
    {
        int code = ftp_reply(control, "USER anonymous\r\n", text, sizeof text);
        if (code == 331)
            code = ftp_reply(control, "PASS x\r\n", text, sizeof text);
        ok = code == 230;
    }
    ok = ok && ftp_reply(control, "TYPE I\r\n", text, sizeof text) == 200;
    unsigned h[6]{};
    ok = ok && ftp_reply(control, "PASV\r\n", text, sizeof text) == 227;
    const char *open = ok ? std::strchr(text, '(') : nullptr;
    ok = ok && open && std::sscanf(open + 1, "%u,%u,%u,%u,%u,%u", &h[0], &h[1], &h[2], &h[3], &h[4], &h[5]) == 6;
    if (ok)
    {
        const int data = connect_local((h[4] << 8) | h[5]);
        char command[600];
        std::snprintf(command, sizeof command, "STOR %s\r\n", remote);
        const int code = data < 0 ? 0 : ftp_reply(control, command, text, sizeof text);
        ok = data >= 0 && (code == 150 || code == 125);
        if (ok)
            ok = send_all(data, content.data(), content.size());
        if (data >= 0)
            sceNetSocketClose(data);
        ok = ok && ftp_reply(control, nullptr, text, sizeof text) == 226;
    }
    ftp_reply(control, "QUIT\r\n", text, sizeof text);
    sceNetSocketClose(control);
    return ok;
}
#endif

bool ends_with_any(const std::string &name, const char *const *extensions, int count)
{
    for (int i = 0; i < count; i++)
    {
        const std::size_t n = std::strlen(extensions[i]);
        if (name.size() > n && !strcasecmp(name.c_str() + name.size() - n, extensions[i]))
            return true;
    }
    return false;
}

// Estante's resolve_game_file(): the image a frontend opens from a PS1 or Switch game folder.
bool resolve_game_file(const std::string &folder, bool ps1, std::string &out)
{
    static const char *const ps1_order[] = {".cue", ".chd", ".m3u", ".pbp", ".iso", ".img", ".bin"};
    static const char *const switch_formats[] = {".nsp", ".xci", ".nsz", ".xcz"};
    std::string body;
    if (!elf_get(fs_url("/api/fs/list?path=", folder), body))
        return false;
    cJSON *root = cJSON_ParseWithLength(body.data(), body.size());
    const cJSON *entries = root ? cJSON_GetObjectItemCaseSensitive(root, "entries") : nullptr;
    std::string best;
    int best_rank = 1000;
    double best_size = -1;
    const cJSON *entry = nullptr;
    cJSON_ArrayForEach(entry, entries)
    {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, "name");
        const cJSON *dir = cJSON_GetObjectItemCaseSensitive(entry, "dir");
        const cJSON *size = cJSON_GetObjectItemCaseSensitive(entry, "size");
        if (!cJSON_IsString(name) || !name->valuestring || cJSON_IsTrue(dir))
            continue;
        const std::string n = name->valuestring;
        if (ps1)
        {
            int rank = 1000;
            for (int i = 0; i < 7; i++)
                if (ends_with_any(n, &ps1_order[i], 1))
                    rank = i;
            if (rank < best_rank || (rank == best_rank && rank < 1000 && n < best))
                best_rank = rank, best = n;
        }
        else if (ends_with_any(n, switch_formats, 4))
        {
            const double bytes = cJSON_IsNumber(size) ? size->valuedouble : 0;
            if (bytes > best_size)
                best_size = bytes, best = n;
        }
    }
    cJSON_Delete(root);
    if (best.empty() || (ps1 && best_rank == 1000))
        return false;
    out = folder;
    if (out.empty() || out.back() != '/')
        out += '/';
    out += best;
    return true;
}

bool running_title(std::string &title, bool &running)
{
    std::string body;
    if (!elf_get("/api/state", body))
        return false;
    cJSON *root = cJSON_ParseWithLength(body.data(), body.size());
    if (!root)
        return false;
    const cJSON *flag = cJSON_GetObjectItemCaseSensitive(root, "gameRunning");
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "runningTitleId");
    const bool ok = cJSON_IsBool(flag) && cJSON_IsString(id) && id->valuestring;
    if (ok)
    {
        running = cJSON_IsTrue(flag);
        title = id->valuestring;
    }
    cJSON_Delete(root);
    return ok;
}

// Porpoise's home-screen launcher, patched to open `app` with `target` (Estante's direct_launch_job()).
bool send_direct_launcher(const std::string &app, const std::string &target)
{
#ifdef BOXSHELF_HOST_PREVIEW
    (void)app, (void)target;
    return false;
#else
    std::string elf;
    if (!elf_get("/api/fs/download?path=/data/homebrew/PPSA99764/assets/forwarder/home-launcher.elf", elf, 200000) ||
        elf.size() < 90000 || elf.size() > 140000 || app.size() != 9 || target.size() + 1 > 2040)
        return false;
    static const char marker[] = "PORPOISE-HOME-LAUNCH-V1";
    const auto at = elf.find(std::string(marker, sizeof marker - 1));
    const auto original = elf.find("PPSA99764");
    if (at == std::string::npos || original == std::string::npos || at + 2047 > elf.size())
        return false;
    elf.replace(original, 9, app);
    std::memset(&elf[at], 0, 2047);
    std::memcpy(&elf[at], target.data(), target.size());
    const int loader = connect_local(9021);
    if (loader < 0)
        return false;
    const bool sent = send_all(loader, elf.data(), elf.size());
    sceNetSocketClose(loader);
    return sent;
#endif
}
} // namespace

Launcher::Launcher()
{
    pthread_mutex_init(&lock_, nullptr);
}

Launcher::~Launcher()
{
    if (joinable_)
        pthread_join(thread_, nullptr);
    pthread_mutex_destroy(&lock_);
}

bool Launcher::start(LaunchRequest request)
{
    pthread_mutex_lock(&lock_);
    if (busy_)
    {
        pthread_mutex_unlock(&lock_);
        return false;
    }
    busy_ = true;
    pthread_mutex_unlock(&lock_);
    if (joinable_)
        pthread_join(thread_, nullptr), joinable_ = false;
    request_ = std::move(request);
    say(tr("Checking the console"), true);
    if (pthread_create(&thread_, nullptr, &Launcher::thread_main, this) != 0)
    {
        say(trf("Could not start %s", request_.title), false);
        pthread_mutex_lock(&lock_);
        busy_ = false;
        pthread_mutex_unlock(&lock_);
        return false;
    }
    joinable_ = true;
    return true;
}

PlayClock::PlayClock()
{
    pthread_mutex_init(&lock_, nullptr);
}

PlayClock::~PlayClock()
{
    stop();
    pthread_mutex_destroy(&lock_);
}

void PlayClock::watch(const std::string &app)
{
    stop();
    pthread_mutex_lock(&lock_);
    app_ = app, quit_ = false, done_ = false;
    pthread_mutex_unlock(&lock_);
    joinable_ = pthread_create(&thread_, nullptr, &PlayClock::thread_main, this) == 0;
}

void PlayClock::stop()
{
    pthread_mutex_lock(&lock_);
    quit_ = true;
    pthread_mutex_unlock(&lock_);
    if (joinable_)
        pthread_join(thread_, nullptr), joinable_ = false;
    pthread_mutex_lock(&lock_);
    app_.clear(), done_ = false;
    pthread_mutex_unlock(&lock_);
}

bool PlayClock::watching() const
{
    pthread_mutex_lock(&lock_);
    const bool w = !app_.empty();
    pthread_mutex_unlock(&lock_);
    return w;
}

bool PlayClock::done() const
{
    pthread_mutex_lock(&lock_);
    const bool d = done_;
    pthread_mutex_unlock(&lock_);
    return d;
}

void *PlayClock::thread_main(void *self)
{
    static_cast<PlayClock *>(self)->run();
    return nullptr;
}

void PlayClock::run()
{
    pthread_mutex_lock(&lock_);
    const std::string app = app_;
    pthread_mutex_unlock(&lock_);
    for (int tick = 0;; ++tick)
    {
        for (int i = 0; i < (tick ? 30 : 4); ++i) // 15 s between questions (2 s for the first), quit checked often
        {
            usleep(500000);
            pthread_mutex_lock(&lock_);
            const bool quit = quit_;
            pthread_mutex_unlock(&lock_);
            if (quit)
                return;
        }
        std::string active;
        bool running = false;
        if (!running_title(active, running))
            continue; // no answer: ask again later
        if (running && active == app)
            continue;
        pthread_mutex_lock(&lock_);
        done_ = true;
        pthread_mutex_unlock(&lock_);
        return;
    }
}

bool Launcher::busy() const
{
    pthread_mutex_lock(&lock_);
    const bool b = busy_;
    pthread_mutex_unlock(&lock_);
    return b;
}

Launcher::Status Launcher::status() const
{
    pthread_mutex_lock(&lock_);
    Status s = status_;
    pthread_mutex_unlock(&lock_);
    return s;
}

void Launcher::say(const std::string &message, bool working, bool ok)
{
    pthread_mutex_lock(&lock_);
    status_.message = message;
    status_.working = working;
    status_.ok = ok;
    ++status_.serial;
    pthread_mutex_unlock(&lock_);
}

void *Launcher::thread_main(void *self)
{
    static_cast<Launcher *>(self)->run();
    return nullptr;
}

void Launcher::run()
{
    const LaunchRequest &r = request_;
    auto finish = [&](const std::string &message, bool ok)
    {
        say(message, false, ok);
        pthread_mutex_lock(&lock_);
        busy_ = false;
        pthread_mutex_unlock(&lock_);
    };
#ifdef BOXSHELF_HOST_PREVIEW
    finish(tr("Starting games needs the console"), false);
    return;
#endif
    std::string active;
    bool running = false;
    if (!running_title(active, running))
        return finish(tr("Could not reach the console. Try again."), false);
    if (running && active == r.app)
        return finish(trf("%s is already open", r.title), true);
    if (running && active != "PPSA99850")
        return finish(tr("Another game is open. Close it from the PS5 menu."), false);

    bool direct = false;
    std::string opened = trf("Opening %s", r.title);
    bool sx2 = false;
    if (!r.target.empty())
    {
        std::string target = r.target;
        const bool resolved = !(r.ps1_folder || r.switch_folder) || resolve_game_file(r.target, r.ps1_folder, target);
        if (r.app == "PPSA99203") // PS5SX2 reads the image from disc-launch.txt when it starts
        {
#ifndef BOXSHELF_HOST_PREVIEW
            if (resolved && ftp_store_text("/data/PCSX2/logs/disc-launch.txt", target))
                opened = trf("Opening PS5SX2 with %s", r.title), sx2 = true;
#endif
        }
        else if (resolved)
        {
            say(trf("Starting %s", r.title), true);
            direct = send_direct_launcher(r.app, target);
        }
        if (!direct && !sx2)
            opened = tr("Direct start unavailable. Opening the emulator.");
    }
    if (!direct)
    {
        std::string body;
        say(opened, true);
        if (!elf_get("/launch?titleId=" + r.app, body))
            return finish(tr("The request failed. Your library is untouched."), false);
    }
    // the console confirms: the title is up within a few seconds
    say(direct ? tr("Request sent. Waiting for the game") : tr("Waiting for the PS5"), true);
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        usleep(attempt ? 1500000 : (direct ? 1500000 : 1000000));
        if (running_title(active, running) && running && active == r.app)
            return finish(trf("%s is starting", r.title), true);
    }
    finish(tr("Not confirmed. Check the game in the PS5 menu."), false);
}
} // namespace boxshelf::app
