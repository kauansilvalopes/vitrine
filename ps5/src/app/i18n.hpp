// Boxshelf PS5 - interface languages: English (Boxshelf's own words), Portuguese (Brazil) and Spanish, as in Estante.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>

namespace boxshelf::app
{
enum class Lang
{
    en,
    pt,
    es,
};

Lang language() noexcept;
void set_language(Lang lang) noexcept;
// The PS5's system language (SCE_SYSTEM_SERVICE_PARAM_ID_LANG) -> one of ours: Portuguese and Spanish where the
// console uses them, English otherwise.
Lang language_from_system(int system_language) noexcept;
std::string_view language_code(Lang lang) noexcept; // "en", "pt", "es"
Lang language_from_code(std::string_view code, Lang fallback) noexcept;
// Name of a language in itself ("English", "Português", "Español").
const char *language_name(Lang lang) noexcept;

// The text in the current language; Boxshelf's English when there is no translation.
const char *tr(const char *english) noexcept;
// tr() with one "%s" filled in.
std::string trf(const char *english, std::string_view arg);
} // namespace boxshelf::app
