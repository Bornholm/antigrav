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

// Qualité automatique : choisit le niveau de rendu (0 basse, 1 moyenne,
// 2 haute) d'après la charge mesurée. Elle s'appuie sur la résolution
// adaptative : si celle-ci doit descendre bas et y rester, le niveau baisse ;
// si le GPU reste largement sous le budget à pleine résolution, il monte. Une
// montée qui doit être annulée peu après bloque les montées suivantes.
struct QualityGovernor {
    int level = 2;
    float low = 0.0f;         // secondes passées à basse résolution
    float easy = 0.0f;        // secondes à pleine résolution, GPU peu chargé
    float sinceRaise = 1e9f;  // temps écoulé depuis la dernière montée
    bool raiseBlocked = false;

    // scaleStep : palier de la résolution adaptative. gpuMs : temps GPU
    // mesuré, 0 s'il est inconnu. Renvoie vrai si le niveau change.
    inline bool update(float frameMs, float budgetMs, int scaleStep, float gpuMs);
};

inline bool QualityGovernor::update(float frameMs, float budgetMs, int scaleStep, float gpuMs) {
    if (frameMs <= 0.0f || budgetMs <= 0.0f || frameMs > budgetMs * 6.0f) return false;
    const float dt = frameMs * 0.001f;
    sinceRaise += dt;
    // Résolution à 60 % ou moins : ce niveau coûte trop cher pour l'appareil.
    low = scaleStep >= 3 ? low + dt : 0.0f;
    if (low > 3.0f && level > 0) {
        if (sinceRaise < 30.0f) raiseBlocked = true;
        level--;
        low = easy = 0.0f;
        return true;
    }
    // Pleine résolution et GPU à moins de 40 % du budget : le niveau du
    // dessus a de la marge. Il faut une mesure du GPU pour en juger.
    easy = scaleStep == 0 && gpuMs > 0.0f && gpuMs < budgetMs * 0.4f ? easy + dt : 0.0f;
    if (easy > 20.0f && level < 2 && !raiseBlocked) {
        level++;
        low = easy = 0.0f;
        sinceRaise = 0.0f;
        return true;
    }
    return false;
}

} // namespace gfx
