#include "game/records.h"

#include <SDL3/SDL.h>

#include <cstdlib>

namespace game {

namespace {

std::string recordsPath() {
    char* pref = SDL_GetPrefPath("soundwave", "soundwave");
    if (!pref) return "";
    std::string path = std::string(pref) + "records.txt";
    SDL_free(pref);
    return path;
}

} // namespace

void Records::load() {
    best.clear();
    const std::string path = recordsPath();
    size_t size = 0;
    char* data = path.empty() ? nullptr : static_cast<char*>(SDL_LoadFile(path.c_str(), &size));
    if (!data) return;
    // Une ligne par morceau : le score, une tabulation, la clé.
    const std::string text(data, size);
    SDL_free(data);
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        const size_t tab = line.find('\t');
        if (tab == std::string::npos || tab + 1 >= line.size()) continue;
        const int score = std::atoi(line.substr(0, tab).c_str());
        if (score > 0) best.emplace_back(line.substr(tab + 1), score);
    }
}

void Records::save() const {
    const std::string path = recordsPath();
    if (path.empty()) return;
    std::string text;
    for (const auto& [key, score] : best) text += std::to_string(score) + "\t" + key + "\n";
    SDL_SaveFile(path.c_str(), text.data(), text.size());
}

int Records::get(const std::string& key) const {
    for (const auto& [k, score] : best)
        if (k == key) return score;
    return 0;
}

bool Records::submit(const std::string& key, int score) {
    if (key.empty() || score <= 0) return false;
    for (auto& [k, old] : best) {
        if (k != key) continue;
        if (score <= old) return false;
        old = score;
        save();
        return true;
    }
    best.emplace_back(key, score);
    save();
    return true;
}

} // namespace game
