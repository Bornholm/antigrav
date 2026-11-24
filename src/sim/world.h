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
// Contre une barrière, seul le cœur du vaisseau compte, comme dans un jeu de
// tir : les ailes passent au travers, ce qui laisse une chance d'esquiver.
constexpr float kHitRadius = 0.9f;

// La jauge d'énergie alimente le boost et sert d'armure : assez pleine, elle
// encaisse une barrière à la place du vaisseau.
constexpr int kMaxMissiles = 16;
constexpr float kArmorCost = 0.33f;     // énergie que coûte une barrière encaissée
constexpr float kArmorFlash = 0.5f;     // secondes d'affichage de la bulle d'armure
// Décharge : gagnée en enchaînant des accords, elle part seule sur le vaisseau
// de devant. Sa cible la pare en étant elle-même en plein accord.
constexpr int kComboSize = 8;           // accords d'affilée pour une décharge
constexpr float kStunDuration = 0.6f;   // secondes sans contrôle après une décharge
constexpr float kAbsorbGain = 0.25f;    // énergie rendue par une décharge parée
constexpr float kHitGain = 0.20f;       // énergie gagnée par le tireur quand sa décharge touche
constexpr float kLeaderGain = 0.20f;    // énergie rendue quand il n'y a personne devant à viser

// Éléments de piste, voir sim::Feature.
constexpr float kBarrierLoss = 0.55f;    // part de vitesse perdue contre une barrière
constexpr float kBarrierStun = 0.6f;     // secondes sans contrôle
constexpr float kBarrierEnergy = 0.15f;

// Course calée sur la musique.
// Le point de synchronisation avance à la vitesse d'un vaisseau bien piloté,
// qui prend ses plaques et place ses accords : c'est kSyncEdge fois la
// vitesse de croisière. Rouler sans rien faire, c'est prendre du retard.
constexpr float kSyncEdge = 1.15f;
constexpr float kWindow = 2.0f;          // secondes d'écart au point de synchronisation qui servent d'échelle
constexpr float kLeadDrag = 0.10f;       // part de vitesse perdue à kWindow d'avance
constexpr float kDraft = 0.16f;          // part de vitesse gagnée à kWindow de retard
constexpr float kBeatTolerance = 0.075f; // secondes autour d'un temps où un appui compte
constexpr float kAccordTime = 0.45f;     // secondes de boost gratuit et renforcé après une frappe en rythme
constexpr float kAccordCooldown = 0.14f; // secondes pendant lesquelles un second appui est ignoré : anti-rebond
constexpr float kAccordEnergy = 0.012f;  // énergie rendue par un accord
constexpr float kPadEnergy = 0.12f;      // énergie rendue par une plaque

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
    int ship = 0;            // modèle, voir shipStats
    bool boosting = false;
    bool scraping = false;   // frotte contre un muret
    bool finished = false;
    float finishTime = 0.0f; // temps de course au passage de la ligne
    float shieldTime = 0.0f; // secondes d'affichage restantes de la bulle d'armure
    float stunTime = 0.0f;   // secondes restantes sans contrôle après un choc
    bool boostHeld = false;  // état précédent de la commande, pour détecter l'appui
    int combo = 0;           // accords réussis d'affilée
    float accord = 0.0f;         // secondes restantes d'un boost lancé en rythme
    float accordCooldown = 0.0f; // secondes avant de pouvoir en relancer un
};

// Décharge à tête chercheuse, elle aussi en coordonnées de piste.
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
    // Horloge musicale par joueur : chacun entend le morceau là où est son
    // vaisseau, sans élastique. Variante essayée puis écartée le 24 novembre
    // 2026, la musique qui change de vitesse étant désagréable. Le jeu ne
    // l'active plus ; les tests de physique s'en servent pour couper
    // l'élastique.
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
