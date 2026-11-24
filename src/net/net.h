#pragma once

#include "net/share.h"
#include "sim/session.h"

#include <cstdint>

namespace net {

constexpr uint16_t kPort = 47800;
constexpr int kMaxFound = 8;
constexpr int kHistory = 128;

enum class Role { Off, Host, Client };

// Partie annoncée sur le réseau local.
struct FoundHost {
    char name[20] = "";
    uint32_t ip = 0;   // ordre réseau
    uint16_t port = 0; // ordre hôte
    int players = 0;
    int track = 0;
    bool racing = false;
    double seen = 0.0;
};

// Multijoueur en réseau local, sur UDP.
//
// L'hôte fait autorité : il simule la course et diffuse des instantanés. Chaque
// client envoie ses commandes, simule en avance pour ne pas ressentir la
// latence, puis se recale sur l'hôte en rejouant les commandes pas encore
// prises en compte.
struct Net {
    Role role = Role::Off;
    int sock = -1;
    double now = 0.0;
    // Perte de paquets simulée, pour les tests : part des envois abandonnés.
    float dropRate = 0.0f;
    uint32_t dropState = 0x2545F491u;

    // --- Hôte
    struct Peer {
        bool active = false;
        uint32_t ip = 0;
        uint16_t port = 0; // ordre réseau
        int slot = 0;
        double lastHeard = 0.0;
        uint32_t lastSeq = 0;
        uint64_t songReady = 0; // morceau que ce joueur a reçu et préparé
    };
    Peer peers[sim::kMaxVehicles];
    // Joueurs humains présents dans le salon. L'emplacement 0 est l'hôte.
    sim::RaceConfig lobby;
    bool racing = false;
    double lastLobbySend = 0.0;
    // Morceau de la course, envoyé aux clients par TCP sur le même numéro de
    // port. songId l'identifie ; 0 tant que rien n'est proposé.
    Share share;
    uint64_t songId = 0;
    // Hôte : nombre de joueurs distants, et combien ont le morceau.
    int peerCount() const;
    int peersReady() const;
    void offerSong(uint64_t id, std::shared_ptr<const std::vector<uint8_t>> data);

    // --- Client
    uint32_t hostIp = 0;
    uint16_t hostPort = 0; // ordre réseau
    bool joined = false;
    int mySlot = -1;
    double lastHeard = 0.0;
    double lastJoinSend = 0.0;
    char myName[20] = "";
    int myShip = 0;
    // Morceau annoncé par l'hôte, et celui que ce client a fini de préparer.
    uint64_t hostSongId = 0;
    uint64_t songReady = 0;
    // L'hôte a lancé la course : la configuration à démarrer est dans startConfig.
    bool startPending = false;
    sim::RaceConfig startConfig;
    // L'hôte est revenu au salon.
    bool hostInLobby = true;
    uint32_t seq = 0;
    sim::Input history[kHistory];
    uint32_t lastSnapshotTick = 0;
    // Écarts entre l'état prédit et l'état corrigé du dernier recalage. Le jeu
    // les résorbe à l'affichage pour qu'une correction ne saute pas aux yeux.
    float corrDist[sim::kMaxVehicles] = {};
    float corrLateral[sim::kMaxVehicles] = {};
    bool corrected = false;
    FoundHost found[kMaxFound];
    int foundCount = 0;

    bool host(const char* name, int ship, uint16_t port = kPort);
    bool client(const char* name, int ship);
    void close();

    // Client : cherche des parties sur le réseau local.
    void discover(uint16_t port = kPort);
    void join(uint32_t ip, uint16_t port);
    // Change de vaisseau dans le salon.
    void setShip(int ship);

    // Lit tous les paquets en attente. `session` est la course en cours, ou nul
    // dans les menus.
    void poll(double now, sim::Session* session);

    // Hôte : démarre la course pour tout le monde avec cette configuration.
    void hostStart(const sim::RaceConfig& config);
    void hostSendSnapshot(const sim::Session& session);
    void hostBackToLobby();

    // Client : un pas de course. Envoie la commande et prédit localement.
    void clientTick(sim::Session& session, const sim::Input& local);

    bool connected() const;

  private:
    void send(uint32_t ip, uint16_t port, const void* data, int size);
    void hostSendLobby();
    void reconcile(sim::Session& session, const void* snapshot);
    int addPeer(uint32_t ip, uint16_t port, const char* name, int ship);
};

// Adresse en notation pointée, pour l'affichage. `ip` en ordre réseau.
void formatIp(uint32_t ip, char* out, int size);
// Renvoie 0 si le texte n'est pas une adresse IPv4.
uint32_t parseIp(const char* text);

} // namespace net
