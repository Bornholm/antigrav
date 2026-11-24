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
    // Vitesse du point de synchronisation ici, en m/s, et instant du morceau
    // auquel il y passe.
    float pace = 340.0f;
    float time = 0.0f;
};

// Après une plaque, aucune barrière dans son couloir pendant cette durée, sur
// cette demi-largeur autour de son axe.
constexpr float kClearAfterPad = 1.5f;  // secondes
constexpr float kClearHalfWidth = 5.0f; // mètres

// Élément posé sur la piste. Tous agissent selon la seule position du
// vaisseau, sans état propre : la prédiction réseau les rejoue telle quelle.
enum class FeatureType { EnergyPad, Barrier };
struct Feature {
    FeatureType type = FeatureType::EnergyPad;
    float dist = 0.0f;      // début de l'élément le long de la piste
    float length = 0.0f;    // longueur, ou épaisseur pour une barrière
    float lateral = 0.0f;   // centre, en décalage par rapport à l'axe
    float halfWidth = 0.0f;
};

// Piste ouverte générée à partir d'un morceau, échantillonnée à pas constant
// le long de son axe. Tout le jeu se repère en coordonnées de piste :
// distance, décalage latéral, hauteur. La distance 0 est la ligne de départ,
// où le morceau commence ; `length` est la ligne d'arrivée, où il finit.
struct Track {
    float length = 0.0f;
    float step = 0.0f;
    float halfWidth = 15.0f;
    float lead = 0.0f;     // piste construite avant la ligne de départ
    float duration = 0.0f; // durée du morceau, en secondes
    std::vector<TrackFrame> frames;
    std::vector<Feature> features;
    std::vector<float> beats; // instants des temps du morceau, croissants
    std::vector<bool> downbeats; // vrai pour le premier temps d'une mesure

    // Vrai si l'instant tombe sur un temps, à la tolérance près.
    bool onBeat(float time, float tolerance) const;

    // id : indice du morceau, de 0 à kTrackCount - 1. Voir sim/content.h.
    static Track make(int id);

    // Distance de la trame d'indice donné.
    float distOf(int frame) const { return step * static_cast<float>(frame) - lead; }
    // Au-delà des extrémités, la piste se prolonge en ligne droite.
    TrackFrame at(float distance) const;
    glm::vec3 point(float distance, float lateral, float height) const;
    // Distance à parcourir depuis `from` pour atteindre `to`. Très grande si
    // `to` est derrière : sur une piste ouverte on n'y repasse pas.
    float ahead(float from, float to) const;
    // Position du point de synchronisation à un instant du morceau.
    float distAt(float time) const;
};

} // namespace sim
