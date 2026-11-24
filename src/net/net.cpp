#include "net/net.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace net {

namespace {

const uint32_t kMagic = 0x35474141u; // "AAG5"
constexpr double kTimeout = 5.0;      // secondes sans nouvelles avant de couper

enum Type : uint8_t {
    kDiscover = 1,
    kAnnounce,
    kJoin,
    kLobby,
    kStart,
    kInput,
    kSnapshot,
    kLeave,
};

// Format des paquets. Structures compactes à champs de taille fixe : les deux
// cibles sont petit-boutistes, donc la mémoire part telle quelle sur le fil.
#pragma pack(push, 1)
struct Header {
    uint32_t magic = kMagic;
    uint8_t type = 0;
};
struct WireRacer {
    char name[20];
    int8_t ship;
    int8_t profile;
};
struct WireConfig {
    int8_t track;
    int8_t ownClock;
    int8_t racerCount;
    uint8_t gridStart;
    WireRacer racers[sim::kMaxVehicles];
};
struct AnnouncePacket {
    Header h;
    char name[20];
    uint8_t players;
    int8_t track;
    uint8_t racing;
};
struct JoinPacket {
    Header h;
    char name[20];
    int8_t ship;
    uint64_t songReady;
};
struct LobbyPacket {
    Header h;
    WireConfig config;
    int8_t yourSlot;
    uint8_t racing;
    uint64_t songId;
};
struct StartPacket {
    Header h;
    WireConfig config;
    int8_t yourSlot;
};
struct WireInput {
    float steer;
    uint8_t flags; // bit 0 bonus
};
struct InputPacket {
    Header h;
    uint32_t seq;
    WireInput input;
};
struct WireVehicle {
    float dist, lateral, lateralVel, speed, roll, finishTime, shieldTime, turboTime, stunTime;
    uint8_t item;
    uint8_t flags; // bit 0 turbo, 1 frottement, 2 arrivé, 3 bouton tenu, 4 boîte franchie
};
struct WireMissile {
    uint8_t active; // bit 0 actif, bit 1 mine
    uint8_t owner;
    float dist, lateral, speed, life;
};
struct SnapshotPacket {
    Header h;
    uint32_t tick;
    uint32_t ackSeq;
    float countdown;
    float raceTime;
    double time;
    uint8_t count;
    WireVehicle vehicles[sim::kMaxVehicles];
    WireInput inputs[sim::kMaxVehicles];
    WireMissile missiles[sim::kMaxMissiles];
};
#pragma pack(pop)

WireInput packInput(const sim::Input& in) {
    WireInput w;
    w.steer = in.steer;
    w.flags = static_cast<uint8_t>(in.use ? 1 : 0);
    return w;
}

sim::Input unpackInput(const WireInput& w) {
    sim::Input in;
    in.steer = std::clamp(w.steer, -1.0f, 1.0f);
    in.use = w.flags & 1;
    return in;
}

WireConfig packConfig(const sim::RaceConfig& c) {
    WireConfig w{};
    w.track = static_cast<int8_t>(c.track);
    w.ownClock = c.ownClock ? 1 : 0;
    w.racerCount = static_cast<int8_t>(c.racerCount);
    w.gridStart = c.gridStart;
    for (int i = 0; i < sim::kMaxVehicles; i++) {
        std::memcpy(w.racers[i].name, c.racers[i].name, sizeof(w.racers[i].name));
        w.racers[i].ship = static_cast<int8_t>(c.racers[i].ship);
        w.racers[i].profile = static_cast<int8_t>(c.racers[i].profile);
    }
    return w;
}

// Les valeurs reçues du réseau sont bornées avant usage.
sim::RaceConfig unpackConfig(const WireConfig& w) {
    sim::RaceConfig c;
    c.track = std::clamp<int>(w.track, 0, sim::kSongSlots - 1);
    c.ownClock = w.ownClock != 0;
    c.racerCount = std::clamp<int>(w.racerCount, 0, sim::kMaxVehicles);
    c.gridStart = w.gridStart != 0;
    for (int i = 0; i < sim::kMaxVehicles; i++) {
        std::memcpy(c.racers[i].name, w.racers[i].name, sizeof(c.racers[i].name));
        c.racers[i].name[sizeof(c.racers[i].name) - 1] = '\0';
        c.racers[i].ship = std::clamp<int>(w.racers[i].ship, 0, sim::kShipCount - 1);
        c.racers[i].profile = std::clamp<int>(w.racers[i].profile, sim::kHuman,
                                              sim::kProfileCount - 1);
    }
    return c;
}

void copyName(char* dst, const char* src) {
    std::snprintf(dst, 20, "%s", src);
}

} // namespace

