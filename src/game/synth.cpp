#include "game/synth.h"

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr float kTau = 6.28318530718f;

} // namespace

void Synth::setEngine(float speed, bool boosting, bool scraping) {
    speed_.store(speed);
    boost_.store(boosting ? 1.0f : 0.0f);
    scrape_.store(scraping ? 1.0f : 0.0f);
}

void Synth::beep(int kind) {
    beepKind_.store(kind);
    beepCount_.fetch_add(1);
}

float Synth::nextNoise() {
    noise_ = noise_ * 1664525u + 1013904223u;
    return static_cast<float>(noise_ >> 8) / 8388608.0f - 1.0f;
}

void Synth::render(float* out, int frames) {
    const float dt = 1.0f / static_cast<float>(kSampleRate);
    const float speedTarget = speed_.load();
    const float boostTarget = boost_.load();
    const float scrapeTarget = scrape_.load();
    const float volume = volume_.load();

    uint32_t beeps = beepCount_.load();
    if (beeps != beepSeen_) {
        beepSeen_ = beeps;
        bool go = beepKind_.load() == 1;
        beepFreq_ = go ? 660.0f : 440.0f;
        beepLength_ = go ? 0.45f : 0.13f;
        beepTime_ = 0.0f;
        beepPhase_ = 0.0f;
    }

    for (int i = 0; i < frames; i++) {
        // Paramètres lissés : un saut de valeur s'entendrait comme un clic.
        speedSmooth_ += (speedTarget - speedSmooth_) * 0.0004f;
        boostSmooth_ += (boostTarget - boostSmooth_) * 0.0003f;
        scrapeSmooth_ += (scrapeTarget - scrapeSmooth_) * 0.002f;

        float ratio = std::clamp(speedSmooth_ / 400.0f, 0.0f, 1.2f);
        // Fondamentale très basse, qui monte peu : c'est un bourdonnement, pas
        // un régime moteur. Une hauteur qui suit la vitesse sonne comme une hélice.
        float f0 = (40.0f + 26.0f * ratio) * (1.0f + 0.07f * boostSmooth_);

        // phase_ : 0 fondamentale, 1 octave désaccordée, 2 quinte, 3 modulation
        // de largeur d'impulsion, 4 pulsation.
        const float freqs[5] = {f0, f0 * 2.006f, f0 * 3.0f, 0.31f, 5.0f + 9.0f * ratio};
        for (int k = 0; k < 5; k++) {
            phase_[k] += freqs[k] * dt;
            if (phase_[k] >= 1.0f) phase_[k] -= 1.0f;
        }
        // Ondes carrées à largeur variable : le timbre creux d'un synthé numérique.
        float duty = 0.5f + 0.28f * std::sin(phase_[3] * kTau);
        float sub = std::sin(phase_[0] * kTau);
        float pulse = phase_[0] < duty ? 1.0f : -1.0f;
        float octave = phase_[1] < 0.5f ? 1.0f : -1.0f;
        float fifth = std::sin(phase_[2] * kTau);
        float engineL = sub * 0.28f + pulse * 0.46f + octave * 0.20f + fifth * 0.16f;
        float engineR = sub * 0.28f + pulse * 0.46f - octave * 0.20f + fifth * 0.16f;

        // Filtre passe-bas sombre, qui s'entrouvre avec la vitesse.
        float cutoff = 260.0f + 620.0f * ratio + 300.0f * boostSmooth_;
        float a = 1.0f - std::exp(-kTau * cutoff * dt);
        float engine[2] = {engineL, engineR};
        float bright[2];
        for (int c = 0; c < 2; c++) {
            lowpass_[c][0] += (engine[c] - lowpass_[c][0]) * a;
            lowpass_[c][1] += (lowpass_[c][0] - lowpass_[c][1]) * a;
            bright[c] = lowpass_[c][0];
            engine[c] = lowpass_[c][1];
        }

        // Grain numérique : une part du signal, prise avant le second filtre,
        // est rééchantillonnée bas et réduite à quelques niveaux, comme un vieux
        // convertisseur. C'est elle qui reste audible sur un haut-parleur de
        // téléphone, qui ne restitue presque rien sous 150 Hz.
        crushPhase_ += (2200.0f + 2600.0f * ratio) * dt;
        if (crushPhase_ >= 1.0f) {
            crushPhase_ -= 1.0f;
            for (int c = 0; c < 2; c++) crushHold_[c] = std::round(bright[c] * 10.0f) / 10.0f;
        }
        float throb = 1.0f + 0.14f * std::sin(phase_[4] * kTau);
        float engineGain = (0.20f + 0.26f * ratio + 0.10f * boostSmooth_) * throb;

        float beep = 0.0f;
        if (beepTime_ < beepLength_) {
            beepPhase_ += beepFreq_ * dt;
            if (beepPhase_ >= 1.0f) beepPhase_ -= 1.0f;
            float envelope = std::min(1.0f, beepTime_ * 200.0f) *
                             std::min(1.0f, (beepLength_ - beepTime_) * 40.0f);
            // Carré adouci, dans le même registre numérique.
            float wave = 0.6f * std::sin(beepPhase_ * kTau) + (beepPhase_ < 0.5f ? 0.2f : -0.2f);
            beep = wave * envelope * 0.20f;
            beepTime_ += dt;
        }

        for (int c = 0; c < 2; c++) {
            float n = nextNoise();

            // Vent : souffle grave et discret.
            float windCut = 1.0f - std::exp(-kTau * (220.0f + 520.0f * ratio) * dt);
            windLow_[c] += (n - windLow_[c]) * windCut;
            windBand_[c] += (windLow_[c] - windBand_[c]) * windCut * 0.25f;
            float wind = (windLow_[c] - windBand_[c]) * 0.16f * ratio * ratio;

            // Boost : souffle un peu plus clair, sans sifflement.
            float rushCut = 1.0f - std::exp(-kTau * 1400.0f * dt);
            boostHigh_[c] += (n - boostHigh_[c]) * rushCut;
            float rush = boostHigh_[c] * 0.07f * boostSmooth_;

            // Frottement contre le muret : bruit résonant.
            float scrapeCut = 1.0f - std::exp(-kTau * 1500.0f * dt);
            scrapeLow_[c] += (n - scrapeLow_[c]) * scrapeCut;
            scrapeBand_[c] += (scrapeLow_[c] - scrapeBand_[c]) * scrapeCut * 0.5f;
            float scrape = (scrapeLow_[c] - scrapeBand_[c]) * 0.40f * scrapeSmooth_;

            float body = engine[c] * 0.62f + crushHold_[c] * 0.38f;
            float sample = body * engineGain + wind + rush + scrape + beep;
            // Écrêtage doux : la somme ne sature jamais brutalement.
            out[i * 2 + c] = std::tanh(sample * 1.4f) * volume;
        }
    }
}

} // namespace game
