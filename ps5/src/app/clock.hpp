// Boxshelf PS5 - the clock row's text from local seconds since 1970 (the runtime has no localtime()).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app/i18n.hpp"
#include "app/scene.hpp"

#include <cstdio>

namespace boxshelf::app
{
// app.js: toLocaleDateString(weekday short, day, month short) + toLocaleTimeString(hour, minute), in the interface
// language: "ter., 10 de nov." (pt-BR), "Tue, Nov 10" (en-US), "mar, 10 nov" (es-ES).
inline Clock format_clock(long long local_seconds)
{
    static const char sat[] = {'s', '\xc3', '\xa1', 'b', '.', 0}; // "sáb."
    static const char mie[] = {'m', 'i', '\xc3', '\xa9', 0};      // "mié"
    static const char sab[] = {'s', '\xc3', '\xa1', 'b', 0};      // "sáb"
    static const char *const days_pt[] = {"dom.", "seg.", "ter.", "qua.", "qui.", "sex.", sat};
    static const char *const months_pt[] = {"jan.", "fev.", "mar.", "abr.", "mai.", "jun.",
                                            "jul.", "ago.", "set.", "out.", "nov.", "dez."};
    static const char *const days_en[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *const months_en[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static const char *const days_es[] = {"dom", "lun", "mar", mie, "jue", "vie", sab};
    static const char *const months_es[] = {"ene", "feb", "mar", "abr", "may", "jun",
                                            "jul", "ago", "sept", "oct", "nov", "dic"};
    long long days_since = local_seconds / 86400, secs = local_seconds % 86400;
    if (secs < 0)
        secs += 86400, --days_since;
    // civil_from_days (Howard Hinnant): days since 1970-01-01 -> y/m/d
    const long long z = days_since + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097, yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    const int day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    const int month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9); // 1..12
    const int weekday = static_cast<int>(((days_since % 7) + 11) % 7); // 1970-01-01 was a Thursday
    char date[48], time[16];
    if (language() == Lang::en)
        std::snprintf(date, sizeof date, "%s, %s %d", days_en[weekday], months_en[month - 1], day);
    else if (language() == Lang::es)
        std::snprintf(date, sizeof date, "%s, %d %s", days_es[weekday], day, months_es[month - 1]);
    else
        std::snprintf(date, sizeof date, "%s, %d de %s", days_pt[weekday], day, months_pt[month - 1]);
    std::snprintf(time, sizeof time, "%02d:%02d", static_cast<int>(secs / 3600), static_cast<int>(secs % 3600 / 60));
    return {date, time};
}
} // namespace boxshelf::app
