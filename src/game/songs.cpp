#include "game/songs.h"

#include "core/log.h"
#include "game/i18n.h"
#include "game/music.h"
#include "game/synth.h"
#include "music/analysis.h"
#include "music/decode.h"
#include "sim/content.h"
#include "sim/generate.h"

#include <SDL3/SDL.h>

#ifdef __ANDROID__
#include <jni.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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

// Ordre des morceaux, du dernier joué au plus ancien : une ligne par nom de
// fichier, dans le dossier du jeu.
std::string orderPath() {
    char* pref = SDL_GetPrefPath("antigrav", "antigrav");
    if (!pref) return "";
    std::string path = std::string(pref) + "recent.txt";
    SDL_free(pref);
    return path;
}

std::vector<std::string> loadOrder() {
    std::vector<std::string> order;
    size_t size = 0;
    const std::string path = orderPath();
    char* data = path.empty() ? nullptr : static_cast<char*>(SDL_LoadFile(path.c_str(), &size));
    if (!data) return order;
    std::string text(data, size);
    SDL_free(data);
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        if (end > at) order.push_back(text.substr(at, end - at));
        at = end + 1;
    }
    return order;
}

void saveOrder(const std::vector<std::string>& order) {
    const std::string path = orderPath();
    if (path.empty()) return;
    std::string text;
    for (const std::string& name : order) text += name + "\n";
    SDL_SaveFile(path.c_str(), text.data(), text.size());
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

// Nom de fichier d'un document Android, demandé au système. Vide ailleurs, ou
// si le système ne le donne pas.
std::string androidDisplayName(const std::string& uri) {
#ifdef __ANDROID__
    if (uri.rfind("content://", 0) != 0) return "";
    JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return "";
    std::string out;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "displayName", "(Ljava/lang/String;)Ljava/lang/String;");
    if (method) {
        jstring arg = env->NewStringUTF(uri.c_str());
        jstring result = static_cast<jstring>(env->CallObjectMethod(activity, method, arg));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        } else if (result) {
            if (const char* chars = env->GetStringUTFChars(result, nullptr)) {
                out = chars;
                env->ReleaseStringUTFChars(result, chars);
            }
        }
        if (result) env->DeleteLocalRef(result);
        env->DeleteLocalRef(arg);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return out;
#else
    (void)uri;
    return "";
#endif
}

// Titre lu dans les étiquettes du fichier, d'après son début. Vide s'il n'en a pas.
std::string taggedTitle(const std::string& path) {
    SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
    if (!io) return "";
    std::vector<unsigned char> head(256 * 1024);
    const size_t got = SDL_ReadIO(io, head.data(), head.size());
    SDL_CloseIO(io);
    return music::titleOf(head.data(), got);
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
    // Les morceaux joués d'abord, dans l'ordre noté ; les autres ensuite, du
    // plus récemment copié au plus ancien.
    const std::vector<std::string> order = loadOrder();
    auto rankOf = [&](const std::string& file) {
        for (size_t i = 0; i < order.size(); i++)
            if (order[i] == file) return static_cast<int>(i);
        return static_cast<int>(order.size());
    };
    std::sort(found.begin(), found.end(), [&](const Found& a, const Found& b) {
        const int ra = rankOf(a.file), rb = rankOf(b.file);
        return ra != rb ? ra < rb : a.modified > b.modified;
    });
    for (size_t i = 0; i < found.size(); i++) {
        // Au-delà de la limite, les plus anciens sont effacés.
        if (static_cast<int>(i) >= kMaxEntries) {
            SDL_RemovePath((dir + found[i].file).c_str());
            continue;
        }
        // Le titre des étiquettes d'abord, le nom du fichier à défaut.
        const std::string tagged = taggedTitle(dir + found[i].file);
        entries.push_back({tagged.empty() ? displayName(found[i].file) : tagged, dir + found[i].file});
    }
}

