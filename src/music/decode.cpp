#include "music/decode.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include <stb_vorbis.c>

#define DR_MP3_NO_STDIO
#include <dr_mp3.h>
#define DR_FLAC_NO_STDIO
#include <dr_flac.h>

namespace music {

namespace {

uint32_t read32(const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint16_t read16(const unsigned char* p) { return uint16_t(p[0] | p[1] << 8); }

bool decodeWav(const unsigned char* data, size_t size, Audio& out, std::string& error) {
    int format = 0, bits = 0;
    size_t pos = 12;
    while (pos + 8 <= size) {
        const uint32_t length = read32(data + pos + 4);
        const unsigned char* body = data + pos + 8;
        if (pos + 8 + length > size && std::memcmp(data + pos, "data", 4) != 0) break;
        if (!std::memcmp(data + pos, "fmt ", 4) && length >= 16) {
            format = read16(body);
            out.channels = read16(body + 2);
            out.sampleRate = int(read32(body + 4));
            bits = read16(body + 14);
            // Format étendu : le vrai code est dans le sous-format.
            if (format == 0xFFFE && length >= 26) format = read16(body + 24);
        } else if (!std::memcmp(data + pos, "data", 4)) {
            const size_t bytes = std::min(size_t(length), size - pos - 8);
            if (out.channels < 1 || out.channels > 8 || out.sampleRate <= 0) break;
            const bool isFloat = format == 3 && bits == 32;
            const bool isInt = format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
            if (!isFloat && !isInt) {
                error = "encodage WAV non pris en charge";
                return false;
            }
            const size_t step = size_t(bits / 8), count = bytes / step;
            out.samples.resize(count - count % size_t(out.channels));
            for (size_t i = 0; i < out.samples.size(); ++i) {
                const unsigned char* s = body + i * step;
                float v = 0.0f;
                if (isFloat) std::memcpy(&v, s, 4);
                else if (bits == 8) v = (float(s[0]) - 128.0f) / 128.0f;
                else if (bits == 16) v = float(int16_t(read16(s))) / 32768.0f;
                else if (bits == 24) v = float(int32_t(uint32_t(s[0]) << 8 | uint32_t(s[1]) << 16 | uint32_t(s[2]) << 24) >> 8) / 8388608.0f;
                else v = float(int32_t(read32(s))) / 2147483648.0f;
                out.samples[i] = v;
            }
            return true;
        }
        pos += 8 + size_t(length) + (length & 1);
    }
    error = "fichier WAV invalide";
    return false;
}

bool decodeVorbis(const unsigned char* data, size_t size, Audio& out, std::string& error) {
    int code = 0;
    stb_vorbis* v = stb_vorbis_open_memory(data, int(size), &code, nullptr);
    if (!v) {
        error = "fichier Ogg Vorbis invalide";
        return false;
    }
    const stb_vorbis_info info = stb_vorbis_get_info(v);
    out.sampleRate = int(info.sample_rate);
    out.channels = info.channels > 2 ? 2 : info.channels;
    out.samples.reserve(size_t(stb_vorbis_stream_length_in_samples(v)) * size_t(out.channels));
    float chunk[4096];
    for (;;) {
        const int n = stb_vorbis_get_samples_float_interleaved(v, out.channels, chunk, 4096);
        if (n <= 0) break;
        out.samples.insert(out.samples.end(), chunk, chunk + size_t(n) * size_t(out.channels));
    }
    stb_vorbis_close(v);
    return true;
}

// Recopie des échantillons décodés, en ne gardant que deux canaux au plus.
void adopt(Audio& out, const float* pcm, size_t frames, int channels, int sampleRate) {
    out.sampleRate = sampleRate;
    out.channels = channels > 2 ? 2 : channels;
    out.samples.resize(frames * size_t(out.channels));
    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < out.channels; ++c) out.samples[i * size_t(out.channels) + size_t(c)] = pcm[i * size_t(channels) + size_t(c)];
}

bool decodeMp3(const unsigned char* data, size_t size, Audio& out, std::string& error) {
    drmp3_config config{};
    drmp3_uint64 frames = 0;
    float* pcm = drmp3_open_memory_and_read_pcm_frames_f32(data, size, &config, &frames, nullptr);
    if (!pcm || frames == 0 || config.channels == 0) {
        if (pcm) drmp3_free(pcm, nullptr);
        error = "fichier MP3 invalide";
        return false;
    }
    adopt(out, pcm, size_t(frames), int(config.channels), int(config.sampleRate));
    drmp3_free(pcm, nullptr);
    return true;
}

bool decodeFlac(const unsigned char* data, size_t size, Audio& out, std::string& error) {
    unsigned int channels = 0, sampleRate = 0;
    drflac_uint64 frames = 0;
    float* pcm = drflac_open_memory_and_read_pcm_frames_f32(data, size, &channels, &sampleRate, &frames, nullptr);
    if (!pcm || frames == 0 || channels == 0) {
        if (pcm) drflac_free(pcm, nullptr);
        error = "fichier FLAC invalide";
        return false;
    }
    adopt(out, pcm, size_t(frames), int(channels), int(sampleRate));
    drflac_free(pcm, nullptr);
    return true;
}

} // namespace

std::vector<float> Audio::mono() const {
    std::vector<float> m(frames());
    for (size_t i = 0; i < m.size(); ++i) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) sum += samples[i * size_t(channels) + size_t(c)];
        m[i] = sum / float(channels);
    }
    return m;
}

