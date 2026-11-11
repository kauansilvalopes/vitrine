// Boxshelf PS5 - the owner's choices. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/settings.hpp"
#include "app/store.hpp"

#include "app/art_fetcher.hpp"
#include "cJSON.h"

#include <sys/stat.h>

#include <cstdio>

namespace boxshelf::app
{
const std::string *Settings::moved_to(const std::string &game_id) const
{
    for (const auto &[id, key] : moves)
        if (id == game_id)
            return &key;
    return nullptr;
}

void Settings::set_move(const std::string &game_id, const std::string &system_key)
{
    for (auto it = moves.begin(); it != moves.end(); ++it)
        if (it->first == game_id)
        {
            if (system_key.empty())
                moves.erase(it);
            else
                it->second = system_key;
            return;
        }
    if (!system_key.empty())
        moves.emplace_back(game_id, system_key);
}

bool Settings::is_favorite(const std::string &game_id) const
{
    for (const auto &f : favorites)
        if (f == game_id)
            return true;
    return false;
}

bool Settings::toggle_favorite(const std::string &game_id)
{
    for (auto it = favorites.begin(); it != favorites.end(); ++it)
        if (*it == game_id)
        {
            favorites.erase(it);
            return false;
        }
    favorites.push_back(game_id);
    return true;
}

void Settings::played(const std::string &game_id, long long when)
{
    for (auto &h : history)
        if (h.id == game_id)
        {
            h.last = when;
            ++h.count;
            return;
        }
    history.push_back({game_id, when, 1});
}

void Settings::add_time(const std::string &game_id, long long ms)
{
    for (auto &h : history)
        if (h.id == game_id)
        {
            h.ms += ms;
            return;
        }
}

const Settings::Played *Settings::history_of(const std::string &game_id) const
{
    for (const auto &h : history)
        if (h.id == game_id)
            return &h;
    return nullptr;
}

std::string Settings::to_json() const
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 1);
    if (!language.empty())
        cJSON_AddStringToObject(root, "language", language.c_str());
    cJSON_AddBoolToObject(root, "backs", backs);
    cJSON_AddStringToObject(root, "sort", sort.c_str());
    cJSON_AddStringToObject(root, "filter", filter.c_str());
    cJSON *m = cJSON_AddObjectToObject(root, "moves");
    for (const auto &[id, key] : moves)
        cJSON_AddStringToObject(m, id.c_str(), key.c_str());
    cJSON *hid = cJSON_AddArrayToObject(root, "hidden");
    for (const auto &id : hidden)
        cJSON_AddItemToArray(hid, cJSON_CreateString(id.c_str()));
    cJSON *f = cJSON_AddArrayToObject(root, "favorites");
    for (const auto &id : favorites)
        cJSON_AddItemToArray(f, cJSON_CreateString(id.c_str()));
    cJSON *h = cJSON_AddObjectToObject(root, "history");
    for (const auto &p : history)
    {
        cJSON *e = cJSON_AddObjectToObject(h, p.id.c_str());
        cJSON_AddNumberToObject(e, "last", static_cast<double>(p.last));
        cJSON_AddNumberToObject(e, "count", p.count);
        if (p.ms > 0)
            cJSON_AddNumberToObject(e, "ms", static_cast<double>(p.ms));
    }
    if (!session.id.empty())
    {
        cJSON *ss = cJSON_AddObjectToObject(root, "session");
        cJSON_AddStringToObject(ss, "id", session.id.c_str());
        cJSON_AddStringToObject(ss, "app", session.app.c_str());
        cJSON_AddNumberToObject(ss, "start", static_cast<double>(session.start));
    }
    char *text = cJSON_Print(root);
    std::string out = text ? text : "{}";
    cJSON_free(text);
    cJSON_Delete(root);
    return out;
}

Settings Settings::from_json(const std::string &json)
{
    Settings s;
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root)
        return s;
    const cJSON *lang = cJSON_GetObjectItemCaseSensitive(root, "language");
    if (cJSON_IsString(lang) && lang->valuestring)
        s.language = lang->valuestring;
    if (const cJSON *b = cJSON_GetObjectItemCaseSensitive(root, "backs"); cJSON_IsBool(b))
        s.backs = cJSON_IsTrue(b) != 0;
    if (const cJSON *o = cJSON_GetObjectItemCaseSensitive(root, "sort"); cJSON_IsString(o) && o->valuestring)
        s.sort = o->valuestring;
    if (const cJSON *f = cJSON_GetObjectItemCaseSensitive(root, "filter"); cJSON_IsString(f) && f->valuestring)
        s.filter = f->valuestring;
    const cJSON *e = nullptr;
    cJSON_ArrayForEach(e, cJSON_GetObjectItemCaseSensitive(root, "moves"))
        if (e->string && cJSON_IsString(e) && e->valuestring)
            s.moves.emplace_back(e->string, e->valuestring);
    cJSON_ArrayForEach(e, cJSON_GetObjectItemCaseSensitive(root, "hidden"))
        if (cJSON_IsString(e) && e->valuestring)
            s.hidden.emplace_back(e->valuestring);
    cJSON_ArrayForEach(e, cJSON_GetObjectItemCaseSensitive(root, "favorites"))
        if (cJSON_IsString(e) && e->valuestring)
            s.favorites.emplace_back(e->valuestring);
    cJSON_ArrayForEach(e, cJSON_GetObjectItemCaseSensitive(root, "history"))
    {
        if (!e->string)
            continue;
        const cJSON *last = cJSON_GetObjectItemCaseSensitive(e, "last");
        const cJSON *count = cJSON_GetObjectItemCaseSensitive(e, "count");
        const cJSON *ms = cJSON_GetObjectItemCaseSensitive(e, "ms");
        s.history.push_back({e->string, cJSON_IsNumber(last) ? static_cast<long long>(last->valuedouble) : 0,
                             cJSON_IsNumber(count) ? count->valueint : 0,
                             cJSON_IsNumber(ms) ? static_cast<long long>(ms->valuedouble) : 0});
    }
    if (const cJSON *ss = cJSON_GetObjectItemCaseSensitive(root, "session"); cJSON_IsObject(ss))
    {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(ss, "id"), *app = cJSON_GetObjectItemCaseSensitive(ss, "app"),
                    *start = cJSON_GetObjectItemCaseSensitive(ss, "start");
        if (cJSON_IsString(id) && cJSON_IsString(app) && cJSON_IsNumber(start))
            s.session = {id->valuestring, app->valuestring, static_cast<long long>(start->valuedouble)};
    }
    cJSON_Delete(root);
    return s;
}

std::string settings_path()
{
    return storage_root() + "/settings.json";
}

Settings load_settings()
{
    std::string text;
    store_read(settings_path(), text, 1u << 20);
    return Settings::from_json(text);
}

bool save_settings(const Settings &settings)
{
    store_mkdirs(storage_root());
    return store_write(settings_path(), settings.to_json());
}
} // namespace boxshelf::app
