#include "game/ghost.h"

#include <SDL3/SDL.h>

#include <cinttypes>
#include <cstdio>

namespace game {

namespace {

// Un fichier par morceau, nommé d'après une empreinte de sa clé : la clé
// d'un morceau du joueur est son nom de fichier, qui peut contenir n'importe quoi.
std::string ghostPath(const std::string& key, bool create) {
    if (key.empty()) return "";
    char* pref = SDL_GetPrefPath("soundwave", "soundwave");
    if (!pref) return "";
    std::string dir = std::string(pref) + "ghosts/";
    SDL_free(pref);
    if (create) SDL_CreateDirectory(dir.c_str());
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : key) h = (h ^ c) * 1099511628211ull;
    char name[32];
    std::snprintf(name, sizeof(name), "%016" PRIx64 ".bin", h);
    return dir + name;
}

} // namespace

bool Ghost::load(const std::string& key) {
    clear();
    const std::string path = ghostPath(key, false);
    size_t size = 0;
    void* data = path.empty() ? nullptr : SDL_LoadFile(path.c_str(), &size);
    if (!data) return false;
    const bool ok = decode(static_cast<const uint8_t*>(data), size);
    SDL_free(data);
    return ok;
}

bool Ghost::save(const std::string& key) const {
    const std::string path = ghostPath(key, true);
    if (path.empty() || !valid()) return false;
    const std::vector<uint8_t> bytes = encode();
    return SDL_SaveFile(path.c_str(), bytes.data(), bytes.size());
}

} // namespace game
