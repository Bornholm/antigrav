#pragma once

#include "game/synth.h"

struct SDL_AudioStream;

namespace game {

// Sortie audio : relie le synthétiseur au périphérique par défaut.
struct Audio {
    Synth synth;
    SDL_AudioStream* stream = nullptr;

    // Renvoie false si aucun périphérique n'est disponible. Le jeu continue sans son.
    bool init();
    void shutdown();
    void setPaused(bool paused);
};

} // namespace game