int SongLibrary::markPlayed(const std::string& path) {
    const size_t cut = path.find_last_of('/');
    const std::string file = cut == std::string::npos ? path : path.substr(cut + 1);
    std::vector<std::string> order = loadOrder();
    order.erase(std::remove(order.begin(), order.end(), file), order.end());
    order.insert(order.begin(), file);
    if (order.size() > static_cast<size_t>(kMaxEntries)) order.resize(static_cast<size_t>(kMaxEntries));
    saveOrder(order);
    scan();
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].path == path) return static_cast<int>(i);
    return -1;
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
        error = std::string(tr("Morceau refusé : ")) + tr(reason.c_str());
        return -1;
    }
    // Sur Android l'adresse ne porte pas le nom du fichier : on le demande au système.
    const std::string named = androidDisplayName(source);
    const std::string file = baseName(named.empty() ? source : named);
    const bool saved = !dir.empty() && SDL_SaveFile((dir + file).c_str(), data, size);
    SDL_free(data);
    if (!saved) {
        error = "Copie du morceau impossible";
        return -1;
    }
    const int index = markPlayed(dir + file);
    if (index >= 0) return index;
    error = "Copie du morceau impossible";
    return -1;
}

bool SongLibrary::load(int index, Music* music) {
    if (index < 0 || index >= static_cast<int>(entries.size())) return false;
    return loadFile(entries[static_cast<size_t>(index)].path, entries[static_cast<size_t>(index)].name, music);
}

namespace {

const uint32_t kPackageMagic = 0x31474b50u; // "PKG1"
constexpr size_t kMaxSharedAudio = 40u * 1024u * 1024u;

// Échantillons à la fréquence du mixage, en stéréo sur 16 bits.
std::shared_ptr<std::vector<int16_t>> toPcm(const music::Audio& audio) {
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
    return pcm;
}

// Coupe un morceau trop long. Renvoie sa durée, en secondes.
float trim(music::Audio& audio) {
    float seconds = static_cast<float>(audio.frames()) / static_cast<float>(audio.sampleRate);
    if (seconds > kMaxSeconds) {
        // Les huit premières minutes suffisent à faire une course.
        audio.samples.resize(static_cast<size_t>(kMaxSeconds * static_cast<float>(audio.sampleRate)) * static_cast<size_t>(audio.channels));
        seconds = kMaxSeconds;
    }
    return seconds;
}

// Installe une partition dans l'emplacement du joueur : nom, sous-titre, décor.
void install(const music::Partition& partition, const char* name, float seconds) {
    sim::setSong(sim::kCustomTrack, partition);
    char tagline[64];
    const int whole = static_cast<int>(seconds);
    if (partition.beats.size() >= 16 && partition.confidence >= 0.1f)
        std::snprintf(tagline, sizeof(tagline), tr("%.0f BPM, %d:%02d"), partition.tempo, whole / 60, whole % 60);
    else
        std::snprintf(tagline, sizeof(tagline), tr("Sans tempo net, %d:%02d"), whole / 60, whole % 60);
    sim::setCustomTrackInfo(name, tagline);

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
}

template <typename T>
void put(std::vector<unsigned char>& out, const T& value) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

} // namespace

uint64_t packageId(const std::vector<unsigned char>& bytes) {
    // FNV-1a sur 64 bits. Jamais nul : zéro veut dire « pas de morceau ».
    uint64_t h = 1469598103934665603ull;
    for (unsigned char b : bytes) h = (h ^ b) * 1099511628211ull;
    return h ? h : 1;
}

std::vector<unsigned char> SongLibrary::makePackage(int track) {
    error.clear();
    std::vector<unsigned char> out;
    const bool custom = track == sim::kCustomTrack;
    const music::Partition& partition = custom ? sim::song(track) : Music::partition(track);
    if (!custom) sim::setSong(track, partition);

    std::vector<unsigned char> audio;
    if (custom) {
        size_t size = 0;
        void* data = loadedPath.empty() ? nullptr : SDL_LoadFile(loadedPath.c_str(), &size);
        if (!data || size == 0) {
            SDL_free(data);
            error = "Morceau introuvable, impossible de l'envoyer";
            return out;
        }
        if (size > kMaxSharedAudio) {
            SDL_free(data);
            error = "Morceau trop gros pour le multijoueur : 40 Mo au plus";
            return out;
        }
        audio.assign(static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + size);
        SDL_free(data);
    }
    const sim::Track generated = sim::Track::make(track);
    const std::vector<unsigned char> packed = music::pack(partition);

    put(out, kPackageMagic);
    put(out, static_cast<int32_t>(track));
    char name[64] = "";
    std::snprintf(name, sizeof(name), "%s", sim::trackInfo(track).name);
    out.insert(out.end(), name, name + sizeof(name));
    put(out, static_cast<uint32_t>(packed.size()));
    out.insert(out.end(), packed.begin(), packed.end());
    put(out, static_cast<uint32_t>(generated.features.size()));
    for (const sim::Feature& f : generated.features) {
        put(out, static_cast<int32_t>(f.type));
        put(out, f.dist);
        put(out, f.length);
        put(out, f.lateral);
        put(out, f.halfWidth);
    }
    put(out, static_cast<uint32_t>(audio.size()));
    out.insert(out.end(), audio.begin(), audio.end());
    return out;
}

