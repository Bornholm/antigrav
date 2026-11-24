#pragma once

#include <glm/glm.hpp>

namespace gfx {

enum class SkyStyle { Ring = 0, Sun = 1, Planet = 2 };
enum class Scenery { City = 0, Canyon = 1, Station = 2 };

// Identité visuelle d'un circuit : couleurs des néons, ciel, brume et décor.
struct Theme {
    glm::vec3 neonA;  // bords de piste, murets
    glm::vec3 neonB;  // filets intérieurs, rails flottants
    glm::vec3 accent; // portique de départ
    glm::vec3 skyZenith, skyMid, skyHaze;
    glm::vec3 fogColor;
    float fogDensity;
    SkyStyle sky;
    Scenery scenery;
};

inline const Theme& themeFor(int track) {
    static const Theme themes[3] = {
        // Néon City : nuit violette, cyan et magenta.
        {{0.05f, 0.75f, 1.0f}, {1.0f, 0.08f, 0.75f}, {1.0f, 0.85f, 0.35f},
         {0.003f, 0.004f, 0.018f}, {0.022f, 0.008f, 0.060f}, {0.150f, 0.030f, 0.230f},
         {0.050f, 0.012f, 0.095f}, 0.0010f, SkyStyle::Ring, Scenery::City},
        // Canyon Solaire : crépuscule orange, ambre et rouge.
        {{1.0f, 0.55f, 0.08f}, {1.0f, 0.10f, 0.32f}, {1.0f, 0.95f, 0.70f},
         {0.016f, 0.004f, 0.030f}, {0.110f, 0.022f, 0.075f}, {0.600f, 0.170f, 0.060f},
         {0.170f, 0.050f, 0.040f}, 0.00085f, SkyStyle::Sun, Scenery::Canyon},
        // Station Orbitale : vide spatial, vert menthe et bleu électrique.
        {{0.18f, 1.0f, 0.62f}, {0.30f, 0.38f, 1.0f}, {1.0f, 1.0f, 1.0f},
         {0.000f, 0.002f, 0.008f}, {0.003f, 0.012f, 0.030f}, {0.015f, 0.090f, 0.110f},
         {0.004f, 0.018f, 0.030f}, 0.00045f, SkyStyle::Planet, Scenery::Station},
    };
    return themes[track < 0 || track > 2 ? 0 : track];
}

} // namespace gfx
