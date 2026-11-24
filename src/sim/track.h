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

// Élément posé sur la piste. Tous agissent selon la seule position du
// vaisseau, sans état propre : la prédiction réseau les rejoue telle quelle.
enum class FeatureType { BoostPad, SlowField, Barrier };
struct Feature {
    FeatureType type = FeatureType::BoostPad;
    float dist = 0.0f;      // début de l'élément le long de la piste
    float length = 0.0f;    // longueur, ou épaisseur pour une barrière
    float lateral = 0.0f;   // centre, en décalage par rapport à l'axe
    float halfWidth = 0.0f;
};

// Circuit fermé, échantillonné à pas constant le long de son axe. Tout le jeu
// se repère en coordonnées de piste : distance, décalage latéral, hauteur.
struct Track {
    float length = 0.0f;
    float step = 0.0f;
    float halfWidth = 15.0f;
    std::vector<TrackFrame> frames;
    std::vector<Feature> features;

    // id : indice du circuit, de 0 à kTrackCount - 1. Voir sim/content.h.
    static Track make(int id);

    // Distance quelconque, ramenée dans [0, length).
    TrackFrame at(float distance) const;
    glm::vec3 point(float distance, float lateral, float height) const;
    // Distance à parcourir depuis `from` pour atteindre `to`, dans [0, length).
    float ahead(float from, float to) const;
};

} // namespace sim
