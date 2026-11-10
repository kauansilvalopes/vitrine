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
} // namespace boxshelf::net