bool decodeMemory(const unsigned char* data, size_t size, Audio& out, std::string& error) {
    out = Audio{};
    bool ok = false;
    if (size >= 12 && !std::memcmp(data, "RIFF", 4) && !std::memcmp(data + 8, "WAVE", 4)) ok = decodeWav(data, size, out, error);
    else if (size >= 4 && !std::memcmp(data, "OggS", 4)) ok = decodeVorbis(data, size, out, error);
    else if (size >= 4 && !std::memcmp(data, "fLaC", 4)) ok = decodeFlac(data, size, out, error);
    else if (size >= 3 && (!std::memcmp(data, "ID3", 3) || (data[0] == 0xFF && (data[1] & 0xE0) == 0xE0)))
        ok = decodeMp3(data, size, out, error);
    else if (size >= 12 && !std::memcmp(data + 4, "ftyp", 4)) error = "format AAC ou M4A non pris en charge";
    else error = "format audio non reconnu";
    if (ok && out.samples.empty()) {
        error = "fichier audio vide";
        ok = false;
    }
    return ok;
}

bool decodeFile(const std::string& path, Audio& out, std::string& error) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        error = "fichier introuvable";
        return false;
    }
    std::vector<unsigned char> data;
    unsigned char chunk[65536];
    for (size_t n; (n = std::fread(chunk, 1, sizeof chunk, file)) > 0;) data.insert(data.end(), chunk, chunk + n);
    std::fclose(file);
    return decodeMemory(data.data(), data.size(), out, error);
}

