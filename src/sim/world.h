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

constexpr int kMaxMissiles = 24;

// Bonus ramassés sur la piste. Un vaisseau en porte un à la fois et le
// déclenche d'un appui.
enum class Item : uint8_t {
    None = 0,
    Shot,   // tir à tête chercheuse sur le vaisseau de devant
    Mine,   // posée derrière soi, elle attend le suivant
    Pulse,  // onde de choc sur les vaisseaux tout proches
    Turbo,  // pointe de vitesse
    Shield, // encaisse le prochain choc, barrière comprise
    Comet,  // réservée au dernier : elle remonte la piste et frappe tous ceux qu'elle dépasse
    Count
};
constexpr float kStunDuration = 0.9f;  // secondes sans contrôle après un choc
constexpr float kHitLoss = 0.40f;      // part de vitesse perdue sur un choc
constexpr float kTurboTime = 2.6f;     // secondes
constexpr float kTurboSpeed = 1.22f;   // vitesse en turbo, rapportée à la croisière
constexpr float kShieldTime = 9.0f;    // secondes, ou jusqu'au premier choc
constexpr float kMineLife = 30.0f;     // secondes
constexpr float kPulseRange = 75.0f;   // portée de l'onde le long de la piste, en mètres
constexpr float kPulseWidth = 16.0f;   // et en travers
// Saut : un tremplin envoie le vaisseau en l'air. Il survole alors barrières,
// mines et boîtes, et braque moins bien.
constexpr float kAirTime = 1.25f;      // secondes en l'air
constexpr float kJumpHeight = 16.0f;   // hauteur au sommet du saut, en mètres
constexpr float kCometSpeed = 300.0f;  // vitesse de la comète en plus de celle du lanceur, en m/s
constexpr float kCometLife = 14.0f;    // secondes
constexpr float kCometChance = 0.22f;  // part des boîtes du dernier qui donnent une comète

// Éléments de piste, voir sim::Feature.
constexpr float kBarrierLoss = 0.55f;    // part de vitesse perdue contre une barrière
constexpr float kBarrierStun = 0.6f;     // secondes sans contrôle

// Course calée sur la musique.
// Le point de synchronisation avance à kSyncEdge fois la vitesse de
// croisière : celle d'un vaisseau qui pilote proprement.
constexpr float kSyncEdge = 0.93f;
constexpr float kWindow = 2.0f;          // secondes d'écart au point de synchronisation qui servent d'échelle
constexpr float kLeadDrag = 0.10f;       // part de vitesse perdue à kWindow d'avance
constexpr float kDraft = 0.16f;          // part de vitesse gagnée à kWindow de retard

struct Input {
    float steer = 0.0f; // -1 à gauche, +1 à droite
    bool use = false;   // déclenche le bonus porté, à l'appui
};

// État d'un vaisseau en coordonnées de piste. Structure plate : un état
// complet se copie d'un bloc, ce dont la prédiction réseau a besoin.
struct Vehicle {
    float dist = 0.0f;       // distance parcourue depuis le départ
    float lateral = 0.0f;    // décalage par rapport à l'axe, positif à droite
    float lateralVel = 0.0f;
    float speed = 0.0f;
    float roll = 0.0f;       // inclinaison visuelle
    int ship = 0;            // modèle, voir shipStats
    bool boosting = false;   // en turbo
    bool scraping = false;   // frotte contre un muret
    bool finished = false;
    float finishTime = 0.0f; // temps de course au passage de la ligne
    float shieldTime = 0.0f; // secondes de bouclier restantes
    float turboTime = 0.0f;  // secondes de turbo restantes
    float stunTime = 0.0f;   // secondes restantes sans contrôle après un choc
    float airTime = 0.0f;    // secondes de saut restantes
    Item item = Item::None;  // bonus porté
    bool useHeld = false;    // état précédent de la commande, pour détecter l'appui
    bool boxHit = false;     // vient de passer sur une boîte à bonus, à servir au prochain pas
};

enum class MissileKind : uint8_t {
    Shot = 0, // suit le vaisseau de devant et s'éteint en le touchant
    Mine,     // immobile, elle attend qu'on lui roule dessus
    Comet     // remonte toute la piste et frappe chacun au passage
};

// Tir, mine ou comète, eux aussi en coordonnées de piste.
struct Missile {
    bool active = false;
    MissileKind kind = MissileKind::Shot;
    int owner = 0;
    float dist = 0.0f;
    float lateral = 0.0f;
    float speed = 0.0f;
    float life = 0.0f; // secondes avant de s'éteindre
    uint8_t struck = 0; // comète : vaisseaux déjà frappés, un bit chacun
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
// Hauteur d'un vaisseau au-dessus de la piste pendant un saut, en mètres.
float airHeight(const Vehicle& vehicle);

// Rang d'un vaisseau, 1 pour le premier.
int rankOf(const World& world, int index);

} // namespace sim
