#pragma once

#include <glm/glm.hpp>

namespace gfx {

enum class SkyStyle { Ring = 0, Sun = 1, Planet = 2, Forge = 3, Abyss = 4 };
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
    // Un thème par morceau livré : trois par coupe, dans l'ordre des coupes.
    static const Theme themes[9] = {
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

        // Coupe Acier. Fonderie : tours noires, orange de coulée et acier froid.
        {{1.0f, 0.30f, 0.05f}, {0.70f, 0.80f, 0.95f}, {1.0f, 0.95f, 0.80f},
         {0.010f, 0.003f, 0.002f}, {0.050f, 0.012f, 0.006f}, {0.550f, 0.100f, 0.020f},
         {0.050f, 0.014f, 0.008f}, 0.0012f, SkyStyle::Forge, Scenery::City},
        // Chaîne de montage : structures suspendues, jaune de signalisation et acier.
        {{1.0f, 0.78f, 0.10f}, {0.75f, 0.85f, 1.0f}, {1.0f, 1.0f, 1.0f},
         {0.004f, 0.005f, 0.008f}, {0.012f, 0.013f, 0.018f}, {0.130f, 0.080f, 0.035f},
         {0.016f, 0.015f, 0.016f}, 0.0009f, SkyStyle::Forge, Scenery::Station},
        // Haut fourneau : parois de roche incandescente, rouge et orange.
        {{1.0f, 0.12f, 0.04f}, {1.0f, 0.55f, 0.05f}, {1.0f, 0.90f, 0.60f},
         {0.020f, 0.002f, 0.000f}, {0.060f, 0.008f, 0.003f}, {0.600f, 0.130f, 0.020f},
         {0.085f, 0.018f, 0.006f}, 0.0011f, SkyStyle::Forge, Scenery::Canyon},

        // Coupe Abysse. La Fosse : parois sous-marines, vert de bioluminescence et bleu profond.
        {{0.15f, 1.0f, 0.45f}, {0.05f, 0.45f, 1.0f}, {0.80f, 1.0f, 0.90f},
         {0.000f, 0.006f, 0.018f}, {0.000f, 0.020f, 0.045f}, {0.010f, 0.100f, 0.120f},
         {0.004f, 0.035f, 0.050f}, 0.0014f, SkyStyle::Abyss, Scenery::Canyon},
        // Méduses : structures flottantes, violet et turquoise.
        {{0.70f, 0.30f, 1.0f}, {0.10f, 0.95f, 0.90f}, {1.0f, 0.90f, 1.0f},
         {0.006f, 0.002f, 0.030f}, {0.030f, 0.012f, 0.080f}, {0.100f, 0.060f, 0.250f},
         {0.030f, 0.015f, 0.070f}, 0.0010f, SkyStyle::Abyss, Scenery::Station},
        // Léviathan : ville engloutie, vert toxique et rouge d'alarme.
        {{0.55f, 1.0f, 0.05f}, {1.0f, 0.15f, 0.20f}, {0.90f, 1.0f, 0.60f},
         {0.000f, 0.004f, 0.002f}, {0.004f, 0.020f, 0.010f}, {0.040f, 0.140f, 0.030f},
         {0.010f, 0.035f, 0.012f}, 0.0015f, SkyStyle::Abyss, Scenery::City},
    };
    return themes[track < 0 || track > 8 ? 0 : track];
}

} // namespace gfx
