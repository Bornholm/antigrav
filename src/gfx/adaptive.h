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

} // namespace gfx