namespace {

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp == 0) return;
    if (cp < 0x80) {
        out += char(cp);
    } else if (cp < 0x800) {
        out += char(0xC0 | (cp >> 6));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += char(0xE0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

// Texte d'une trame ID3 : un octet d'encodage, puis le texte.
std::string id3Text(const unsigned char* p, size_t size) {
    std::string out;
    if (size < 2) return out;
    const int encoding = p[0];
    p++;
    size--;
    if (encoding == 0) {
        for (size_t i = 0; i < size && p[i]; i++) appendUtf8(out, p[i]); // Latin-1
    } else if (encoding == 3) {
        for (size_t i = 0; i < size && p[i]; i++) out += char(p[i]); // déjà en UTF-8
    } else {
        // UTF-16, avec marque d'ordre pour l'encodage 1, gros-boutiste pour le 2.
        bool big = encoding == 2;
        size_t i = 0;
        if (size >= 2 && ((p[0] == 0xFF && p[1] == 0xFE) || (p[0] == 0xFE && p[1] == 0xFF))) {
            big = p[0] == 0xFE;
            i = 2;
        }
        for (; i + 1 < size; i += 2) {
            const uint32_t cp = big ? uint32_t(p[i]) << 8 | p[i + 1] : uint32_t(p[i + 1]) << 8 | p[i];
            if (cp == 0) break;
            if (cp >= 0xD800 && cp <= 0xDFFF) continue; // hors du plan de base : ignoré
            appendUtf8(out, cp);
        }
    }
    return out;
}

void id3Tags(const unsigned char* data, size_t size, std::string& artist, std::string& title) {
    if (size < 10 || std::memcmp(data, "ID3", 3) != 0) return;
    const int version = data[3];
    const size_t total = (size_t(data[6] & 0x7F) << 21 | size_t(data[7] & 0x7F) << 14 | size_t(data[8] & 0x7F) << 7 | size_t(data[9] & 0x7F)) + 10;
    const size_t end = std::min(size, total);
    size_t at = 10;
    const size_t head = version == 2 ? 6 : 10;
    while (at + head <= end) {
        const unsigned char* f = data + at;
        if (f[0] == 0) break; // remplissage
        size_t length;
        if (version == 2) length = size_t(f[3]) << 16 | size_t(f[4]) << 8 | f[5];
        else if (version == 4) length = size_t(f[4] & 0x7F) << 21 | size_t(f[5] & 0x7F) << 14 | size_t(f[6] & 0x7F) << 7 | size_t(f[7] & 0x7F);
        else length = size_t(f[4]) << 24 | size_t(f[5]) << 16 | size_t(f[6]) << 8 | f[7];
        if (length == 0 || at + head + length > end) break;
        const bool isTitle = version == 2 ? !std::memcmp(f, "TT2", 3) : !std::memcmp(f, "TIT2", 4);
        const bool isArtist = version == 2 ? !std::memcmp(f, "TP1", 3) : !std::memcmp(f, "TPE1", 4);
        if (isTitle) title = id3Text(f + head, length);
        if (isArtist) artist = id3Text(f + head, length);
        at += head + length;
    }
}

// Commentaire Vorbis « CLÉ=valeur », sans tenir compte de la casse de la clé.
void vorbisComment(const char* text, size_t length, std::string& artist, std::string& title) {
    auto match = [&](const char* key) {
        const size_t n = std::strlen(key);
        if (length <= n || text[n] != '=') return false;
        for (size_t i = 0; i < n; i++)
            if ((text[i] | 0x20) != (key[i] | 0x20)) return false;
        return true;
    };
    if (match("title")) title.assign(text + 6, length - 6);
    if (match("artist")) artist.assign(text + 7, length - 7);
}

void flacTags(const unsigned char* data, size_t size, std::string& artist, std::string& title) {
    size_t at = 4;
    while (at + 4 <= size) {
        const bool last = data[at] & 0x80;
        const int type = data[at] & 0x7F;
        const size_t length = size_t(data[at + 1]) << 16 | size_t(data[at + 2]) << 8 | data[at + 3];
        at += 4;
        if (at + length > size) return;
        if (type == 4) {
            const unsigned char* p = data + at;
            const unsigned char* stop = p + length;
            if (stop - p < 4) return;
            const uint32_t vendor = read32(p);
            p += 4;
            if (size_t(stop - p) < size_t(vendor) + 4) return;
            p += vendor;
            const uint32_t count = read32(p);
            p += 4;
            for (uint32_t i = 0; i < count && stop - p >= 4; i++) {
                const uint32_t n = read32(p);
                p += 4;
                if (size_t(stop - p) < n) return;
                vorbisComment(reinterpret_cast<const char*>(p), n, artist, title);
                p += n;
            }
            return;
        }
        at += length;
        if (last) return;
    }
}

} // namespace

std::string titleOf(const unsigned char* data, size_t size) {
    std::string artist, title;
    if (size >= 3 && !std::memcmp(data, "ID3", 3)) {
        id3Tags(data, size, artist, title);
    } else if (size >= 4 && !std::memcmp(data, "fLaC", 4)) {
        flacTags(data, size, artist, title);
    } else if (size >= 4 && !std::memcmp(data, "OggS", 4)) {
        int code = 0;
        if (stb_vorbis* v = stb_vorbis_open_memory(data, int(size), &code, nullptr)) {
            const stb_vorbis_comment comments = stb_vorbis_get_comment(v);
            for (int i = 0; i < comments.comment_list_length; i++)
                vorbisComment(comments.comment_list[i], std::strlen(comments.comment_list[i]), artist, title);
            stb_vorbis_close(v);
        }
    }
    // Les étiquettes peuvent contenir n'importe quoi : pas de caractère de contrôle.
    auto clean = [](std::string& text) {
        std::string out;
        for (char c : text)
            if (static_cast<unsigned char>(c) >= 32) out += c;
        while (!out.empty() && out.back() == ' ') out.pop_back();
        text = out;
    };
    clean(artist);
    clean(title);
    if (title.empty()) return "";
    return artist.empty() ? title : artist + " - " + title;
}

} // namespace music
