// Boxshelf PS5 - the store (see store.hpp). SPDX-License-Identifier: GPL-3.0-or-later
#include "app/store.hpp"

#include "app/elf_client.hpp"
#include "cJSON.h"
#include "platform/ps5/system.hpp"

#include <pthread.h>
#include <sys/stat.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>

#ifndef BOXSHELF_HOST
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
enum class Mode
{
    unset,
    local,  // POSIX files under root_
    remote, // Elf Arsenal: HTTP to read and list, FTP to write
};
Mode mode_ = Mode::unset;
std::string root_;
const char *mode_name_ = "download0";
pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER;

// ---- local files ------------------------------------------------------------------------------------------------

bool local_read(const std::string &path, std::string &out, std::size_t max)
{
    out.clear();
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    char chunk[16384];
    std::size_t n;
    while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0 && out.size() < max)
        out.append(chunk, n);
    std::fclose(f);
    return true;
}

bool local_write(const std::string &path, const std::string &data)
{
    const std::string temp = path + ".part";
    FILE *f = std::fopen(temp.c_str(), "wb");
    if (!f)
        return false;
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    if (std::fclose(f) != 0 || !ok)
    {
        std::remove(temp.c_str());
        return false;
    }
    std::remove(path.c_str());
    return std::rename(temp.c_str(), path.c_str()) == 0;
}

void local_mkdirs(const std::string &dir)
{
    for (std::size_t p = dir.find('/', 1);; p = dir.find('/', p + 1))
    {
        mkdir(dir.substr(0, p).c_str(), 0777);
        if (p == std::string::npos)
            break;
    }
}

#ifndef BOXSHELF_HOST
// ---- Elf Arsenal's FTP (127.0.0.1:2121, anonymous) ------------------------------------------------------------------

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
    const int s = sceNetSocket("vitrine-store", 2, 1, 6);
    if (s < 0)
        return -1;
    const int timeout = 10000000; // us
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

// One command (or none) and its whole reply: the code, 0 when no reply line came.
int ftp_reply(int control, const std::string &command, char *text, std::size_t cap)
{
    if (!command.empty() && !send_all(control, command.data(), command.size()))
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

// Logs in; the control socket, -1 when the server is not there.
int ftp_open()
{
    const int control = connect_local(2121);
    if (control < 0)
        return -1;
    char text[512];
    bool ok = ftp_reply(control, "", text, sizeof text) == 220;
    if (ok)
    {
        int code = ftp_reply(control, "USER anonymous\r\n", text, sizeof text);
        if (code == 331)
            code = ftp_reply(control, "PASS vitrine\r\n", text, sizeof text);
        ok = code == 230;
    }
    ok = ok && ftp_reply(control, "TYPE I\r\n", text, sizeof text) == 200;
    if (!ok)
    {
        sceNetSocketClose(control);
        return -1;
    }
    return control;
}

void ftp_close(int control)
{
    char text[256];
    ftp_reply(control, "QUIT\r\n", text, sizeof text);
    sceNetSocketClose(control);
}

// STOR <temp>, then the old file goes and the new one takes its name
bool ftp_write(const std::string &path, const std::string &data)
{
    const int control = ftp_open();
    if (control < 0)
        return false;
    char text[512];
    const std::string temp = path + ".part";
    unsigned h[6]{};
    bool ok = ftp_reply(control, "PASV\r\n", text, sizeof text) == 227;
    const char *open = ok ? std::strchr(text, '(') : nullptr;
    ok = ok && open && std::sscanf(open + 1, "%u,%u,%u,%u,%u,%u", &h[0], &h[1], &h[2], &h[3], &h[4], &h[5]) == 6;
    if (ok)
    {
        const int dsock = connect_local((h[4] << 8) | h[5]);
        const int code = dsock < 0 ? 0 : ftp_reply(control, "STOR " + temp + "\r\n", text, sizeof text);
        ok = dsock >= 0 && (code == 150 || code == 125);
        if (ok)
            ok = send_all(dsock, data.data(), data.size());
        if (dsock >= 0)
            sceNetSocketClose(dsock);
        ok = ok && ftp_reply(control, "", text, sizeof text) == 226;
    }
    if (ok)
    {
        ftp_reply(control, "DELE " + path + "\r\n", text, sizeof text); // not there yet: fine
        ok = ftp_reply(control, "RNFR " + temp + "\r\n", text, sizeof text) == 350 &&
             ftp_reply(control, "RNTO " + path + "\r\n", text, sizeof text) / 100 == 2;
    }
    ftp_close(control);
    return ok;
}

// ---- Elf Arsenal's HTTP: reads and listings, a listing kept per folder ------------------------------------------------

struct Entry
{
    long long size = 0, mtime = 0;
};
std::map<std::string, std::map<std::string, Entry>> listed_; // folder -> name -> entry (under lock_)

void split(const std::string &path, std::string &dir, std::string &name)
{
    const auto slash = path.rfind('/');
    dir = slash == std::string::npos ? std::string() : path.substr(0, slash);
    name = slash == std::string::npos ? path : path.substr(slash + 1);
}

// the folder's listing, read once (under lock_)
std::map<std::string, Entry> &listing(const std::string &dir)
{
    auto it = listed_.find(dir);
    if (it != listed_.end())
        return it->second;
    std::map<std::string, Entry> entries;
    std::string body;
    if (!elf_get(fs_url("/api/fs/list?path=", dir), body, 16u << 20) && body.empty())
    {
        // no answer at all: not kept, the next question asks again
        static std::map<std::string, Entry> none;
        none.clear();
        return none;
    }
    if (cJSON *root = cJSON_ParseWithLength(body.data(), body.size()))
        {
            const cJSON *e = nullptr;
            cJSON_ArrayForEach(e, cJSON_GetObjectItemCaseSensitive(root, "entries"))
            {
                const cJSON *n = cJSON_GetObjectItemCaseSensitive(e, "name");
                const cJSON *d = cJSON_GetObjectItemCaseSensitive(e, "dir");
                if (!cJSON_IsString(n) || cJSON_IsTrue(d))
                    continue;
                const cJSON *s = cJSON_GetObjectItemCaseSensitive(e, "size");
                const cJSON *m = cJSON_GetObjectItemCaseSensitive(e, "mtime");
                entries[n->valuestring] = {cJSON_IsNumber(s) ? static_cast<long long>(s->valuedouble) : 0,
                                           cJSON_IsNumber(m) ? static_cast<long long>(m->valuedouble) : 0};
            }
            cJSON_Delete(root);
        }
    return listed_.emplace(dir, std::move(entries)).first->second;
}
#endif
} // namespace