void formatIp(uint32_t ip, char* out, int size) {
    in_addr addr;
    addr.s_addr = ip;
    std::snprintf(out, static_cast<size_t>(size), "%s", inet_ntoa(addr));
}

uint32_t parseIp(const char* text) {
    in_addr addr;
    return inet_pton(AF_INET, text, &addr) == 1 ? addr.s_addr : 0;
}

static int openSocket(uint16_t port) {
    int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(s);
        return -1;
    }
    return s;
}

bool Net::host(const char* name, int ship, uint16_t port) {
    close();
    sock = openSocket(port);
    if (sock < 0) return false;
    role = Role::Host;
    racing = false;
    lobby = sim::RaceConfig{};
    lobby.racerCount = 1;
    copyName(lobby.racers[0].name, name);
    lobby.racers[0].ship = ship;
    lobby.racers[0].profile = sim::kHuman;
    for (Peer& p : peers) p = Peer{};
    songId = 0;
    // Sans ce port TCP, les clients ne pourront pas recevoir le morceau.
    if (!share.serve(port)) {
        close();
        return false;
    }
    role = Role::Host;
    return true;
}

bool Net::client(const char* name, int ship) {
    close();
    sock = openSocket(0); // port choisi par le système
    if (sock < 0) return false;
    role = Role::Client;
    copyName(myName, name);
    myShip = ship;
    joined = false;
    mySlot = -1;
    foundCount = 0;
    startPending = false;
    hostInLobby = true;
    hostSongId = 0;
    songReady = 0;
    return true;
}

void Net::close() {
    if (sock >= 0) {
        Header leave;
        leave.type = kLeave;
        if (role == Role::Client && joined) send(hostIp, hostPort, &leave, sizeof(leave));
        if (role == Role::Host)
            for (const Peer& p : peers)
                if (p.active) send(p.ip, p.port, &leave, sizeof(leave));
        ::close(sock);
    }
    sock = -1;
    share.close();
    role = Role::Off;
    joined = false;
    racing = false;
}

int Net::peerCount() const {
    int count = 0;
    for (const Peer& p : peers) count += p.active;
    return count;
}

int Net::peersReady() const {
    int count = 0;
    for (const Peer& p : peers) count += p.active && songId != 0 && p.songReady == songId;
    return count;
}

void Net::offerSong(uint64_t id, std::shared_ptr<const std::vector<uint8_t>> data) {
    songId = id;
    share.offer(id, std::move(data));
    if (role == Role::Host) hostSendLobby();
}

bool Net::connected() const {
    if (role == Role::Host) return true;
    return role == Role::Client && joined && now - lastHeard < kTimeout;
}

void Net::send(uint32_t ip, uint16_t port, const void* data, int size) {
    if (sock < 0) return;
    if (dropRate > 0.0f) {
        dropState = dropState * 1664525u + 1013904223u;
        if (static_cast<float>(dropState >> 8) / 16777216.0f < dropRate) return;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = port;
    ::sendto(sock, data, static_cast<size_t>(size), 0, reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr));
}

void Net::discover(uint16_t port) {
    Header h;
    h.type = kDiscover;
    // Diffusion sur le réseau local, plus la machine elle-même pour jouer à
    // deux instances sur le même poste.
    send(htonl(INADDR_BROADCAST), htons(port), &h, sizeof(h));
    send(htonl(INADDR_LOOPBACK), htons(port), &h, sizeof(h));
}

void Net::join(uint32_t ip, uint16_t port) {
    hostIp = ip;
    hostPort = htons(port);
    joined = false;
    mySlot = -1;
    lastHeard = now;
    lastJoinSend = -1.0;
    seq = 0;
    lastSnapshotTick = 0;
}

void Net::setShip(int ship) {
    myShip = ship;
    if (role == Role::Host) lobby.racers[0].ship = ship;
    lastJoinSend = -1.0; // renvoie la demande tout de suite
}

