#include "game/settings.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace game {

namespace {

std::string settingsPath() {
    char* pref = SDL_GetPrefPath("soundwave", "soundwave");
    if (!pref) return "";
    std::string path = std::string(pref) + "settings.txt";
    SDL_free(pref);
    return path;
}

} // namespace

void Settings::load() {
    const std::string path = settingsPath();
    size_t size = 0;
    char* data = path.empty() ? nullptr : static_cast<char*>(SDL_LoadFile(path.c_str(), &size));
    if (!data) return;
    // Une ligne par réglage, sous la forme clé=valeur.
    std::string text(data, size);
    SDL_free(data);
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "name") std::snprintf(name, sizeof(name), "%s", value.c_str());
        else if (key == "lang") lang = langFromCode(value.c_str());
        else if (key == "swap") swapButtons = value == "1";
    }
}

void Settings::save() const {
    const std::string path = settingsPath();
    if (path.empty()) return;
    std::string text = std::string("name=") + name + "\n";
    if (lang != Lang::Count) text += std::string("lang=") + langCode(lang) + "\n";
    text += std::string("swap=") + (swapButtons ? "1" : "0") + "\n";
    SDL_SaveFile(path.c_str(), text.data(), text.size());
}

} // namespace game
