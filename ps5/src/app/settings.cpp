// Boxshelf PS5 - the owner's choices. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/settings.hpp"

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

std::string Settings::to_json() const
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 1);
    if (!language.empty())
        cJSON_AddStringToObject(root, "language", language.c_str());
    cJSON_AddBoolToObject(root, "backs", backs);
    cJSON *m = cJSON_AddObjectToObject(root, "moves");
    for (const auto &[id, key] : moves)
        cJSON_AddStringToObject(m, id.c_str(), key.c_str());
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
    const cJSON *e = nullptr;
    cJSON_ArrayForEach(e, cJSON_GetObjectItemCaseSensitive(root, "moves"))
        if (e->string && cJSON_IsString(e) && e->valuestring)
            s.moves.emplace_back(e->string, e->valuestring);
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
    if (FILE *f = std::fopen(settings_path().c_str(), "rb"))
    {
        char chunk[4096];
        std::size_t n;
        while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0 && text.size() < (1u << 20))
            text.append(chunk, n);
        std::fclose(f);
    }
    return Settings::from_json(text);
}

bool save_settings(const Settings &settings)
{
    mkdir(storage_root().c_str(), 0777);
    const std::string path = settings_path(), tmp = path + ".tmp", text = settings.to_json();
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f)
        return false;
    bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = std::fclose(f) == 0 && ok;
    if (ok)
        ok = std::rename(tmp.c_str(), path.c_str()) == 0;
    if (!ok)
        std::remove(tmp.c_str());
    return ok;
}
} // namespace boxshelf::app
