#pragma once

#include "sim/track.h"

namespace sim {

constexpr int kMaxVehicles = 8;
constexpr float kFixedDt = 1.0f / 60.0f;

constexpr float kCruiseSpeed = 340.0f; // m/s
constexpr float kBoostSpeed = 405.0f;
constexpr float kShipHalfWidth = 3.2f;

struct Input {
    float steer = 0.0f; // -1 à gauche, +1 à droite
    bool boost = false;
};

// État d'un vaisseau en coordonnées de piste. Structure plate : un état
// complet se copie d'un bloc, ce dont la prédiction réseau aura besoin.
struct Vehicle {
    float dist = 0.0f;       // distance parcourue depuis le départ, tours compris
    float lateral = 0.0f;    // décalage par rapport à l'axe, positif à droite
    float lateralVel = 0.0f;
    float speed = 0.0f;
    float energy = 1.0f;
    float roll = 0.0f;       // inclinaison visuelle
    float topSpeed = kCruiseSpeed;
    bool boosting = false;
    bool scraping = false;   // frotte contre un muret
    bool finished = false;
    float finishTime = 0.0f; // temps de course au passage de la ligne
};

struct World {
    Vehicle vehicles[kMaxVehicles];
    int count = 0;
    double time = 0.0;
    int laps = 3;
    float countdown = 0.0f; // secondes avant le départ, les vaisseaux restent sur la grille
    float raceTime = 0.0f;  // secondes depuis le départ
};

// Tour en cours d'un vaisseau, de 1 à laps.
int lapOf(const World& world, const Track& track, const Vehicle& vehicle);

void step(World& world, const Track& track, const Input* inputs);

// Pilote automatique : suit l'intérieur des virages et compense la dérive.
Input autopilot(const Vehicle& vehicle, const Track& track, float laneOffset);

// Rang d'un vaisseau, 1 pour le premier.
int rankOf(const World& world, int index);

} // namespace sim