int Net::addPeer(uint32_t ip, uint16_t port, const char* name, int ship) {
    for (Peer& p : peers)
        if (p.active && p.ip == ip && p.port == port) {
            lobby.racers[p.slot].ship = std::clamp(ship, 0, sim::kShipCount - 1);
            p.lastHeard = now;
            return p.slot;
        }
    if (racing || lobby.racerCount >= sim::kMaxVehicles) return -1;
    for (Peer& p : peers) {
        if (p.active) continue;
        p = Peer{};
        p.active = true;
        p.ip = ip;
        p.port = port;
        p.slot = lobby.racerCount++;
        p.lastHeard = now;
        sim::RacerSetup& racer = lobby.racers[p.slot];
        copyName(racer.name, name);
        racer.ship = std::clamp(ship, 0, sim::kShipCount - 1);
        racer.profile = sim::kHuman;
        return p.slot;
    }
    return -1;
}

void Net::hostSendLobby() {
    LobbyPacket packet{};
    packet.h = Header{};
    packet.h.type = kLobby;
    packet.config = packConfig(lobby);
    packet.racing = racing;
    packet.songId = songId;
    for (const Peer& p : peers) {
        if (!p.active) continue;
        packet.yourSlot = static_cast<int8_t>(p.slot);
        send(p.ip, p.port, &packet, sizeof(packet));
    }
}

void Net::hostStart(const sim::RaceConfig& config) {
    racing = true;
    StartPacket packet{};
    packet.h = Header{};
    packet.h.type = kStart;
    packet.config = packConfig(config);
    for (Peer& p : peers) {
        if (!p.active) continue;
        p.lastSeq = 0;
        packet.yourSlot = static_cast<int8_t>(p.slot);
        // Trois envois : UDP ne garantit pas l'arrivée, et les instantanés qui
        // suivent déclenchent de toute façon le démarrage chez un retardataire.
        for (int i = 0; i < 3; i++) send(p.ip, p.port, &packet, sizeof(packet));
    }
}

void Net::hostBackToLobby() {
    racing = false;
    hostSendLobby();
}

void Net::hostSendSnapshot(const sim::Session& session) {
    const sim::World& w = session.world;
    SnapshotPacket packet{};
    packet.h = Header{};
    packet.h.type = kSnapshot;
    packet.tick = w.tick;
    packet.countdown = w.countdown;
    packet.raceTime = w.raceTime;
    packet.time = w.time;
    packet.count = static_cast<uint8_t>(w.count);
    for (int i = 0; i < w.count; i++) {
        const sim::Vehicle& v = w.vehicles[i];
        WireVehicle& o = packet.vehicles[i];
        o.dist = v.dist;
        o.lateral = v.lateral;
        o.lateralVel = v.lateralVel;
        o.speed = v.speed;
        o.roll = v.roll;
        o.finishTime = v.finishTime;
        o.shieldTime = v.shieldTime;
        o.turboTime = v.turboTime;
        o.stunTime = v.stunTime;
        o.item = static_cast<uint8_t>(v.item);
        o.flags = static_cast<uint8_t>((v.boosting ? 1 : 0) | (v.scraping ? 2 : 0) | (v.finished ? 4 : 0) |
                                       (v.useHeld ? 8 : 0) | (v.boxHit ? 16 : 0));
        packet.inputs[i] = packInput(session.lastInputs[i]);
    }
    for (int i = 0; i < sim::kMaxMissiles; i++) {
        const sim::Missile& m = w.missiles[i];
        packet.missiles[i] = {static_cast<uint8_t>((m.active ? 1 : 0) | (m.mine ? 2 : 0)), static_cast<uint8_t>(m.owner),
                              m.dist, m.lateral, m.speed, m.life};
    }
    for (const Peer& p : peers) {
        if (!p.active) continue;
        packet.ackSeq = p.lastSeq;
        send(p.ip, p.port, &packet, sizeof(packet));
    }
}

