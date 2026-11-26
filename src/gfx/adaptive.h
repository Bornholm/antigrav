#pragma once

#include <algorithm>

namespace gfx {

// Résolution adaptative : fait varier un facteur d'échelle de la 3D par
// paliers pour tenir la cadence. Elle baisse dès que les images arrivent en
// retard, et ne remonte qu'après un moment de calme, de plus en plus long si
// la remontée précédente n'a pas tenu.
struct AdaptiveScale {
    static constexpr int kSteps = 5;
    static constexpr float kFactors[kSteps] = {1.0f, 0.85f, 0.72f, 0.6f, 0.5f};
    int step = 0;
    float late = 0.0f;       // secondes d'images en retard accumulées
    float calm = 0.0f;       // secondes sans retard
    float cooldown = 0.0f;   // délai avant le prochain changement
    float retry = 4.0f;      // calme exigé avant de remonter
    float sinceRaise = 1e9f; // temps écoulé depuis la dernière remontée

    float factor() const { return kFactors[step]; }
    // frameMs : durée de l'image qui vient de finir. budgetMs : durée visée.
    // gpuMs : temps GPU mesuré, 0 s'il est inconnu. Renvoie vrai si le palier change.
    inline bool update(float frameMs, float budgetMs, float gpuMs);
};

inline bool AdaptiveScale::update(float frameMs, float budgetMs, float gpuMs) {
    // Un à-coup isolé, un chargement par exemple, ne dit rien de la charge.
    if (frameMs <= 0.0f || budgetMs <= 0.0f || frameMs > budgetMs * 6.0f) return false;
    const float dt = frameMs * 0.001f;
    cooldown = std::max(0.0f, cooldown - dt);
    sinceRaise += dt;
    if (frameMs > budgetMs * 1.2f) {
        late += dt;
        calm = 0.0f;
    } else {
        late = std::max(0.0f, late - dt * 0.5f);
        calm += dt;
    }
    if (late > 0.4f && cooldown <= 0.0f && step < kSteps - 1) {
        // La dernière remontée n'a pas tenu : attendre plus longtemps avant la suivante.
        if (sinceRaise < 6.0f) retry = std::min(retry * 2.0f, 120.0f);
        step++;
        late = calm = 0.0f;
        cooldown = 1.5f;
        return true;
    }
    if (step > 0 && calm > retry && cooldown <= 0.0f) {
        // Avec une mesure du GPU, ne remonter que si le palier du dessus tient
        // dans le budget : le coût suit à peu près le nombre de pixels.
        const float ratio = kFactors[step - 1] / kFactors[step];
        if (gpuMs > 0.0f && gpuMs * ratio * ratio > budgetMs * 0.8f) {
            calm = 0.0f;
            return false;
        }
        step--;
        late = calm = 0.0f;
        cooldown = 1.5f;
        sinceRaise = 0.0f;
        return true;
    }
    return false;
}

// Qualité et cadence automatiques, d'après la charge mesurée. Le régulateur
// choisit le niveau de rendu (0 basse, 1 moyenne, 2 haute) et, sur un écran
// plus rapide que 60 Hz, s'il faut plafonner à 60 images par seconde.
//
// Il s'appuie sur la résolution adaptative. Si celle-ci doit descendre bas et
// y rester, le niveau baisse ; déjà en basse, la cadence passe à 60 images par
// seconde. Si le GPU reste largement sous le budget à pleine résolution, il
// remonte dans l'ordre inverse : d'abord la cadence de l'écran, puis le
// niveau. Une montée qui doit être annulée peu après bloque les suivantes.
struct QualityGovernor {
    int level = 2;
    bool cap60 = false;       // cadence plafonnée à 60 images par seconde
    bool ownsLevel = true;    // le niveau est automatique
    bool ownsCadence = true;  // la cadence est automatique
    float low = 0.0f;         // secondes passées à basse résolution
    float easy = 0.0f;        // secondes à pleine résolution, GPU peu chargé
    float sinceRaise = 1e9f;  // temps écoulé depuis la dernière montée
    bool raiseBlocked = false;

    // budgetMs : durée d'image visée, plafond compris. screenMs : durée d'une
    // image de l'écran, sans plafond. scaleStep : palier de la résolution
    // adaptative. gpuMs : temps GPU mesuré, 0 s'il est inconnu. Renvoie vrai
    // si le niveau ou la cadence change.
    inline bool update(float frameMs, float budgetMs, float screenMs, int scaleStep, float gpuMs);
};

inline bool QualityGovernor::update(float frameMs, float budgetMs, float screenMs, int scaleStep, float gpuMs) {
    if (frameMs <= 0.0f || budgetMs <= 0.0f || frameMs > budgetMs * 6.0f) return false;
    const float dt = frameMs * 0.001f;
    sinceRaise += dt;
    // Un écran à 60 Hz ou moins n'a rien à gagner au plafond.
    const bool canCap = ownsCadence && screenMs < 1000.0f / 60.0f - 0.5f;
    // Résolution à 60 % ou moins : le rendu coûte trop cher pour l'appareil.
    low = scaleStep >= 3 ? low + dt : 0.0f;
    if (low > 3.0f) {
        const bool lowerLevel = ownsLevel && level > 0;
        const bool capNow = !lowerLevel && canCap && !cap60;
        if (lowerLevel || capNow) {
            if (sinceRaise < 30.0f) raiseBlocked = true;
            if (lowerLevel) level--;
            else cap60 = true;
            low = easy = 0.0f;
            return true;
        }
    }
    // Pleine résolution et GPU peu chargé : de la marge pour monter. Il faut
    // une mesure du GPU pour en juger. Le coût d'une image ne dépend pas de
    // la cadence : la même mesure dit si l'écran entier tiendrait.
    const bool full = scaleStep == 0 && gpuMs > 0.0f;
    const bool uncap = cap60 && ownsCadence && full && gpuMs < screenMs * 0.4f;
    const bool raise = ownsLevel && level < 2 && full && gpuMs < budgetMs * 0.4f;
    easy = uncap || raise ? easy + dt : 0.0f;
    if (easy > 20.0f && !raiseBlocked) {
        if (uncap) cap60 = false;
        else level++;
        low = easy = 0.0f;
        sinceRaise = 0.0f;
        return true;
    }
    return false;
}

} // namespace gfx
