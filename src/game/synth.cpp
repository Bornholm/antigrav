#include "game/synth.h"

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr float kTau = 6.28318530718f;

float saw(float phase) { return 2.0f * phase - 1.0f; }

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
        beepFreq_ = go ? 988.0f : 659.0f;
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
        float f0 = (38.0f + 150.0f * std::pow(ratio, 1.2f)) * (1.0f + 0.12f * boostSmooth_);

        // Moteur : dents de scie désaccordées, sous-octave et sifflement de turbine.
        const float freqs[5] = {f0, f0 * 1.008f, f0 * 0.5f, f0 * 2.013f, f0 * 6.0f};
        for (int k = 0; k < 5; k++) {
            phase_[k] += freqs[k] * dt;
            if (phase_[k] >= 1.0f) phase_[k] -= 1.0f;
        }
        float sub = phase_[2] < 0.5f ? 1.0f : -1.0f;
        float whine = std::sin(phase_[4] * kTau);
        float engineL = saw(phase_[0]) * 0.55f + sub * 0.35f + saw(phase_[3]) * 0.18f;
        float engineR = saw(phase_[1]) * 0.55f + sub * 0.35f + saw(phase_[3]) * 0.18f;

        // Filtre passe-bas qui s'ouvre avec la vitesse.
        float cutoff = 260.0f + 2400.0f * ratio + 1200.0f * boostSmooth_;
        float a = 1.0f - std::exp(-kTau * cutoff * dt);
        float engine[2] = {engineL, engineR};
        for (int c = 0; c < 2; c++) {
            lowpass_[c][0] += (engine[c] - lowpass_[c][0]) * a;
            lowpass_[c][1] += (lowpass_[c][0] - lowpass_[c][1]) * a;
            engine[c] = lowpass_[c][1];
        }
        float engineGain = 0.10f + 0.22f * ratio;

        float beep = 0.0f;
        if (beepTime_ < beepLength_) {
            beepPhase_ += beepFreq_ * dt;
            float envelope = std::min(1.0f, beepTime_ * 200.0f) *
                             std::min(1.0f, (beepLength_ - beepTime_) * 40.0f);
            beep = std::sin(beepPhase_ * kTau) * envelope * 0.22f;
            beepTime_ += dt;
        }

        for (int c = 0; c < 2; c++) {
            float n = nextNoise();

            // Vent : bruit filtré en bande, plus fort et plus aigu avec la vitesse.
            float windCut = 1.0f - std::exp(-kTau * (500.0f + 1900.0f * ratio) * dt);
            windLow_[c] += (n - windLow_[c]) * windCut;
            windBand_[c] += (windLow_[c] - windBand_[c]) * windCut * 0.25f;
            float wind = (windLow_[c] - windBand_[c]) * 0.30f * ratio * ratio;

            // Boost : souffle aigu.
            boostHigh_[c] += (n - boostHigh_[c]) * 0.35f;
            float rush = (n - boostHigh_[c]) * 0.10f * boostSmooth_;

            // Frottement contre le muret : bruit résonant.
            float scrapeCut = 1.0f - std::exp(-kTau * 2600.0f * dt);
            scrapeLow_[c] += (n - scrapeLow_[c]) * scrapeCut;
            scrapeBand_[c] += (scrapeLow_[c] - scrapeBand_[c]) * scrapeCut * 0.5f;
            float scrape = (scrapeLow_[c] - scrapeBand_[c]) * 0.45f * scrapeSmooth_;

            float sample = engine[c] * engineGain + whine * 0.025f * ratio + wind + rush +
                           scrape + beep;
            // Écrêtage doux : la somme ne sature jamais brutalement.
            out[i * 2 + c] = std::tanh(sample * 1.4f) * volume;
        }
    }
}

} // namespace game
