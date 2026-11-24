#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace net {

// Envoi d'un gros bloc de données de l'hôte aux clients, sur TCP : le morceau
// de la course et sa partition. UDP ne convient pas à quelques mégaoctets qui
// doivent arriver entiers.
//
// Tout est non bloquant : pump() avance les transferts en cours et rend la
// main aussitôt, il s'appelle à chaque image.
struct Share {
    static constexpr size_t kMaxSize = 64u * 1024u * 1024u;
    static constexpr int kMaxLinks = 8;

    // --- Hôte
    bool serve(uint16_t port);
    // Bloc proposé aux clients. Changer de bloc coupe les envois en cours.
    void offer(uint64_t id, std::shared_ptr<const std::vector<uint8_t>> data);

    // --- Client
    // Demande à l'hôte le bloc d'identifiant donné. `ip` en ordre réseau.
    void fetch(uint32_t ip, uint16_t port, uint64_t id);
    bool fetching() const { return client >= 0; }
    // Part reçue, de 0 à 1.
    float progress() const;
    // Vrai une fois, quand le bloc demandé est arrivé entier : il est alors
    // dans `received`.
    bool take(uint64_t id);
    std::vector<uint8_t> received;

    void pump();
    void close();

    int listener = -1;
    uint64_t offeredId = 0;
    std::shared_ptr<const std::vector<uint8_t>> offered;
    struct Link {
        int sock = -1;
        size_t sent = 0;
        std::shared_ptr<const std::vector<uint8_t>> data;
        uint8_t header[16] = {};
        size_t headerSent = 0;
    };
    Link links[kMaxLinks];

    int client = -1;
    uint64_t wantedId = 0;
    uint8_t header[16] = {};
    size_t headerGot = 0;
    size_t expected = 0;
    bool done = false;
    uint32_t fromIp = 0;
    uint16_t fromPort = 0;
};

} // namespace net
