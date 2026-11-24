#pragma once

#include <atomic>
#include <cstdint>

namespace game {

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
    void setVolume(float volume) { volume_.store(volume); }

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

    float nextNoise();
};

} // namespace game
