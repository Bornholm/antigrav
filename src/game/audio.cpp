#include "game/audio.h"

#include "core/log.h"

#include <SDL3/SDL.h>

namespace game {

namespace {

// Appelé par SDL depuis son fil audio quand le périphérique manque de données.
void SDLCALL feed(void* userdata, SDL_AudioStream* stream, int additional, int) {
    auto* audio = static_cast<Audio*>(userdata);
    float buffer[1024]; // 512 échantillons stéréo
    int frames = additional / static_cast<int>(sizeof(float) * 2);
    while (frames > 0) {
        int chunk = frames < 512 ? frames : 512;
        audio->synth.render(buffer, chunk);
        if (audio->musicReady) audio->music.mix(buffer, chunk);
        SDL_PutAudioStreamData(stream, buffer, chunk * static_cast<int>(sizeof(float)) * 2);
        frames -= chunk;
    }
}

} // namespace

bool Audio::init() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        logWarn("Audio indisponible: %s", SDL_GetError());
        return false;
    }
    // Avant d'ouvrir le périphérique : ensuite seul le fil audio y touche.
    musicReady = music.init();
    const SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, Synth::kSampleRate};
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, this);
    if (!stream) {
        logWarn("Aucune sortie audio: %s", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream);
    return true;
}

void Audio::shutdown() {
    if (stream) SDL_DestroyAudioStream(stream);
    stream = nullptr;
    music.shutdown();
    musicReady = false;
}

void Audio::setPaused(bool paused) {
    if (!stream) return;
    if (paused) SDL_PauseAudioStreamDevice(stream);
    else SDL_ResumeAudioStreamDevice(stream);
}

} // namespace game
