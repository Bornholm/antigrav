#pragma once

#include <atomic>
#include <cstdint>

namespace game {

// Effets sonores des bonus et des chocs, tous synthétisés.
enum class Sfx : uint8_t { Pickup, Shot, Mine, Pulse, Turbo, ShieldUp, ShieldBreak, Comet, Hit, Jump, Roll, Alert, Count };

// Synthétiseur du son de course : moteur, vent, boost, frottement et bips du
// départ. Tout est calculé, aucun fichier audio n'est chargé.
//
// Le jeu écrit les paramètres depuis son fil, le rendu audio les lit depuis le
// sien. Les échanges passent par des atomiques, sans verrou.
class Synth {
  public:
    static constexpr int kSampleRate = 48000;

    void setEngine(float speed, bool boosting, bool scraping);
    // kind 0 : bip du compte à rebours. kind 1 : signal du départ.
    void beep(int kind);
    // Petite musique de fin de course. kind 0 : victoire. kind 1 : défaite.
    void jingle(int kind);
    void setVolume(float volume) { volume_.store(volume); }
    // Joue un effet sonore, à un volume de 0 à 1.
    void play(Sfx sfx, float volume = 1.0f);

    // Écrit `frames` échantillons stéréo entrelacés, en flottants de -1 à 1.
    void render(float* out, int frames);

  private:
    std::atomic<float> speed_{0.0f};
    std::atomic<float> boost_{0.0f};
    std::atomic<float> scrape_{0.0f};
    std::atomic<float> volume_{0.6f};
    std::atomic<uint32_t> beepCount_{0};
    std::atomic<int> beepKind_{0};

    // État du rendu, touché uniquement par render().
    float speedSmooth_ = 0.0f, boostSmooth_ = 0.0f, scrapeSmooth_ = 0.0f;
    float phase_[5] = {};
    float lowpass_[2][2] = {};
    float windBand_[2] = {}, windLow_[2] = {};
    float scrapeBand_[2] = {}, scrapeLow_[2] = {};
    float boostHigh_[2] = {};
    float crushPhase_ = 0.0f, crushHold_[2] = {};
    uint32_t noise_ = 0x1234567u;
    uint32_t beepSeen_ = 0;
    float beepPhase_ = 0.0f, beepTime_ = 1e9f, beepFreq_ = 0.0f, beepLength_ = 0.0f;
    std::atomic<uint32_t> jingleCount_{0};
    std::atomic<int> jingleKind_{0};
    uint32_t jingleSeen_ = 0;
    int jinglePlaying_ = -1;
    float jingleTime_ = 0.0f;
    float jingleEcho_[2][12000] = {}; // écho d'un quart de seconde
    int jingleEchoAt_ = 0;
    float jingleLp_[3] = {}; // filtres de la nappe et du souffle
    float jingleSample(float dt);

    // Effets sonores : le jeu les dépose dans une file, le rendu les joue
    // sur quelques voix.
    static constexpr int kQueue = 16;
    std::atomic<uint32_t> sfxWrite_{0};
    std::atomic<uint16_t> sfxQueue_[kQueue] = {};
    uint32_t sfxRead_ = 0;
    struct Voice {
        int kind = -1;
        float time = 0.0f, volume = 0.0f, phase = 0.0f, low = 0.0f, band = 0.0f;
    };
    Voice voices_[8];
    float voiceSample(Voice& v, float dt);

    float nextNoise();
};

} // namespace game
