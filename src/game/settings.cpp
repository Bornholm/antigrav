#include "game/settings.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
        // Ancienne clé, écrite même sans choix du joueur : seules basse et
        // moyenne, qui ne pouvaient venir que de lui, restent imposées.
        else if (key == "quality") quality = value == "0" ? 0 : value == "1" ? 1 : -1;
        else if (key == "graphics") quality = value == "auto" ? -1 : std::clamp(std::atoi(value.c_str()), 0, 2);
        else if (key == "auto_quality") autoQuality = std::clamp(std::atoi(value.c_str()), -1, 2);
        else if (key == "difficulty") difficulty = value == "0" ? 0 : value == "2" ? 2 : 1;
        else if (key == "cap60") cap60 = value == "1";
        else if (key == "ship") ship = std::atoi(value.c_str());
        else if (key == "key_left") keys[0] = std::atoi(value.c_str());
        else if (key == "key_right") keys[1] = std::atoi(value.c_str());
        else if (key == "key_use") keys[2] = std::atoi(value.c_str());
    }
}

void Settings::save() const {
    const std::string path = settingsPath();
    if (path.empty()) return;
    std::string text = std::string("name=") + name + "\n";
    if (lang != Lang::Count) text += std::string("lang=") + langCode(lang) + "\n";
    text += std::string("swap=") + (swapButtons ? "1" : "0") + "\n";
    text += "graphics=" + (quality < 0 ? std::string("auto") : std::to_string(quality)) + "\n";
    if (autoQuality >= 0) text += "auto_quality=" + std::to_string(autoQuality) + "\n";
    text += std::string("difficulty=") + static_cast<char>('0' + difficulty) + "\n";
    text += std::string("cap60=") + (cap60 ? "1" : "0") + "\n";
    text += "ship=" + std::to_string(ship) + "\n";
    static const char* const kKeyNames[] = {"key_left", "key_right", "key_use"};
    for (int i = 0; i < 3; i++)
        if (keys[i] > 0) text += std::string(kKeyNames[i]) + "=" + std::to_string(keys[i]) + "\n";
    SDL_SaveFile(path.c_str(), text.data(), text.size());
}

} // namespace game
