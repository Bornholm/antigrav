#pragma once

#include "sim/track.h"
#include "sim/world.h"

namespace sim {

constexpr int kHuman = -1;

struct RacerSetup {
    char name[20] = "";
    int ship = 0;
    int profile = kHuman; // personnalité d'IA, ou kHuman pour un joueur
};

// Tout ce qui définit une course. C'est aussi ce que l'hôte envoie aux clients.
struct RaceConfig {
    int track = 0;
    int laps = 3;
    int racerCount = 0;
    RacerSetup racers[kMaxVehicles];
    bool gridStart = true; // départ arrêté avec compte à rebours
};

// Une course en cours : circuit, état du monde et origine des commandes de
// chaque vaisseau. Sans affichage, donc utilisable par les tests et le réseau.
struct Session {
    RaceConfig config;
    Track track;
    World world;
    World previous; // état du pas précédent, pour interpoler le rendu
    // Vaisseau piloté par les commandes locales et suivi par la caméra.
    int localIndex = 0;
    // Le vaisseau local est piloté par l'ordinateur : démonstration et captures.
    bool autopilot = false;
    // Commandes des joueurs distants, tenues à jour par le code réseau.
    Input remoteInputs[kMaxVehicles];
    // Commandes appliquées au dernier pas.
    Input lastInputs[kMaxVehicles];

    void start(const RaceConfig& config, int localIndex);
    // Commandes de chaque vaisseau pour le prochain pas.
    void gatherInputs(const Input& local, Input* out) const;
    // Avance d'un pas fixe.
    void tick(const Input& local);
};

} // namespace sim
