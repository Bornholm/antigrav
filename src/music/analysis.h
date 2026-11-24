#pragma once

#include <cstddef>
#include <vector>

// Analyse d'un morceau : tempo, temps, attaques, énergie et sections. Le
// résultat, la partition, sert à générer la piste. Ce module ne dépend ni de
// SDL ni du rendu.
namespace music {

struct Beat {
    float time;     // secondes
    float strength; // 0 à 1
    int bar;        // position dans la mesure, 0 pour le premier temps
};

enum Band { kLow = 0, kMid = 1, kHigh = 2, kBandCount = 3 };

struct Onset {
    float time;
    float strength; // 0 à 1
    int band;
};

struct Section {
    float start;
    float end;
    float energy;     // 0 à 1, moyenne de la courbe d'énergie
    float density;    // attaques franches par temps
    float brightness; // 0 pour un son sourd, 1 pour un son brillant
};

struct Partition {
    static constexpr int kBeatsPerBar = 4;
    static constexpr float kEnergyRate = 10.0f; // points par seconde

    float duration = 0.0f;
    float tempo = 0.0f;      // battements par minute, 0 si aucun tempo trouvé
    float confidence = 0.0f; // 0 à 1, part des attaques qui tombent sur la grille
    std::vector<Beat> beats;
    std::vector<Onset> onsets;
    std::vector<Section> sections;
    std::vector<float> energy; // 0 à 1, kEnergyRate points par seconde
    // Niveau de huit bandes de fréquences, du grave à l'aigu, de 0 à 1 :
    // kSpectrumRate lignes de kSpectrumBands valeurs par seconde. Sert à faire
    // danser le décor.
    static constexpr int kSpectrumBands = 8;
    static constexpr float kSpectrumRate = 25.0f;
    std::vector<float> spectrum;

    float energyAt(float time) const;
    // Remplit `out` avec les kSpectrumBands niveaux à cet instant.
    void spectrumAt(float time, float* out) const;
};

// Données intermédiaires, pour les images de contrôle.
struct AnalysisTrace {
    static constexpr int kBands = 48;
    float frameRate = 0.0f;
    float frameOffset = 0.0f; // instant de la trame 0
    int frames = 0;
    std::vector<float> spectrum;          // frames * kBands, log de l'amplitude
    std::vector<float> flux[kBandCount];  // flux spectral par bande
};

Partition analyze(const float* mono, size_t count, int sampleRate, AnalysisTrace* trace = nullptr);

} // namespace music
