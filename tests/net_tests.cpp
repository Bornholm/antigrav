// Tests du réseau : un hôte et un client dans le même processus, reliés par
// la boucle locale.
#include "net/net.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
#include <string_view>
#include <unistd.h>

namespace {

int failures = 0;

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("ECHEC %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            failures++;                                                         \
        }                                                                       \
    } while (0)

const uint16_t kTestPort = 47931;

// Fait circuler les paquets : la boucle locale les livre presque aussitôt.
void pump(net::Net& host, net::Net& client, sim::Session* hs, sim::Session* cs, double& now) {
    for (int i = 0; i < 4; i++) {
        usleep(300);
        now += 0.01;
        host.poll(now, hs);
        client.poll(now, cs);
    }
}

// Joue une course complète entre un hôte et un client, avec la part de paquets
// perdus donnée. Renvoie l'écart latéral moyen corrigé chez le client.
void playRace(float dropRate, int latencyTicks) {
    net::Net host, client;
    double now = 0.0;
    CHECK(host.host("Hote", 0, kTestPort));
    CHECK(client.client("Invite", 1));

    // Découverte.
    client.discover(kTestPort);
    pump(host, client, nullptr, nullptr, now);
    CHECK(client.foundCount == 1);
    if (client.foundCount != 1) return;
    CHECK(client.found[0].players == 1);

    // Entrée dans le salon.
    client.join(client.found[0].ip, client.found[0].port);
    for (int i = 0; i < 20 && !client.joined; i++) {
        now += 0.3;
        pump(host, client, nullptr, nullptr, now);
    }
    CHECK(client.joined);
    CHECK(client.mySlot == 1);
    CHECK(host.lobby.racerCount == 2);
    CHECK(client.lobby.racerCount == 2);

    // L'hôte complète avec deux IA et lance.
    sim::RaceConfig config = host.lobby;
    config.track = 0;
    config.gridStart = true;
    for (int profile : {0, 2}) {
        sim::RacerSetup& ai = config.racers[config.racerCount++];
        std::snprintf(ai.name, sizeof(ai.name), "IA %d", profile);
        ai.ship = 2 + config.racerCount % 2;
        ai.profile = profile;
    }
    host.dropRate = client.dropRate = dropRate;
    sim::Session hostSession, clientSession;
    hostSession.start(config, 0);
    host.hostStart(config);
    for (int i = 0; i < 30 && !client.startPending; i++) {
        host.hostStart(config);
        pump(host, client, &hostSession, nullptr, now);
    }
    CHECK(client.startPending);
    clientSession.start(client.startConfig, client.mySlot);
    client.startPending = false;
    CHECK(clientSession.config.racerCount == 4);
    CHECK(clientSession.localIndex == 1);

    // Course. Le client prend `latencyTicks` pas d'avance sur l'hôte, comme
    // s'il y avait ce délai sur le réseau.
    double correction = 0.0;
    int corrections = 0;
    float worstJump = 0.0f;
    const int steps = 60 * 125; // le morceau dure 100 secondes
    for (int t = 0; t < steps; t++) {
        now += sim::kFixedDt;
        sim::Input hostInput, clientInput;
        hostInput.steer = 0.5f * std::sin(static_cast<float>(t) * 0.02f);
        hostInput.use = t % 240 == 0;
        clientInput.steer = 0.6f * std::sin(static_cast<float>(t) * 0.031f + 1.0f);
        clientInput.use = t % 200 == 100;

        client.clientTick(clientSession, clientInput);
        if (t >= latencyTicks) {
            usleep(150);
            host.poll(now, &hostSession);
            hostSession.tick(hostInput);
            if (hostSession.world.tick % 2 == 0) host.hostSendSnapshot(hostSession);
        }
        usleep(150);
        client.poll(now, &clientSession);
        if (client.corrected) {
            client.corrected = false;
            float jump = std::abs(client.corrLateral[1]);
            correction += jump;
            worstJump = std::max(worstJump, jump);
            corrections++;
        }
    }

    CHECK(corrections > steps / 8);
    // La prédiction du vaisseau local colle à l'hôte : les recalages restent
    // de l'ordre de quelques centimètres en moyenne.
    double mean = corrections ? correction / corrections : 99.0;
    std::printf("perte %.0f %%, avance %d pas: %d recalages, écart latéral moyen %.3f m, "
                "pire %.2f m\n",
                dropRate * 100.0f, latencyTicks, corrections, mean, worstJump);
    CHECK(mean < 0.25);

    // Les deux côtés racontent la même course.
    for (int i = 0; i < 4; i++) {
        const sim::Vehicle& h = hostSession.world.vehicles[i];
        const sim::Vehicle& c = clientSession.world.vehicles[i];
        CHECK(std::abs(h.dist - c.dist) < 60.0f + 400.0f * sim::kFixedDt * latencyTicks);
        CHECK(std::abs(h.lateral - c.lateral) < 3.0f);
        CHECK(h.finished == c.finished);
        if (h.finished && c.finished) CHECK(std::abs(h.finishTime - c.finishTime) < 0.05f);
    }
    CHECK(hostSession.world.vehicles[0].finished);
    host.close();
    client.close();
}

// Transfert du morceau de l'hôte aux clients, et attente de leur accord.
void testSongShare() {
    const uint16_t port = 47961;
    net::Net host, client;
    CHECK(host.host("Hote", 0, port));
    CHECK(client.client("Invite", 1));
    client.join(net::parseIp("127.0.0.1"), port);
    double now = 0.0;
    auto spin = [&](int rounds) {
        for (int i = 0; i < rounds; i++) {
            now += 0.02;
            host.poll(now, nullptr);
            client.poll(now, nullptr);
            usleep(500);
        }
    };
    spin(40);
    CHECK(client.joined);
    CHECK(host.peerCount() == 1 && host.peersReady() == 0);

    // Trois mégaoctets, au contenu reconnaissable.
    auto blob = std::make_shared<std::vector<uint8_t>>(3u * 1024u * 1024u + 17u);
    uint32_t state = 99u;
    for (uint8_t& b : *blob) {
        state = state * 1664525u + 1013904223u;
        b = static_cast<uint8_t>(state >> 24);
    }
    const uint64_t id = 0x1234ABCDull;
    host.offerSong(id, blob);
    spin(40);
    CHECK(client.hostSongId == id);

    client.share.fetch(client.hostIp, port, id);
    bool got = false;
    for (int i = 0; i < 4000 && !got; i++) {
        spin(1);
        got = client.share.take(id);
    }
    CHECK(got);
    CHECK(client.share.received == *blob);
    CHECK(host.peersReady() == 0);

    // Le client se dit prêt : l'hôte le voit à son prochain signe de vie.
    client.songReady = id;
    client.lastJoinSend = -1.0;
    spin(40);
    CHECK(host.peersReady() == 1);

    // Nouveau morceau : plus personne n'est prêt, et l'ancien n'est plus servi.
    auto other = std::make_shared<std::vector<uint8_t>>(1000, uint8_t(7));
    host.offerSong(id + 1, other);
    spin(40);
    CHECK(client.hostSongId == id + 1);
    CHECK(host.peersReady() == 0);
    client.share.fetch(client.hostIp, port, id);
    got = false;
    for (int i = 0; i < 200 && !got; i++) {
        spin(1);
        got = client.share.take(id);
    }
    CHECK(!got);
    client.share.fetch(client.hostIp, port, id + 1);
    for (int i = 0; i < 400 && !got; i++) {
        spin(1);
        got = client.share.take(id + 1);
    }
    CHECK(got && client.share.received == *other);
    host.close();
    client.close();
}

void testAddresses() {
    char text[32];
    uint32_t ip = net::parseIp("192.168.1.42");
    CHECK(ip != 0);
    net::formatIp(ip, text, sizeof(text));
    CHECK(std::string_view(text) == "192.168.1.42");
    CHECK(net::parseIp("pas une adresse") == 0);
    CHECK(net::parseIp("300.1.1.1") == 0);
}

} // namespace

int main() {
    testSongShare();
    testAddresses();
    playRace(0.0f, 0);
    playRace(0.0f, 3);
    playRace(0.25f, 3);
    if (failures == 0) std::printf("réseau: tous les tests passent\n");
    return failures == 0 ? 0 : 1;
}
