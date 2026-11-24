#pragma once

#include "game/music.h"
#include "game/synth.h"

struct SDL_AudioStream;

namespace game {

// Sortie audio : mixe le synthétiseur et la musique vers le périphérique par défaut.
struct Audio {
    Synth synth;
    Music music;
    bool musicReady = false;
    SDL_AudioStream* stream = nullptr;

    // Renvoie false si aucun périphérique n'est disponible. Le jeu continue sans son.
    bool init();
    void shutdown();
    void setPaused(bool paused);
};

} // namespace game
