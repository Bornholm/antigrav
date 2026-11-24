#pragma once

#include "music/analysis.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

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
    // Morceau du joueur, décodé en mémoire. Vient après les morceaux livrés.
    static constexpr int kCustom = kCount;

    bool init();
    void shutdown();
    // Morceau à jouer en boucle, ou -1 pour le silence. Le changement se fait
    // en fondu.
    void play(int piece) {
        synced_.store(false);
        wanted_.store(piece);
    }
    // Lecture calée sur la course : le morceau se joue une fois, à l'instant
    // donné par le jeu à chaque image, et à la vitesse `speed` : 1 pour la
    // vitesse normale. Un instant négatif, ou `running` à faux, donne le
    // silence.
    void sync(int piece, double time, bool running, double speed = 1.0) {
        syncTime_.store(time);
        syncSpeed_.store(speed);
        syncRunning_.store(running);
        synced_.store(true);
        wanted_.store(piece);
    }
    // Échantillons du morceau du joueur : stéréo entrelacé à 48 kHz.
    void setCustom(std::shared_ptr<const std::vector<int16_t>> pcm);
    // Partition du morceau d'un circuit, analysée à la première demande.
    static const music::Partition& partition(int track);
    void setVolume(float volume) { volume_.store(volume); }

    // Ajoute la musique à `frames` échantillons stéréo entrelacés à 48 kHz.
    void mix(float* out, int frames);

  private:
    stb_vorbis* decoders_[kCount] = {};
    std::mutex customMutex_;
    std::shared_ptr<const std::vector<int16_t>> customPending_; // déposé par le jeu
    std::shared_ptr<const std::vector<int16_t>> custom_;        // lu par le fil audio
    bool customChanged_ = false;
    size_t customCursor_ = 0;
    std::atomic<int> wanted_{-1};
    std::atomic<float> volume_{0.8f};
    std::atomic<bool> synced_{false};
    std::atomic<bool> syncRunning_{false};
    std::atomic<double> syncTime_{0.0};
    std::atomic<double> syncSpeed_{1.0};
    int current_ = -1;
    // Lecture calée, à vitesse variable.
    double readPosition_ = 0.0; // position dans le morceau, en échantillons
    double fraction_ = 0.0;     // avance entre previous_ et next_
    float previous_[2] = {}, next_[2] = {};
    bool have_ = false;
    bool ended_ = false;
    float gain_ = 0.0f;
};

} // namespace game
