#pragma once

#include "sim/track.h"

namespace sim {

constexpr int kMaxVehicles = 8;
constexpr float kFixedDt = 1.0f / 60.0f;

constexpr float kCruiseSpeed = 340.0f; // m/s
constexpr float kBoostSpeed = 405.0f;
constexpr float kShipHalfWidth = 3.2f;

// Boost, missile et bouclier puisent dans la même jauge d'énergie.
constexpr int kMaxMissiles = 16;
constexpr float kMissileCost = 0.25f;
constexpr float kShieldCost = 0.20f;
constexpr float kShieldDuration = 2.5f; // secondes
constexpr float kStunDuration = 1.2f;   // secondes sans contrôle après un impact

struct Input {
    float steer = 0.0f; // -1 à gauche, +1 à droite
    bool boost = false;
    bool fire = false;   // tire un missile à l'appui
    bool shield = false; // lève le bouclier à l'appui
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
    float shieldTime = 0.0f; // secondes de bouclier restantes
    float stunTime = 0.0f;   // secondes restantes après un impact de missile
    bool fireHeld = false;   // état précédent des commandes, pour détecter l'appui
    bool shieldHeld = false;
};

// Missile à tête chercheuse, lui aussi en coordonnées de piste.
struct Missile {
    bool active = false;
    int owner = 0;
    float dist = 0.0f;
    float lateral = 0.0f;
    float speed = 0.0f;
    float life = 0.0f; // secondes avant de s'éteindre
};

struct World {
    Vehicle vehicles[kMaxVehicles];
    Missile missiles[kMaxMissiles];
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

// Complète les commandes d'un pilote automatique : quand tirer, quand se protéger.
void autopilotItems(const World& world, int index, Input& input);

// Rang d'un vaisseau, 1 pour le premier.
int rankOf(const World& world, int index);

} // namespace sim