void store_init()
{
    pthread_mutex_lock(&lock_);
    if (mode_ != Mode::unset)
    {
        pthread_mutex_unlock(&lock_);
        return;
    }
#ifdef BOXSHELF_HOST
    const char *dir = std::getenv("BOXSHELF_DOWNLOAD0");
    root_ = std::string(dir ? dir : ".local/download0") + "/boxshelf";
    mode_ = Mode::local, mode_name_ = "local";
#else
    const std::string data = "/data/vitrine";
    // 1. straight to /data, when the app may write there
    local_mkdirs(data);
    const std::string probe = data + "/.probe";
    if (local_write(probe, "vitrine") && std::remove(probe.c_str()) == 0)
        root_ = data, mode_ = Mode::local, mode_name_ = "direct";
    else
    {
        // 2. through Elf Arsenal: the folder made with its API, a probe written over FTP and listed back
        std::string body;
        elf_get(fs_url("/api/fs/mkdir?path=", data), body); // (already there is fine)
        bool ok = ftp_write(probe, "vitrine");
        if (ok)
        {
            root_ = data, mode_ = Mode::remote, mode_name_ = "elf";
            listed_.erase(data);
            ok = listing(data).count(".probe") > 0;
            if (const int control = ftp_open(); control >= 0)
            {
                char text[256];
                ftp_reply(control, "DELE " + probe + "\r\n", text, sizeof text);
                ftp_close(control);
            }
            listed_.clear();
        }
        if (!ok)
        {
            // 3. the app's own download data, as before (wiped by the console on a crash or an update)
            root_ = "/download0/boxshelf";
            mode_ = Mode::local, mode_name_ = "download0";
        }
    }
#endif
    pthread_mutex_unlock(&lock_);
    hui::sys::log("[BOXSHELF] store: %s (%s)", root_.c_str(), mode_name_);
}

std::string store_root()
{
    store_init();
    return root_;
}

const char *store_mode()
{
    store_init();
    return mode_name_;
}

bool store_read(const std::string &path, std::string &out, std::size_t max)
{
    store_init();
#ifndef BOXSHELF_HOST
    if (mode_ == Mode::remote)
    {
        out.clear();
        return elf_get(fs_url("/api/fs/download?path=", path), out, max) && !out.empty();
    }
#endif
    return local_read(path, out, max);
}

bool store_write(const std::string &path, const std::string &data)
{
    store_init();
#ifndef BOXSHELF_HOST
    if (mode_ == Mode::remote)
    {
        const bool ok = ftp_write(path, data);
        if (ok)
        {
            std::string dir, name;
            split(path, dir, name);
            pthread_mutex_lock(&lock_);
            if (auto it = listed_.find(dir); it != listed_.end())
                it->second[name] = {static_cast<long long>(data.size()), static_cast<long long>(std::time(nullptr))};
            pthread_mutex_unlock(&lock_);
        }
        return ok;
    }
#endif
    return local_write(path, data);
}

bool store_stat(const std::string &path, long long *size, long long *mtime)
{
    store_init();
#ifndef BOXSHELF_HOST
    if (mode_ == Mode::remote)
    {
        std::string dir, name;
        split(path, dir, name);
        pthread_mutex_lock(&lock_);
        const auto &entries = listing(dir);
        const auto it = entries.find(name);
        const bool found = it != entries.end();
        if (found && size)
            *size = it->second.size;
        if (found && mtime)
            *mtime = it->second.mtime;
        pthread_mutex_unlock(&lock_);
        return found;
    }
#endif
    struct stat st
    {
    };
    if (stat(path.c_str(), &st) != 0)
        return false;
    if (size)
        *size = static_cast<long long>(st.st_size);
    if (mtime)
        *mtime = static_cast<long long>(st.st_mtime);
    return true;
}

void store_mkdirs(const std::string &dir)
{
    store_init();
#ifndef BOXSHELF_HOST
    if (mode_ == Mode::remote)
    {
        std::string body;
        elf_get(fs_url("/api/fs/mkdir?path=", dir), body);
        return;
    }
#endif
    local_mkdirs(dir);
}
} // namespace boxshelf::app
