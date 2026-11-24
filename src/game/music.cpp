#include "game/music.h"

#include "core/log.h"
#include "game/synth.h"
#include "music/decode.h"

#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include <stb_vorbis.c>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" {
extern const unsigned char antigrav_music_0[], antigrav_music_0_end[];
extern const unsigned char antigrav_music_1[], antigrav_music_1_end[];
extern const unsigned char antigrav_music_2[], antigrav_music_2_end[];
extern const unsigned char antigrav_music_3[], antigrav_music_3_end[];
}

namespace game {

namespace {
constexpr float kFadeSeconds = 0.45f;
}

namespace {

struct Blob {
    const unsigned char* begin;
    const unsigned char* end;
};
Blob blob(int piece) {
    static const Blob blobs[Music::kCount] = {
        {antigrav_music_0, antigrav_music_0_end},
        {antigrav_music_1, antigrav_music_1_end},
        {antigrav_music_2, antigrav_music_2_end},
        {antigrav_music_3, antigrav_music_3_end},
    };
    return blobs[piece];
}

} // namespace

const music::Partition& Music::partition(int track) {
    static music::Partition partitions[kCount - kFirstTrack];
    static bool done[kCount - kFirstTrack] = {};
    track = std::clamp(track, 0, kCount - kFirstTrack - 1);
    if (!done[track]) {
        done[track] = true;
        const Blob b = blob(kFirstTrack + track);
        music::Audio audio;
        std::string error;
        if (music::decodeMemory(b.begin, static_cast<size_t>(b.end - b.begin), audio, error)) {
            const std::vector<float> mono = audio.mono();
            partitions[track] = music::analyze(mono.data(), mono.size(), audio.sampleRate);
            logInfo("Morceau %d analysé : %.1f s, %.1f BPM, confiance %.2f", track,
                    partitions[track].duration, partitions[track].tempo, partitions[track].confidence);
        } else {
            logWarn("Morceau %d illisible : %s", track, error.c_str());
        }
    }
    return partitions[track];
}

bool Music::init() {
    Blob blobs[kCount];
    for (int i = 0; i < kCount; i++) blobs[i] = blob(i);
    bool ok = true;
    for (int i = 0; i < kCount; i++) {
        int error = 0;
        decoders_[i] = stb_vorbis_open_memory(
            blobs[i].begin, static_cast<int>(blobs[i].end - blobs[i].begin), &error, nullptr);
        if (!decoders_[i]) {
            logWarn("Musique %d illisible, erreur Vorbis %d", i, error);
            ok = false;
            continue;
        }
        // Les fichiers sont produits à la fréquence du mixage : pas de
        // rééchantillonnage à l'exécution.
        stb_vorbis_info info = stb_vorbis_get_info(decoders_[i]);
        if (info.sample_rate != static_cast<unsigned>(Synth::kSampleRate))
            logWarn("Musique %d à %u Hz, attendu %d", i, info.sample_rate, Synth::kSampleRate);
    }
    return ok;
}

void Music::shutdown() {
    for (stb_vorbis*& decoder : decoders_) {
        if (decoder) stb_vorbis_close(decoder);
        decoder = nullptr;
    }
}

void Music::mix(float* out, int frames) {
    const float step = 1.0f / (kFadeSeconds * static_cast<float>(Synth::kSampleRate));
    const float volume = volume_.load();
    float buffer[1024];

    while (frames > 0) {
        int wanted = wanted_.load();
        if (wanted < -1 || wanted >= kCount || (wanted >= 0 && !decoders_[wanted])) wanted = -1;

        // Morceau précédent éteint : on passe au suivant, depuis son début.
        if (current_ != wanted && gain_ <= 0.0f) {
            current_ = wanted;
            readPosition_ = 0.0;
            fraction_ = 0.0;
            have_ = false;
            ended_ = false;
            if (current_ >= 0) stb_vorbis_seek_start(decoders_[current_]);
        }
        int chunk = std::min(frames, 512);
        if (current_ < 0) {
            out += chunk * 2;
            frames -= chunk;
            continue;
        }

        const bool synced = synced_.load() && current_ == wanted;
        if (synced) {
            // Lecture calée : silence tant que la course n'a pas démarré, puis
            // le morceau suit l'instant du jeu.
            const double target = syncTime_.load();
            if (!syncRunning_.load() || target < 0.0) {
                out += chunk * 2;
                frames -= chunk;
                continue;
            }
            const double rate = Synth::kSampleRate;
            // Un écart de plus d'un quart de seconde se rattrape d'un saut,
            // un écart plus petit en jouant un rien plus vite ou plus lentement.
            const double error = target - readPosition_ / rate;
            if (std::abs(error) > 0.25) {
                stb_vorbis_seek(decoders_[current_], static_cast<unsigned>(std::max(0.0, target * rate)));
                readPosition_ = std::max(0.0, target * rate);
                fraction_ = 0.0;
                have_ = false;
                ended_ = false;
            }
            const double play = std::clamp(syncSpeed_.load() + std::clamp(error, -0.25, 0.25) * 0.4, 0.2, 1.9);
            gain_ = 1.0f; // pas de fondu : le premier temps doit claquer

            // Rééchantillonnage linéaire entre deux échantillons décodés.
            const float g = volume;
            for (int i = 0; i < chunk; i++) {
                while (!have_ || fraction_ >= 1.0) {
                    if (have_) fraction_ -= 1.0;
                    previous_[0] = next_[0];
                    previous_[1] = next_[1];
                    float sample[2] = {0.0f, 0.0f};
                    if (!ended_ && stb_vorbis_get_samples_float_interleaved(decoders_[current_], 2, sample, 2) == 0)
                        ended_ = true;
                    next_[0] = sample[0];
                    next_[1] = sample[1];
                    if (!have_) {
                        previous_[0] = next_[0];
                        previous_[1] = next_[1];
                        have_ = true;
                        break;
                    }
                }
                const float t = static_cast<float>(fraction_);
                out[i * 2] += (previous_[0] + (next_[0] - previous_[0]) * t) * g;
                out[i * 2 + 1] += (previous_[1] + (next_[1] - previous_[1]) * t) * g;
                fraction_ += play;
                readPosition_ += play;
            }
            out += chunk * 2;
            frames -= chunk;
            continue;
        }

        // Fin du fichier : retour au début, la boucle continue dans le même bloc.
        int got = 0;
        for (int tries = 0; got < chunk && tries < 3; tries++) {
            int n = stb_vorbis_get_samples_float_interleaved(decoders_[current_], 2,
                                                             buffer + got * 2, (chunk - got) * 2);
            if (n == 0) stb_vorbis_seek_start(decoders_[current_]);
            got += n;
        }

        float direction = current_ == wanted ? step : -step;
        for (int i = 0; i < got; i++) {
            gain_ = std::clamp(gain_ + direction, 0.0f, 1.0f);
            float g = gain_ * gain_ * volume;
            out[i * 2] += buffer[i * 2] * g;
            out[i * 2 + 1] += buffer[i * 2 + 1] * g;
        }
        out += chunk * 2;
        frames -= chunk;
    }
}

} // namespace game
