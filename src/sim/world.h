#pragma once

#include "sim/content.h"
#include "sim/track.h"

#include <cstdint>

namespace sim {

constexpr int kMaxVehicles = 8;
constexpr float kFixedDt = 1.0f / 60.0f;

constexpr float kCruiseSpeed = 340.0f; // m/s
constexpr float kBoostSpeed = 405.0f;
constexpr float kShipHalfWidth = 3.2f;

// Boost, missile et bouclier puisent dans la même jauge d'énergie.
constexpr int kMaxMissiles = 16;
constexpr float kMissileCost = 0.25f;
constexpr float kShieldCost = 0.35f;
constexpr float kShieldDuration = 2.5f; // secondes
constexpr float kStunDuration = 1.2f;   // secondes sans contrôle après un impact
constexpr float kAbsorbGain = 0.25f;    // énergie rendue par un missile absorbé
constexpr float kHitGain = 0.20f;       // énergie gagnée par le tireur quand son missile touche

// Éléments de piste, voir sim::Feature.
constexpr float kPadSpeed = 450.0f;      // vitesse que donne une plaque de boost
constexpr float kPadAccel = 500.0f;      // m/s² tant qu'on est dessus
constexpr float kSlowDrag = 1.8f;        // part de vitesse perdue par seconde dans un champ
constexpr float kSlowDrain = 0.25f;      // énergie perdue par seconde dans un champ
constexpr float kBarrierLoss = 0.55f;    // part de vitesse perdue contre une barrière
constexpr float kBarrierStun = 0.6f;     // secondes sans contrôle
constexpr float kBarrierEnergy = 0.15f;

// Course calée sur la musique.
constexpr float kWindow = 3.0f;          // secondes d'écart au point de synchronisation qui servent d'échelle
constexpr float kLeadDrag = 0.10f;       // part de vitesse perdue à kWindow d'avance
constexpr float kDraft = 0.16f;          // part de vitesse gagnée à kWindow de retard
constexpr float kBeatTolerance = 0.075f; // secondes autour d'un temps où un appui compte
constexpr float kAccordTime = 0.55f;     // secondes de boost gratuit et renforcé après un appui en rythme
constexpr float kAccordCooldown = 1.8f;  // secondes avant le prochain accord possible
constexpr float kAccordEnergy = 0.05f;   // énergie rendue par un accord
constexpr float kPadEnergy = 0.04f;      // énergie rendue par une plaque

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
    int ship = 0;            // modèle, voir shipStats
    bool boosting = false;
    bool scraping = false;   // frotte contre un muret
    bool finished = false;
    float finishTime = 0.0f; // temps de course au passage de la ligne
    float shieldTime = 0.0f; // secondes de bouclier restantes
    float stunTime = 0.0f;   // secondes restantes après un impact de missile
    bool fireHeld = false;   // état précédent des commandes, pour détecter l'appui
    bool boostHeld = false;
    float accord = 0.0f;         // secondes restantes d'un boost lancé en rythme
    float accordCooldown = 0.0f; // secondes avant de pouvoir en relancer un
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
    uint32_t tick = 0;
    double time = 0.0;
    // Horloge musicale. Commune : tous entendent le morceau au même instant,
    // et l'écart de chacun au point de synchronisation est retenu par un
    // élastique. Par joueur : chacun entend le morceau là où est son vaisseau,
    // et la course est libre.
    bool ownClock = false;
    float countdown = 0.0f; // secondes avant le départ, les vaisseaux restent sur la grille
    float raceTime = 0.0f;  // secondes depuis le départ
};

// Instant du morceau pour un vaisseau, selon l'horloge de la course.
float songTime(const World& world, const Track& track, const Vehicle& vehicle);
// Avance d'un vaisseau sur le point de synchronisation, en secondes. Négative s'il est en retard.
float gapOf(const World& world, const Track& track, const Vehicle& vehicle);

void step(World& world, const Track& track, const Input* inputs);

// Avance un seul vaisseau d'un pas, sans missiles ni contacts. La prédiction
// réseau s'en sert pour rejouer les commandes du joueur local.
void stepVehicle(Vehicle& vehicle, const Input& input, const Track& track, const World& world);

// Pilote automatique : conduite, boost, tir et bouclier selon sa personnalité.
// laneOffset est son couloir préféré, en mètres par rapport à l'axe.
Input autopilot(const World& world, int index, const Track& track, const AiProfile& profile,
                float laneOffset);

// Vitesse de croisière d'un vaisseau là où il se trouve.
float cruiseSpeed(const Vehicle& vehicle, const Track& track);

// Rang d'un vaisseau, 1 pour le premier.
int rankOf(const World& world, int index);

} // namespace sim