void Net::reconcile(sim::Session& session, const void* data) {
    const SnapshotPacket& s = *static_cast<const SnapshotPacket*>(data);
    sim::World& w = session.world;
    const sim::World predicted = w;

    w.tick = s.tick;
    w.countdown = s.countdown;
    w.raceTime = s.raceTime;
    w.time = s.time;
    w.count = std::min<int>(s.count, sim::kMaxVehicles);
    for (int i = 0; i < w.count; i++) {
        const WireVehicle& o = s.vehicles[i];
        sim::Vehicle& v = w.vehicles[i];
        v.dist = o.dist;
        v.lateral = o.lateral;
        v.lateralVel = o.lateralVel;
        v.speed = o.speed;
        v.roll = o.roll;
        v.finishTime = o.finishTime;
        v.shieldTime = o.shieldTime;
        v.turboTime = o.turboTime;
        v.stunTime = o.stunTime;
        v.item = o.item < static_cast<uint8_t>(sim::Item::Count) ? static_cast<sim::Item>(o.item) : sim::Item::None;
        v.boosting = o.flags & 1;
        v.scraping = o.flags & 2;
        v.finished = o.flags & 4;
        v.useHeld = o.flags & 8;
        v.boxHit = o.flags & 16;
        if (i != session.localIndex) session.remoteInputs[i] = unpackInput(s.inputs[i]);
    }
    for (int i = 0; i < sim::kMaxMissiles; i++) {
        const WireMissile& m = s.missiles[i];
        w.missiles[i].active = m.active & 1;
        w.missiles[i].mine = m.active & 2;
        w.missiles[i].owner = std::min<int>(m.owner, sim::kMaxVehicles - 1);
        w.missiles[i].dist = m.dist;
        w.missiles[i].lateral = m.lateral;
        w.missiles[i].speed = m.speed;
        w.missiles[i].life = m.life;
    }

    // Rejoue les commandes locales que l'hôte n'avait pas encore reçues.
    uint32_t pending = seq - s.ackSeq;
    session.previous = w;
    if (pending < static_cast<uint32_t>(kHistory)) {
        for (uint32_t k = s.ackSeq + 1; k <= seq; k++) session.tick(history[k % kHistory]);
    }

    for (int i = 0; i < w.count; i++) {
        corrDist[i] = predicted.vehicles[i].dist - w.vehicles[i].dist;
        corrLateral[i] = predicted.vehicles[i].lateral - w.vehicles[i].lateral;
    }
    corrected = true;
}

void Net::clientTick(sim::Session& session, const sim::Input& local) {
    seq++;
    history[seq % kHistory] = local;
    InputPacket packet{};
    packet.h = Header{};
    packet.h.type = kInput;
    packet.seq = seq;
    packet.input = packInput(local);
    send(hostIp, hostPort, &packet, sizeof(packet));
    session.tick(local);
}

