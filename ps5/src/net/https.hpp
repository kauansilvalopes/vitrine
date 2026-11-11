// Boxshelf PS5 - small HTTPS client on libcurl (console: PacBrew's build through console_curl.c; PC: the system's).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string>

namespace boxshelf::net
{
// curl_global_init, once, from one thread, before any request. Safe to call again.
bool https_init();
// GET url into body (at most max_bytes). status: the HTTP code (0 when no answer). Never call on the render thread.
bool https_get(const std::string &url, std::string &body, std::size_t max_bytes, long *status = nullptr,
               std::string *error = nullptr, long timeout_ms = 30000);
// GET url, handing the body over piece by piece as it arrives; on_data returns false to stop (not an error when the
// caller has all it wanted: `stopped` then says so). Never call on the render thread.
bool https_stream(const std::string &url, bool (*on_data)(void *user, const char *data, std::size_t size), void *user,
                  long *status = nullptr, std::string *error = nullptr, long timeout_ms = 600000, bool *stopped = nullptr);
} // namespace boxshelf::net