bool SongLibrary::applyPackage(const std::vector<unsigned char>& bytes, Music* music, SongPackage& out) {
    error = "Morceau reçu illisible";
    size_t at = 0;
    auto get = [&](void* into, size_t size) {
        if (bytes.size() - at < size) return false;
        std::memcpy(into, bytes.data() + at, size);
        at += size;
        return true;
    };
    uint32_t magic = 0, count = 0;
    int32_t track = 0;
    SongPackage p;
    if (!get(&magic, 4) || magic != kPackageMagic || !get(&track, 4) || !get(p.name, sizeof(p.name))) return false;
    p.name[sizeof(p.name) - 1] = '\0';
    if (track < 0 || track >= sim::kSongSlots) return false;
    p.track = track;
    if (!get(&count, 4) || bytes.size() - at < count || !music::unpack(bytes.data() + at, count, p.partition)) return false;
    at += count;
    if (!get(&count, 4) || count > 20000) return false;
    for (uint32_t i = 0; i < count; i++) {
        int32_t type = 0;
        sim::Feature f;
        if (!get(&type, 4) || !get(&f.dist, 4) || !get(&f.length, 4) || !get(&f.lateral, 4) || !get(&f.halfWidth, 4)) return false;
        if (type < 0 || type > static_cast<int32_t>(sim::FeatureType::Jump)) return false;
        if (!std::isfinite(f.dist) || !std::isfinite(f.length) || !std::isfinite(f.lateral) || !std::isfinite(f.halfWidth)) return false;
        if (f.length < 0.0f || f.length > 500.0f || f.halfWidth <= 0.0f || f.halfWidth > 60.0f || std::abs(f.lateral) > 60.0f) return false;
        f.type = static_cast<sim::FeatureType>(type);
        p.features.push_back(f);
    }
    if (!get(&count, 4) || bytes.size() - at < count) return false;
    p.audio.assign(bytes.begin() + static_cast<long>(at), bytes.begin() + static_cast<long>(at + count));

    if (p.track == sim::kCustomTrack) {
        music::Audio audio;
        std::string reason;
        if (!music::decodeMemory(p.audio.data(), p.audio.size(), audio, reason)) {
            error = std::string(tr("Morceau reçu refusé : ")) + tr(reason.c_str());
            return false;
        }
        const float seconds = trim(audio);
        install(p.partition, p.name, seconds);
        if (music) music->setCustom(toPcm(audio));
        loadedPath.clear();
    } else {
        // Morceau livré : la musique est déjà là, seule la partition de l'hôte compte.
        sim::setSong(p.track, p.partition);
    }
    error.clear();
    out = std::move(p);
    return true;
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
        error = std::string(tr("Morceau refusé : ")) + tr(reason.c_str());
        return false;
    }
    if (static_cast<float>(audio.frames()) / static_cast<float>(audio.sampleRate) < kMinSeconds) {
        error = "Morceau trop court : 30 secondes au moins";
        return false;
    }
    const float seconds = trim(audio);
    const std::vector<float> mono = audio.mono();
    const music::Partition partition = music::analyze(mono.data(), mono.size(), audio.sampleRate);
    install(partition, name.c_str(), seconds);
    if (music) music->setCustom(toPcm(audio));
    loadedPath = path;
    logInfo("Morceau du joueur : %s, %.1f s, %.1f BPM, confiance %.2f", name.c_str(), seconds,
            partition.tempo, partition.confidence);
    return true;
}

} // namespace game