void Net::poll(double time, sim::Session* session) {
    now = time;
    if (sock < 0) return;
    share.pump();

    uint8_t buffer[1500];
    for (;;) {
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        ssize_t n = ::recvfrom(sock, buffer, sizeof(buffer), 0,
                               reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (n < static_cast<ssize_t>(sizeof(Header))) break;
        Header h;
        std::memcpy(&h, buffer, sizeof(h));
        if (h.magic != kMagic) continue;
        uint32_t ip = from.sin_addr.s_addr;
        uint16_t port = from.sin_port;
        auto sized = [&](size_t size) { return static_cast<size_t>(n) >= size; };

        if (role == Role::Host) {
            Peer* peer = nullptr;
            for (Peer& p : peers)
                if (p.active && p.ip == ip && p.port == port) peer = &p;
            if (peer) peer->lastHeard = now;

            if (h.type == kDiscover) {
                AnnouncePacket a{};
                a.h = Header{};
                a.h.type = kAnnounce;
                copyName(a.name, lobby.racers[0].name);
                a.players = static_cast<uint8_t>(lobby.racerCount);
                a.track = static_cast<int8_t>(lobby.track);
                a.racing = racing;
                send(ip, port, &a, sizeof(a));
            } else if (h.type == kJoin && sized(sizeof(JoinPacket))) {
                JoinPacket j;
                std::memcpy(&j, buffer, sizeof(j));
                j.name[sizeof(j.name) - 1] = '\0';
                if (addPeer(ip, port, j.name, j.ship) >= 0) {
                    for (Peer& p : peers)
                        if (p.active && p.ip == ip && p.port == port) p.songReady = j.songReady;
                    hostSendLobby();
                }
            } else if (h.type == kInput && peer && session && sized(sizeof(InputPacket))) {
                InputPacket in;
                std::memcpy(&in, buffer, sizeof(in));
                // Un paquet arrivé en retard ne remplace pas une commande plus récente.
                if (in.seq > peer->lastSeq) {
                    peer->lastSeq = in.seq;
                    if (peer->slot < sim::kMaxVehicles)
                        session->remoteInputs[peer->slot] = unpackInput(in.input);
                }
            } else if (h.type == kLeave && peer) {
                peer->lastHeard = -1e9; // traité comme un délai dépassé, plus bas
            }
        } else if (role == Role::Client) {
            if (h.type == kAnnounce && sized(sizeof(AnnouncePacket))) {
                AnnouncePacket a;
                std::memcpy(&a, buffer, sizeof(a));
                a.name[sizeof(a.name) - 1] = '\0';
                FoundHost* slot = nullptr;
                // Un hôte de la même machine répond deux fois, par la boucle locale
                // et par l'interface réseau : même nom et même port, une seule entrée.
                for (int i = 0; i < foundCount; i++)
                    if (found[i].port == ntohs(port) &&
                        (found[i].ip == ip || std::strcmp(found[i].name, a.name) == 0))
                        slot = &found[i];
                if (slot && slot->ip != ip && now - slot->seen < 2.0) continue;
                if (!slot && foundCount < kMaxFound) slot = &found[foundCount++];
                if (slot) {
                    copyName(slot->name, a.name);
                    slot->ip = ip;
                    slot->port = ntohs(port);
                    slot->players = a.players;
                    slot->track = a.track;
                    slot->racing = a.racing != 0;
                    slot->seen = now;
                }
                continue;
            }
            if (ip != hostIp || port != hostPort) continue;
            lastHeard = now;
            if (h.type == kLobby && sized(sizeof(LobbyPacket))) {
                LobbyPacket l;
                std::memcpy(&l, buffer, sizeof(l));
                lobby = unpackConfig(l.config);
                mySlot = l.yourSlot;
                joined = true;
                hostInLobby = l.racing == 0;
                hostSongId = l.songId;
            } else if (h.type == kStart && sized(sizeof(StartPacket))) {
                StartPacket st;
                std::memcpy(&st, buffer, sizeof(st));
                // Un doublon de START ne relance pas une course déjà démarrée.
                if (hostInLobby || !session) {
                    startConfig = unpackConfig(st.config);
                    mySlot = st.yourSlot;
                    startPending = true;
                    hostInLobby = false;
                    seq = 0;
                    lastSnapshotTick = 0;
                }
            } else if (h.type == kSnapshot && session && sized(sizeof(SnapshotPacket))) {
                SnapshotPacket s;
                std::memcpy(&s, buffer, sizeof(s));
                if (s.tick > lastSnapshotTick && !startPending) {
                    lastSnapshotTick = s.tick;
                    reconcile(*session, &s);
                }
            } else if (h.type == kLeave) {
                joined = false;
                lastHeard = -1e9;
            }
        }
    }

    if (role == Role::Host) {
        // Joueurs muets depuis trop longtemps : hors du salon, ou confiés à
        // l'ordinateur si la course est lancée.
        for (Peer& p : peers) {
            if (!p.active || now - p.lastHeard < kTimeout) continue;
            p.active = false;
            if (racing) {
                if (session && p.slot < session->config.racerCount)
                    session->config.racers[p.slot].profile = 2;
            }
            if (!racing) {
                for (int i = p.slot; i + 1 < lobby.racerCount; i++)
                    lobby.racers[i] = lobby.racers[i + 1];
                lobby.racerCount--;
                for (Peer& other : peers)
                    if (other.active && other.slot > p.slot) other.slot--;
            }
        }
        if (now - lastLobbySend > 0.25) {
            lastLobbySend = now;
            hostSendLobby();
        }
    } else if (role == Role::Client && hostIp != 0) {
        // Demande d'entrée répétée tant que l'hôte n'a pas répondu, puis à
        // faible cadence pour signaler qu'on est toujours là.
        double period = joined ? 1.0 : 0.25;
        if (now - lastJoinSend > period) {
            lastJoinSend = now;
            JoinPacket j{};
            j.h = Header{};
            j.h.type = kJoin;
            copyName(j.name, myName);
            j.ship = static_cast<int8_t>(myShip);
            j.songReady = songReady;
            send(hostIp, hostPort, &j, sizeof(j));
        }
    }
}

} // namespace net
