// Boxshelf PS5 - HTTP GET to Elf Arsenal on 127.0.0.1:6969 (the sandboxed app reaches the file system through it).
// Socket code from Estante's http_get_to(). SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>

namespace boxshelf::app
{
// "/api/fs/list?path=" + the percent-encoded path ('/' kept).
std::string fs_url(std::string_view endpoint, std::string_view path);
// GET path; true with the body on a 2xx answer (HTTP/1.0, no chunked bodies).
bool elf_get(const std::string &path, std::string &body, std::size_t max_bytes = 4u << 20);
} // namespace boxshelf::app
