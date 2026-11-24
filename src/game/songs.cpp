#include "game/songs.h"

#include "core/log.h"
#include "game/music.h"
#include "game/synth.h"
#include "music/analysis.h"
#include "music/decode.h"
#include "sim/content.h"
#include "sim/generate.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace game {

namespace {

constexpr size_t kMaxFileSize = 80u * 1024u * 1024u;
constexpr float kMinSeconds = 30.0f;
constexpr float kMaxSeconds = 480.0f;

int theme = 0;
int serial = 0;

std::string libraryDir() {
    char* pref = SDL_GetPrefPath("antigrav", "antigrav");
    if (!pref) return "";
    std::string dir = std::string(pref) + "songs";
    SDL_free(pref);
    SDL_CreateDirectory(dir.c_str());
    return dir + "/";
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Nom de fichier tiré d'un chemin ou d'une adresse content:// d'Android.
std::string baseName(const std::string& source) {
    std::string decoded;
    for (size_t i = 0; i < source.size(); i++) {
        if (source[i] == '%' && i + 2 < source.size() && hexValue(source[i + 1]) >= 0 && hexValue(source[i + 2]) >= 0) {
            decoded += static_cast<char>(hexValue(source[i + 1]) * 16 + hexValue(source[i + 2]));
            i += 2;
        } else {
            decoded += source[i];
        }
    }
    const size_t cut = decoded.find_last_of("/\\:");
    std::string name = cut == std::string::npos ? decoded : decoded.substr(cut + 1);
    for (char& c : name)
        if (static_cast<unsigned char>(c) < 32 || c == '/' || c == '\\') c = '_';
    if (name.empty()) name = "morceau";
    if (name.size() > 80) name.resize(80);
    return name;
}

std::string displayName(const std::string& file) {
    const size_t dot = file.find_last_of('.');
    std::string name = dot == std::string::npos || dot == 0 ? file : file.substr(0, dot);
    // Les fichiers téléchargés gardent souvent un identifiant entre crochets.
    const size_t bracket = name.rfind(" [");
    if (bracket != std::string::npos && name.back() == ']' && bracket > 0) name.resize(bracket);
    return name;
}

} // namespace

int customTheme() { return theme; }
int customSerial() { return serial; }

void SongLibrary::scan() {
    entries.clear();
    const std::string dir = libraryDir();
    if (dir.empty()) return;
    int count = 0;
    char** names = SDL_GlobDirectory(dir.c_str(), nullptr, 0, &count);
    struct Found {
        std::string file;
        SDL_Time modified;
    };
    std::vector<Found> found;
    for (int i = 0; names && i < count; i++) {
        SDL_PathInfo info;
        if (SDL_GetPathInfo((dir + names[i]).c_str(), &info) && info.type == SDL_PATHTYPE_FILE)
            found.push_back({names[i], info.modify_time});
    }
    SDL_free(names);
    std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.modified > b.modified; });
    for (size_t i = 0; i < found.size(); i++) {
        // Au-delà de la limite, les plus anciens sont effacés.
        if (static_cast<int>(i) >= kMaxEntries) {
            SDL_RemovePath((dir + found[i].file).c_str());
            continue;
        }
        entries.push_back({displayName(found[i].file), dir + found[i].file});
    }
}

int SongLibrary::import(const std::string& source) {
    error.clear();
    const std::string dir = libraryDir();
    size_t size = 0;
    void* data = SDL_LoadFile(source.c_str(), &size);
    if (!data || size == 0) {
        SDL_free(data);
        error = "Fichier illisible";
        return -1;
    }
    if (size > kMaxFileSize) {
        SDL_free(data);
        error = "Fichier trop gros : 80 Mo au plus";
        return -1;
    }
    // On vérifie qu'il se décode avant de le garder.
    music::Audio audio;
    std::string reason;
    if (!music::decodeMemory(static_cast<const unsigned char*>(data), size, audio, reason)) {
        SDL_free(data);
        error = "Morceau refusé : " + reason;
        return -1;
    }
    const std::string file = baseName(source);
    const bool saved = !dir.empty() && SDL_SaveFile((dir + file).c_str(), data, size);
    SDL_free(data);
    if (!saved) {
        error = "Copie du morceau impossible";
        return -1;
    }
    scan();
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].path == dir + file) return static_cast<int>(i);
    error = "Copie du morceau impossible";
    return -1;
}

