#pragma once

#include <glm/glm.hpp>

#include <vector>

namespace sim {

// Repère local de la piste en un point de son axe.
struct TrackFrame {
    glm::vec3 pos{0.0f};
    glm::vec3 tangent{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    // Courbure signée en 1/m, positive dans un virage à droite.
    float curvature = 0.0f;
};

// Circuit fermé, échantillonné à pas constant le long de son axe. Tout le jeu
// se repère en coordonnées de piste : distance, décalage latéral, hauteur.
struct Track {
    float length = 0.0f;
    float step = 0.0f;
    float halfWidth = 15.0f;
    std::vector<TrackFrame> frames;

    // id : indice du circuit, de 0 à kTrackCount - 1. Voir sim/content.h.
    static Track make(int id);

    // Distance quelconque, ramenée dans [0, length).
    TrackFrame at(float distance) const;
    glm::vec3 point(float distance, float lateral, float height) const;
};

} // namespace sim
