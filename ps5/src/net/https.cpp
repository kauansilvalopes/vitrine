// Boxshelf PS5 - HTTPS client. SPDX-License-Identifier: GPL-3.0-or-later
#include "net/https.hpp"

#include <curl/curl.h>

#ifndef BOXSHELF_HOST
#include "net/console_curl.h"
#endif

#include <atomic>

namespace boxshelf::net
{
namespace
{
std::atomic<int> initialised{0};

struct Sink
{
    std::string *body;
    std::size_t max;
    bool overflow = false;
};

std::size_t on_body(char *data, std::size_t size, std::size_t count, void *user)
{
    auto *sink = static_cast<Sink *>(user);
    const std::size_t n = size * count;
    if (sink->body->size() + n > sink->max)
    {
        sink->overflow = true;
        return 0; // stops the transfer
    }
    sink->body->append(data, n);
    return n;
}
} // namespace

bool https_init()
{
    int expected = 0;
    if (initialised.compare_exchange_strong(expected, 1))
        initialised = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? 2 : 3;
    return initialised == 2;
}

bool https_get(const std::string &url, std::string &body, std::size_t max_bytes, long *status, std::string *error,
               long timeout_ms)
{
    body.clear();
    if (status)
        *status = 0;
    if (!https_init())
    {
        if (error)
            *error = "curl_global_init failed";
        return false;
    }
    CURL *easy = curl_easy_init();
    if (!easy)
        return false;
#ifndef BOXSHELF_HOST
    console_curl_setup(easy); // no signals, the console's CA list, non-blocking sockets
#endif
    Sink sink{&body, max_bytes};
    curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(easy, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 4L);
    curl_easy_setopt(easy, CURLOPT_USERAGENT, "Boxshelf-PS5/0.1");
    curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, ""); // gzip / zstd when the server offers them
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, 8000L); // covers the name lookup too
    curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &sink);
    const CURLcode result = curl_easy_perform(easy);
    long code = 0;
    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(easy);
    if (status)
        *status = code;
    if (result != CURLE_OK || code < 200 || code >= 300)
    {
        if (error)
            *error = sink.overflow ? "too large" : result != CURLE_OK ? curl_easy_strerror(result) : "HTTP " + std::to_string(code);
        body.clear();
        return false;
    }
    return true;
}
} // namespace boxshelf::net
