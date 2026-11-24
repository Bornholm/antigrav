// Tests de l'analyse audio sur des signaux synthétiques, dont on connaît le
// tempo, les temps et les sections.
#include "music/analysis.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static int failures = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            ++failures;                                   \
            std::printf("ÉCHEC %s:%d : ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                     \
            std::printf("\n");                            \
        }                                                 \
    } while (0)

namespace {

constexpr int kRate = 44100;

struct Noise {
    uint32_t state = 12345;
    float next() {
        state = state * 1664525u + 1013904223u;
        return float(int32_t(state)) / 2147483648.0f;
    }
};

// Batterie simple : grosse caisse sur chaque temps, plus forte sur le premier
// de la mesure, caisse claire sur les temps 2 et 4, charleston sur les
// demi-temps. `level` donne le volume de chaque mesure.
std::vector<float> drums(float bpm, float seconds, float offset, const std::vector<float>& level = {}) {
    std::vector<float> out(size_t(seconds * kRate), 0.0f);
    Noise noise;
    const double beat = 60.0 / bpm;
    auto add = [&](double time, int kind, float gain) {
        const size_t at = size_t(time * kRate);
        const int length = kind == 0 ? kRate / 8 : kRate / 16;
        for (int i = 0; i < length && at + size_t(i) < out.size(); ++i) {
            const float t = float(i) / kRate;
            float v;
            if (kind == 0) v = std::sin(6.2831853f * (55.0f + 60.0f * std::exp(-t * 40.0f)) * t) * std::exp(-t * 22.0f);
            else if (kind == 1) v = (0.6f * noise.next() + 0.4f * std::sin(6.2831853f * 330.0f * t)) * std::exp(-t * 35.0f);
            else v = noise.next() * std::exp(-t * 110.0f) * 0.4f;
            out[at + size_t(i)] += gain * v;
        }
    };
    for (int i = 0; offset + i * beat < seconds; ++i) {
        const size_t bar = size_t(i / 4);
        const float gain = level.empty() ? 1.0f : level[std::min(bar, level.size() - 1)];
        const double t = offset + i * beat;
        add(t, 0, gain * (i % 4 == 0 ? 0.9f : 0.6f));
        if (i % 2 == 1) add(t, 1, gain * 0.5f);
        add(t + beat * 0.5, 2, gain * 0.3f);
    }
    return out;
}

void testTempo(float bpm) {
    const float offset = 0.37f;
    const std::vector<float> signal = drums(bpm, 40.0f, offset);
    const music::Partition p = music::analyze(signal.data(), signal.size(), kRate);
    CHECK(std::fabs(p.tempo - bpm) < bpm * 0.01f, "tempo %.2f attendu %.2f", p.tempo, bpm);
    CHECK(p.confidence > 0.5f, "confiance %.2f à %.0f BPM", p.confidence, bpm);

    // Chaque temps détecté doit tomber à moins de 30 ms d'un vrai temps.
    const double beat = 60.0 / bpm;
    int good = 0, downbeats = 0, goodDownbeats = 0;
    for (const music::Beat& b : p.beats) {
        const double index = std::round((b.time - offset) / beat);
        const double error = std::fabs(b.time - (offset + index * beat));
        good += error < 0.03;
        if (b.bar == 0) {
            ++downbeats;
            goodDownbeats += (long(index) % 4 + 4) % 4 == 0;
        }
    }
    const int expected = int((40.0f - offset) / beat);
    CHECK(int(p.beats.size()) >= expected - 2 && int(p.beats.size()) <= expected + 2,
          "%zu temps, %d attendus à %.0f BPM", p.beats.size(), expected, bpm);
    CHECK(good >= int(p.beats.size() * 0.95f), "%d temps justes sur %zu à %.0f BPM", good, p.beats.size(), bpm);
    CHECK(downbeats > 0 && goodDownbeats == downbeats, "premier temps de mesure faux à %.0f BPM : %d sur %d", bpm, goodDownbeats, downbeats);
}

void testOnsets() {
    const std::vector<float> signal = drums(120.0f, 20.0f, 0.2f);
    const music::Partition p = music::analyze(signal.data(), signal.size(), kRate);
    int low = 0, high = 0;
    for (const music::Onset& o : p.onsets) {
        // Le bruit blanc du charleston laisse une trace faible dans les graves.
        low += o.band == music::kLow && o.strength > 0.5f;
        high += o.band == music::kHigh;
    }
    // 40 grosses caisses, 40 charlestons plus 20 caisses claires dans les aigus.
    CHECK(low >= 36 && low <= 44, "%d attaques graves franches, 40 attendues", low);
    CHECK(high >= 50, "%d attaques aiguës, 60 attendues", high);
}

void testSections() {
    // 16 mesures douces, 16 fortes avec charleston doublé, 16 douces.
    const float bpm = 120.0f;
    std::vector<float> level(48, 0.08f);
    for (int i = 16; i < 32; ++i) level[size_t(i)] = 1.0f;
    std::vector<float> signal = drums(bpm, 96.0f, 0.0f, level);
    // La partie forte reçoit une nappe aiguë, pour changer aussi de timbre.
    for (size_t i = size_t(32.0f * kRate); i < size_t(64.0f * kRate) && i < signal.size(); ++i)
        signal[i] += 0.15f * std::sin(6.2831853f * 3000.0f * float(i) / kRate);
    const music::Partition p = music::analyze(signal.data(), signal.size(), kRate);
    bool at32 = false, at64 = false;
    for (const music::Section& s : p.sections) {
        at32 |= std::fabs(s.start - 32.0f) < 2.1f;
        at64 |= std::fabs(s.start - 64.0f) < 2.1f;
    }
    CHECK(at32 && at64, "frontières à 32 et 64 s non trouvées, %zu sections", p.sections.size());
    CHECK(p.sections.size() <= 5, "%zu sections pour trois parties", p.sections.size());
    CHECK(p.energyAt(48.0f) > 0.8f, "énergie %.2f dans la partie forte", p.energyAt(48.0f));
    CHECK(p.energyAt(16.0f) < 0.3f, "énergie %.2f dans la partie douce", p.energyAt(16.0f));
    CHECK(std::fabs(p.tempo - bpm) < 1.2f, "tempo %.2f malgré le changement de volume", p.tempo);

    // Spectre réduit : huit bandes entre 0 et 1, plus hautes en moyenne dans
    // la partie forte, et la nappe à 3 kHz ne ressort que là.
    float loud[music::Partition::kSpectrumBands], sumLoud = 0.0f, sumQuiet = 0.0f, toneLoud = 0.0f, toneQuiet = 0.0f;
    for (float t = 34.0f; t < 62.0f; t += 0.5f) {
        p.spectrumAt(t, loud);
        for (float v : loud) sumLoud += v;
        toneLoud += loud[5];
        p.spectrumAt(t - 30.0f, loud);
        for (float v : loud) {
            sumQuiet += v;
            CHECK(v >= 0.0f && v <= 1.0f, "niveau de bande %.2f hors de 0 à 1", v);
        }
        toneQuiet += loud[5];
    }
    CHECK(sumLoud > sumQuiet * 1.5f, "spectre %.1f dans la partie forte, %.1f dans la douce", sumLoud, sumQuiet);
    CHECK(toneLoud > toneQuiet * 2.0f, "bande de la nappe : %.1f contre %.1f", toneLoud, toneQuiet);
    p.spectrumAt(-1.0f, loud);
    CHECK(loud[0] == 0.0f, "spectre non nul avant le début");
    p.spectrumAt(1e6f, loud);
    CHECK(loud[0] == 0.0f, "spectre non nul après la fin");
}

void testDegenerate() {
    const std::vector<float> silence(size_t(kRate) * 10, 0.0f);
    music::Partition p = music::analyze(silence.data(), silence.size(), kRate);
    CHECK(p.beats.empty() && p.tempo == 0.0f, "tempo %.1f trouvé dans le silence", p.tempo);
    CHECK(p.sections.size() == 1, "%zu sections dans le silence", p.sections.size());
    CHECK(std::fabs(p.duration - 10.0f) < 0.01f, "durée %.2f", p.duration);

    p = music::analyze(silence.data(), 100, kRate);
    CHECK(p.beats.empty() && p.sections.size() == 1, "signal trop court mal géré");
    p = music::analyze(nullptr, 0, kRate);
    CHECK(p.duration == 0.0f, "signal vide mal géré");

    // Un son tenu n'a pas de tempo : la confiance doit le dire.
    std::vector<float> tone(size_t(kRate) * 20);
    for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.3f * std::sin(6.2831853f * 220.0f * float(i) / kRate);
    p = music::analyze(tone.data(), tone.size(), kRate);
    CHECK(p.confidence < 0.3f, "confiance %.2f sur un son tenu", p.confidence);
}

} // namespace

int main() {
    for (float bpm : {90.0f, 118.0f, 128.0f, 140.0f, 174.0f}) testTempo(bpm);
    testOnsets();
    testSections();
    testDegenerate();
    if (failures) {
        std::printf("%d échec(s)\n", failures);
        return 1;
    }
    std::printf("analyse audio : tous les tests passent\n");
    return 0;
}
