// Boxshelf PS5 - Elf Arsenal client. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/elf_client.hpp"

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
std::string fs_url(std::string_view endpoint, std::string_view path)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string out(endpoint);
    for (unsigned char c : path)
    {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                          c == '_' || c == '.' || c == '~' || c == '/';
        if (safe)
            out += static_cast<char>(c);
        else
        {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

#ifdef BOXSHELF_HOST_PREVIEW
bool elf_get(const std::string &, std::string &body, std::size_t)
{
    body.clear();
    return false; // the PC preview has no console to ask
}
#else
namespace
{
struct NetAddress
{
    std::uint8_t length, family;
    std::uint16_t port;
    std::uint32_t address;
    std::uint16_t virtual_port;
    std::uint8_t zero[6];
};
} // namespace

bool elf_get(const std::string &path, std::string &body, std::size_t max_bytes)
{
    body.clear();
    const int s = sceNetSocket("boxshelf", 2, 1, 6); // AF_INET, SOCK_STREAM, TCP
    if (s < 0)
        return false;
    const int timeout = 3000000; // us: send, receive, connect
    for (int opt : {0x1105, 0x1106, 0x1109})
        sceNetSetsockopt(s, 0xffff, opt, &timeout, sizeof timeout);
    constexpr std::uint16_t port = 6969;
    NetAddress address{sizeof(NetAddress), 2, static_cast<std::uint16_t>((port << 8) | (port >> 8)), 0x0100007f, 0, {}};
    if (sceNetConnect(s, &address, sizeof address) < 0)
    {
        sceNetSocketClose(s);
        return false;
    }
    char request[2048];
    const int len = std::snprintf(request, sizeof request,
                                  "GET %s HTTP/1.0\r\nHost: 127.0.0.1:6969\r\nConnection: close\r\n\r\n", path.c_str());
    if (len <= 0 || static_cast<std::size_t>(len) >= sizeof request)
    {
        sceNetSocketClose(s);
        return false;
    }
    for (int off = 0; off < len;)
    {
        const int n = sceNetSend(s, request + off, static_cast<std::size_t>(len - off), 0);
        if (n <= 0)
        {
            sceNetSocketClose(s);
            return false;
        }
        off += n;
    }
    std::string response;
    char chunk[16384];
    bool eof = false;
    while (response.size() < max_bytes)
    {
        const int n = sceNetRecv(s, chunk, sizeof chunk, 0);
        if (n == 0)
        {
            eof = true;
            break;
        }
        if (n < 0)
            break;
        response.append(chunk, static_cast<std::size_t>(n));
    }
    sceNetSocketClose(s);
    int code = 0;
    if (!eof || std::sscanf(response.c_str(), "HTTP/%*s %d", &code) != 1 || code < 200 || code >= 300)
        return false;
    const auto head_end = response.find("\r\n\r\n");
    if (head_end == std::string::npos)
        return false;
    const std::string head = response.substr(0, head_end);
    if (head.find("Transfer-Encoding: chunked") != std::string::npos ||
        head.find("transfer-encoding: chunked") != std::string::npos)
        return false;
    body = response.substr(head_end + 4);
    return true;
}
#endif
} // namespace boxshelf::app
