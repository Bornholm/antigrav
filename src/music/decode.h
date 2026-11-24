#pragma once

#include <string>
#include <vector>

// Décodage complet d'un fichier audio en mémoire. Formats : Ogg Vorbis et WAV.
namespace music {

struct Audio {
    int sampleRate = 0;
    int channels = 0;
    std::vector<float> samples; // entrelacés

    size_t frames() const { return channels ? samples.size() / size_t(channels) : 0; }
    std::vector<float> mono() const;
};

// Renvoie false et remplit `error` si le fichier est illisible.
bool decodeFile(const std::string& path, Audio& out, std::string& error);
bool decodeMemory(const unsigned char* data, size_t size, Audio& out, std::string& error);

} // namespace music