bool SongLibrary::load(int index, Music* music) {
    if (index < 0 || index >= static_cast<int>(entries.size())) return false;
    return loadFile(entries[static_cast<size_t>(index)].path, entries[static_cast<size_t>(index)].name, music);
}

bool SongLibrary::loadFile(const std::string& path, const std::string& name, Music* music) {
    error.clear();
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    music::Audio audio;
    std::string reason = "fichier illisible";
    const bool ok = data && music::decodeMemory(static_cast<const unsigned char*>(data), size, audio, reason);
    SDL_free(data);
    if (!ok) {
        error = "Morceau refusé : " + reason;
        return false;
    }
    float seconds = static_cast<float>(audio.frames()) / static_cast<float>(audio.sampleRate);
    if (seconds < kMinSeconds) {
        error = "Morceau trop court : 30 secondes au moins";
        return false;
    }
    if (seconds > kMaxSeconds) {
        // Les huit premières minutes suffisent à faire une course.
        audio.samples.resize(static_cast<size_t>(kMaxSeconds * static_cast<float>(audio.sampleRate)) * static_cast<size_t>(audio.channels));
        seconds = kMaxSeconds;
    }

    const std::vector<float> mono = audio.mono();
    const music::Partition partition = music::analyze(mono.data(), mono.size(), audio.sampleRate);
    sim::setSong(sim::kCustomTrack, partition);

    char tagline[64];
    const int whole = static_cast<int>(seconds);
    if (partition.beats.size() >= 16 && partition.confidence >= 0.1f)
        std::snprintf(tagline, sizeof(tagline), "%.0f BPM, %d:%02d", partition.tempo, whole / 60, whole % 60);
    else
        std::snprintf(tagline, sizeof(tagline), "Sans tempo net, %d:%02d", whole / 60, whole % 60);
    sim::setCustomTrackInfo(name.c_str(), tagline);

    // Décor : la station pour les tempos rapides, sinon la ville pour un son
    // brillant et le canyon pour un son plus sourd.
    float bright = 0.0f, weight = 0.0f;
    for (const music::Section& s : partition.sections) {
        bright += s.brightness * (s.end - s.start);
        weight += s.end - s.start;
    }
    bright = weight > 0.0f ? bright / weight : 0.5f;
    theme = partition.tempo >= 150.0f ? 2 : bright >= 0.52f ? 0 : 1;
    serial++;

    if (music) {
        // Échantillons à la fréquence du mixage, en stéréo sur 16 bits.
        const size_t frames = audio.frames();
        const double ratio = static_cast<double>(audio.sampleRate) / Synth::kSampleRate;
        const size_t outFrames = static_cast<size_t>(static_cast<double>(frames) / ratio);
        auto pcm = std::make_shared<std::vector<int16_t>>(outFrames * 2);
        const int channels = audio.channels;
        for (size_t i = 0; i < outFrames; i++) {
            const double at = static_cast<double>(i) * ratio;
            const size_t i0 = std::min(frames - 1, static_cast<size_t>(at));
            const size_t i1 = std::min(frames - 1, i0 + 1);
            const float t = static_cast<float>(at - static_cast<double>(i0));
            for (int c = 0; c < 2; c++) {
                const size_t ch = static_cast<size_t>(std::min(c, channels - 1));
                const float a = audio.samples[i0 * static_cast<size_t>(channels) + ch];
                const float b = audio.samples[i1 * static_cast<size_t>(channels) + ch];
                (*pcm)[i * 2 + static_cast<size_t>(c)] =
                    static_cast<int16_t>(std::lround(std::clamp(a + (b - a) * t, -1.0f, 1.0f) * 32767.0f));
            }
        }
        music->setCustom(std::move(pcm));
    }
    logInfo("Morceau du joueur : %s, %.1f s, %.1f BPM, confiance %.2f", name.c_str(), seconds,
            partition.tempo, partition.confidence);
    return true;
}

} // namespace game
