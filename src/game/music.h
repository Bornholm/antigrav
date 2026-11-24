#pragma once

#include <atomic>

struct stb_vorbis;

namespace game {

// Musique du jeu : un thème pour les menus et un morceau par circuit, en Ogg
// Vorbis inclus dans le binaire et décodés au fil de la lecture.
//
// Le jeu choisit le morceau depuis son fil, le fil audio décode et mixe. Seul
// le fil audio touche aux décodeurs.
class Music {
  public:
    static constexpr int kTheme = 0;
    static constexpr int kFirstTrack = 1; // puis un morceau par circuit
    static constexpr int kCount = 4;

    bool init();
    void shutdown();
    // Morceau à jouer en boucle, ou -1 pour le silence. Le changement se fait
    // en fondu.
    void play(int piece) { wanted_.store(piece); }
    void setVolume(float volume) { volume_.store(volume); }

    // Ajoute la musique à `frames` échantillons stéréo entrelacés à 48 kHz.
    void mix(float* out, int frames);

  private:
    stb_vorbis* decoders_[kCount] = {};
    std::atomic<int> wanted_{-1};
    std::atomic<float> volume_{0.5f};
    int current_ = -1;
    float gain_ = 0.0f;
};

} // namespace game
