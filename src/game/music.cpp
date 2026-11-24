#include "game/music.h"

#include "core/log.h"
#include "game/synth.h"

#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include <stb_vorbis.c>

#include <algorithm>

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

bool Music::init() {
    struct Blob {
        const unsigned char* begin;
        const unsigned char* end;
    };
    const Blob blobs[kCount] = {
        {antigrav_music_0, antigrav_music_0_end},
        {antigrav_music_1, antigrav_music_1_end},
        {antigrav_music_2, antigrav_music_2_end},
        {antigrav_music_3, antigrav_music_3_end},
    };
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
            if (current_ >= 0) stb_vorbis_seek_start(decoders_[current_]);
        }
        int chunk = std::min(frames, 512);
        if (current_ < 0) {
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
