#pragma once

#include <algorithm>
#include <cmath>

namespace game {

// Ce que la course relève sur le vaisseau du joueur, pas après pas.
struct ScoreTally {
    float time = 0.0f;   // secondes de course comptées
    float tempo = 0.0f;  // secondes pondérées par la tenue du tempo, de 0 à 1
    float scrape = 0.0f; // secondes passées à frotter un muret
    int hits = 0;        // chocs encaissés : tirs, mines, ondes, comètes, barrières
    int pickups = 0;     // bonus ramassés

    // gap : avance du vaisseau sur la musique, en secondes.
    void step(float dt, float gap, bool scraping) {
        time += dt;
        tempo += dt * tempoQuality(gap);
        if (scraping) scrape += dt;
    }
    // Plein à une demi-seconde d'avance, nul à trois secondes de retard.
    static float tempoQuality(float gap) { return std::clamp((gap + 3.0f) / 3.5f, 0.0f, 1.0f); }
};

// Score d'une course, sur 10 000, et sa note de S à D. Il ne dépend pas de la
// durée du morceau : deux courses se comparent d'un morceau à l'autre.
struct Score {
    int place = 0;  // classement à l'arrivée, sur 4 000
    int tempo = 0;  // tenue du tempo, sur 3 000
    int clean = 0;  // pilotage sans choc ni frottement, sur 2 000
    int items = 0;  // bonus ramassés, sur 1 000
    int total = 0;
    char grade = 'D';
};

inline char gradeOf(int total) {
    return total >= 9000 ? 'S' : total >= 7500 ? 'A' : total >= 6000 ? 'B' : total >= 4000 ? 'C' : 'D';
}

// difficulty : niveau des IA, de 0 à 2. Battre des IA faciles rapporte moins,
// ce qui réserve la note S au niveau normal et au-dessus.
inline Score computeScore(const ScoreTally& t, int rank, int racers, int difficulty) {
    Score s;
    // Seul en piste, la place ne prouve rien : elle rapporte la moitié.
    static const float kWeight[3] = {0.7f, 0.85f, 1.0f};
    const float place = racers > 1 ? kWeight[std::clamp(difficulty, 0, 2)] * static_cast<float>(racers - rank) /
                                         static_cast<float>(racers - 1)
                                   : 0.5f;
    const float minutes = std::max(t.time, 1.0f) / 60.0f;
    const float tempo = t.time > 0.0f ? t.tempo / t.time : 0.0f;
    // Les chocs font partie de la course : un par minute, ou six secondes de
    // frottement, coûte un huitième de la part de pilotage.
    const float faults = (static_cast<float>(t.hits) + t.scrape / 6.0f) / minutes;
    const float clean = std::clamp(1.0f - faults / 8.0f, 0.0f, 1.0f);
    // Huit bonus par minute font le plein : c'est le rythme de qui n'en rate guère.
    const float items = std::clamp(static_cast<float>(t.pickups) / (8.0f * minutes), 0.0f, 1.0f);
    s.place = static_cast<int>(std::lround(place * 4000.0f));
    s.tempo = static_cast<int>(std::lround(tempo * 3000.0f));
    s.clean = static_cast<int>(std::lround(clean * 2000.0f));
    s.items = static_cast<int>(std::lround(items * 1000.0f));
    s.total = s.place + s.tempo + s.clean + s.items;
    s.grade = gradeOf(s.total);
    return s;
}

} // namespace game
