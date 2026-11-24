#include "game/app.h"

#include "core/log.h"

#include <SDL3/SDL.h>

#include <arpa/inet.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <mutex>
#include <string>
#include <cstdio>
#include <cstring>

#ifdef __ANDROID__
#include <jni.h>
#endif

namespace game {

namespace {

using gfx::Align;
using gfx::FontId;
using glm::vec2;
using glm::vec4;

const vec4 kWhite{1.0f, 1.0f, 1.0f, 1.0f};
const vec4 kCyan{0.25f, 0.92f, 1.0f, 1.0f};
const vec4 kMagenta{1.0f, 0.28f, 0.86f, 1.0f};
const vec4 kYellow{1.0f, 0.80f, 0.25f, 1.0f};
const vec4 kDim{0.75f, 0.82f, 1.0f, 0.75f};

const char* const kAiNames[] = {"SynthFox", "KazeVR", "NovaBlade", "Vex", "Orbit", "Lumen", "Kyra"};
const char* const kPersonalityLabels[] = {"Variées", "Agressif", "Défensif", "Pilote",
                                          "Imprévisible"};

void clientTickThunk(void* user, Race& race, const sim::Input& local) {
    static_cast<net::Net*>(user)->clientTick(race, local);
}

// Android filtre les paquets diffusés tant qu'aucune application n'a pris un
// verrou multicast. Sans lui, un hôte sur téléphone reste invisible.
void acquireMulticastLock() {
#ifdef __ANDROID__
    JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "acquireMulticastLock", "()V");
    if (method) env->CallVoidMethod(activity, method);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
#endif
}

} // namespace

namespace {

// Résultat du sélecteur de fichiers. Son rappel peut venir d'un autre fil.
std::mutex dialogMutex;
std::string dialogPath;
bool dialogDone = false;
bool dialogOpen = false;

void SDLCALL onSongChosen(void*, const char* const* files, int) {
    std::lock_guard<std::mutex> lock(dialogMutex);
    dialogPath = files && files[0] ? files[0] : "";
    dialogDone = true;
}

} // namespace

const char* App::songLabel(int index) const {
    if (index < sim::kTrackCount) return sim::trackInfo(index).name;
    const int entry = index - sim::kTrackCount;
    if (entry < static_cast<int>(songs.entries.size())) return songs.entries[static_cast<size_t>(entry)].name.c_str();
    return "Ouvrir un fichier...";
}

// Passe au morceau d'indice donné dans le sélecteur. Un morceau du joueur est
// décodé et analysé ici.
void App::selectSong(int index) {
    songIndex = index;
    notice = "";
    if (index < sim::kTrackCount) {
        track = index;
    } else if (index - sim::kTrackCount < static_cast<int>(songs.entries.size())) {
        if (!songs.load(index - sim::kTrackCount, music)) {
            notice = songs.error.c_str();
            return;
        }
        track = sim::kCustomTrack;
    } else {
        return; // l'entrée « ouvrir » ne change pas la piste de démonstration
    }
    startAttract();
    publishSong();
}

void App::openSongDialog() {
    static const SDL_DialogFileFilter filters[] = {
        {"Musique", "mp3;ogg;flac;wav"},
        {"Tous les fichiers", "*"},
    };
    {
        std::lock_guard<std::mutex> lock(dialogMutex);
        if (dialogOpen) return;
        dialogOpen = true;
        dialogDone = false;
    }
    notice = "";
    SDL_ShowOpenFileDialog(onSongChosen, nullptr, window, filters, 2, nullptr, false);
}

void App::pollSongDialog() {
    std::string path;
    {
        std::lock_guard<std::mutex> lock(dialogMutex);
        if (!dialogDone) return;
        dialogDone = false;
        dialogOpen = false;
        path = dialogPath;
    }
    if (!path.empty()) importSong(path); // vide : annulé
}

bool App::importSong(const std::string& path) {
    const int entry = songs.import(path);
    if (entry < 0) {
        notice = songs.error.c_str();
        return false;
    }
    selectSong(sim::kTrackCount + entry);
    return track == sim::kCustomTrack;
}

void App::publishSong() {
    if (net.role != net::Role::Host) return;
    net.lobby.track = track;
    auto bytes = std::make_shared<std::vector<unsigned char>>(songs.makePackage(track));
    if (bytes->empty()) {
        notice = songs.error.c_str();
        publishedSong = 0;
        net.offerSong(0, nullptr);
        return;
    }
    publishedSong = packageId(*bytes);
    net.offerSong(publishedSong, bytes);
}

void App::receiveSong() {
    songStatus[0] = '\0';
    if (net.role != net::Role::Client || !net.joined || net.hostSongId == 0) return;
    if (net.hostSongId == appliedSong) return;
    // Pendant une course, on ne touche pas au morceau en cours.
    if (inRace) return;
    if (net.share.take(net.hostSongId)) {
        if (packageId(net.share.received) == net.hostSongId &&
            songs.applyPackage(net.share.received, music, hostSong)) {
            appliedSong = net.hostSongId;
            logInfo("Morceau de l'hôte reçu : %s, %zu octets, %zu éléments de piste", hostSong.name,
                    net.share.received.size(), hostSong.features.size());
            net.songReady = appliedSong;
            net.lastJoinSend = -1.0; // prévient l'hôte tout de suite
            track = hostSong.track;
            songIndex = track < sim::kTrackCount ? track : sim::kTrackCount;
            race.useFeatures = true;
            race.givenFeatures = hostSong.features;
            race.keepSong = true;
            startAttract();
        } else {
            notice = songs.error.empty() ? "Morceau reçu abîmé" : songs.error.c_str();
        }
        net.share.received.clear();
        return;
    }
    if (!net.share.fetching() || net.share.wantedId != net.hostSongId)
        net.share.fetch(net.hostIp, ntohs(net.hostPort), net.hostSongId);
    std::snprintf(songStatus, sizeof(songStatus), "Réception du morceau : %d %%",
                  static_cast<int>(net.share.progress() * 100.0f));
}

bool App::useSongFile(const std::string& path) {
    const size_t cut = path.find_last_of('/');
    std::string name = cut == std::string::npos ? path : path.substr(cut + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name.resize(dot);
    if (!songs.loadFile(path, name, music)) return false;
    track = sim::kCustomTrack;
    return true;
}

void App::init(int startTrack) {
    songs.scan();
    track = std::clamp(startTrack, 0, sim::kSongSlots - 1);
    songIndex = track < sim::kTrackCount ? track : sim::kTrackCount;
    std::snprintf(playerName, sizeof(playerName), "Pilote %02d",
                  static_cast<int>(SDL_GetTicksNS() / 1000 % 90) + 10);
    startAttract();
}

void App::go(Screen next) {
    screen = next;
    confirmQuit = false;
    ui.reset();
}

// Course de démonstration derrière les menus : le vaisseau choisi, filmé en
// orbite, sur le circuit choisi.
void App::startAttract() {
    RaceConfig config;
    config.track = track;
    config.gridStart = false;
    config.racerCount = 1;
    std::snprintf(config.racers[0].name, sizeof(config.racers[0].name), "%s", playerName);
    config.racers[0].ship = ship;
    config.racers[0].profile = 2;
    config = withOpponents(config, 3);
    // Démonstration sans combat : un bouclier ou une vrille cacherait le vaisseau.
    for (int i = 0; i < config.racerCount; i++) config.racers[i].profile = 2;
    race.clientTick = nullptr;
    race.init(config, 0);
    race.autopilot = true;
    race.orbit = true;
    race.showHud = false;
    race.synth = nullptr;
    inRace = false;
    if (synth) synth->setEngine(0.0f, false, false);
}

RaceConfig App::withOpponents(RaceConfig base, int count) const {
    int humans = base.racerCount;
    for (int i = 0; i < count && base.racerCount < sim::kMaxVehicles; i++) {
        RacerSetup& ai = base.racers[base.racerCount++];
        std::snprintf(ai.name, sizeof(ai.name), "%s", kAiNames[i % 7]);
        ai.ship = (ship + 1 + i) % sim::kShipCount;
        ai.profile = personality == 0 ? i % sim::kProfileCount : personality - 1;
    }
    (void)humans;
    return base;
}

void App::startQuickRace(const RaceConfig& config, bool autopilot) {
    track = config.track;
    race.clientTick = nullptr;
    race.init(config, 0);
    race.autopilot = autopilot;
    race.orbit = false;
    race.showHud = true;
    race.synth = synth;
    inRace = true;
    go(Screen::Race);
}

void App::startSolo() {
    RaceConfig config;
    config.track = track;
    config.gridStart = true;
    config.racerCount = 1;
    std::snprintf(config.racers[0].name, sizeof(config.racers[0].name), "%s", playerName);
    config.racers[0].ship = ship;
    config.racers[0].profile = kHuman;
    startQuickRace(withOpponents(config, opponents), false);
}

void App::startHosted() {
    RaceConfig config = net.lobby;
    config.track = track;
    config.gridStart = true;
    int room = sim::kMaxVehicles - config.racerCount;
    config = withOpponents(config, std::min(opponents, room));
    net.hostStart(config);
    startQuickRace(config, false);
    lastSnapshotTick = 0;
}

void App::leaveNetwork() {
    net.close();
    publishedSong = 0;
    appliedSong = 0;
    race.useFeatures = false;
    race.keepSong = false;
    songStatus[0] = '\0';
    startAttract();
}

void App::update(double time, float dt, const Controls& controls, const UiInput& input,
                 glm::vec2 screenSize, const gfx::FontAtlas& fonts) {
    now = time;
    pollSongDialog();
    race.statsLine = statsLine;
    // Thème dans les menus. En course, le morceau du circuit, calé sur le
    // temps de course : il démarre au signal du départ.
    if (music) {
        if (screen == Screen::Race) {
            // Horloge par joueur : le morceau avance avec le vaisseau, plus
            // vite quand il va vite, au ralenti quand il percute.
            const sim::Vehicle& player = race.world.vehicles[race.localIndex];
            // Instant du morceau, affiné entre deux pas de simulation. La
            // course n'a pas de pause : la musique tourne dès le départ. Il
            // ne faut surtout pas la dire arrêtée quand une image passe sans
            // pas de simulation, ce qui arrive une image sur deux à 120 Hz.
            const bool moving = race.world.countdown <= 0.0f;
            const double songTime = moving ? sim::songTime(race.world, race.track, player) + race.accumulator : -1.0;
            // Vitesse du morceau : 1 avec l'horloge commune, celle du vaisseau
            // rapportée à celle de la musique avec l'horloge par joueur.
            double speed = 1.0;
            if (race.world.ownClock) {
                const sim::TrackFrame f = race.track.at(player.dist);
                speed = player.speed / std::max(1.0f, f.pace) / std::max(0.5f, 1.0f - f.curvature * player.lateral);
            }
            music->sync(Music::kFirstTrack + race.config.track, songTime, moving, speed);
        } else {
            music->play(Music::kTheme);
        }
    }
    // La démonstration derrière les menus repart quand son morceau est fini.
    if (screen != Screen::Race && !inRace && race.finished()) startAttract();

    // Réseau : lit les paquets, puis réagit aux changements d'état.
    net.poll(now, inRace ? &race : nullptr);
    if (net.role == net::Role::Client) {
        receiveSong();
        // Le départ attend que le morceau de l'hôte soit installé.
        if (net.startPending && (net.hostSongId == 0 || net.hostSongId == appliedSong)) {
            net.startPending = false;
            track = net.startConfig.track;
            race.init(net.startConfig, net.mySlot);
            race.clientTick = clientTickThunk;
            race.clientUser = &net;
            race.autopilot = false;
            race.orbit = false;
            race.showHud = true;
            race.synth = synth;
            inRace = true;
            go(Screen::Race);
        }
        if (inRace && net.corrected) {
            // L'écart de recalage est résorbé à l'affichage plutôt que montré d'un coup.
            for (int i = 0; i < race.world.count; i++) {
                race.smoothDist[i] += net.corrDist[i];
                race.smoothLateral[i] += net.corrLateral[i];
            }
        }
        net.corrected = false;
        if (inRace && net.hostInLobby && net.joined) {
            startAttract();
            go(Screen::Lobby);
        }
        if ((screen == Screen::Lobby || inRace) && net.hostIp != 0 && !net.connected() &&
            now - net.lastHeard > 5.0) {
            leaveNetwork();
            notice = "Connexion à l'hôte perdue";
            go(Screen::Multi);
        }
        if (screen == Screen::Multi && now - lastDiscover > 1.0) {
            lastDiscover = now;
            net.discover();
        }
    }

    if (autoHostPlayers > 0 && net.role == net::Role::Host && screen == Screen::Lobby &&
        net.lobby.racerCount >= autoHostPlayers && publishedSong != 0 && net.peersReady() == net.peerCount())
        startHosted();
    if (autoJoin && screen == Screen::Multi && net.foundCount > 0) {
        net.join(net.found[0].ip, net.found[0].port);
        go(Screen::Lobby);
    }

    race.update(dt, controls);

    if (net.role == net::Role::Host && inRace) {
        // Un instantané tous les deux pas, soit trente par seconde.
        uint32_t tick = race.world.tick;
        if (tick != lastSnapshotTick && tick % 2 == 0) {
            lastSnapshotTick = tick;
            net.hostSendSnapshot(race);
        }
    }
    if (race.orbit) {
        race.orbitAngle += dt * 0.35f;
        // Le vaisseau se place là où le menu laisse de la place.
        glm::vec2 wanted = screen == Screen::Main ? glm::vec2(0.0f, 4.5f)
                           : (screen == Screen::Solo || screen == Screen::Lobby
                                  ? glm::vec2(7.0f, 3.0f)
                                  : glm::vec2(0.0f, 4.5f));
        race.orbitShift += (wanted - race.orbitShift) * std::min(1.0f, dt * 5.0f);
    }

    // Interface des menus, ou bandeau de fin de course.
    menuHud.begin(fonts, screenSize.x, screenSize.y);
    ui.begin(menuHud, input, static_cast<float>(now));
    if (screen == Screen::Race) {
        bool over = race.finished();
        auto abandon = [&] {
            if (net.role != net::Role::Off) leaveNetwork();
            else startAttract();
            go(Screen::Main);
        };
        if (over) {
            if (ui.button({800.0f, 560.0f}, {190.0f, 38.0f}, "CONTINUER", true, kYellow)) {
                if (net.role == net::Role::Host) {
                    net.hostBackToLobby();
                    startAttract();
                    go(Screen::Lobby);
                } else if (net.role == net::Role::Client) {
                    startAttract();
                    go(Screen::Lobby);
                } else {
                    startAttract();
                    go(Screen::Solo);
                }
            } else if (input.back) {
                abandon();
            }
        } else if (confirmQuit) {
            // La course continue derrière : en réseau, on ne peut pas la figer.
            ui.panel({800.0f, 440.0f}, {340.0f, 130.0f});
            ui.text({800.0f, 392.0f}, "QUITTER LA COURSE ?", 34.0f, vec4(1.0f));
            if (ui.button({650.0f, 490.0f}, {130.0f, 38.0f}, "REPRENDRE") || input.back) {
                confirmQuit = false;
                ui.reset();
            } else if (ui.button({950.0f, 490.0f}, {130.0f, 38.0f}, "QUITTER", true, kMagenta)) {
                abandon();
            }
        } else if ((menuHud.vAnchor = gfx::VAnchor::Top, ui.button({1225.0f, 62.0f}, {66.0f, 26.0f}, "MENU")) ||
                   input.back) {
            confirmQuit = true;
            ui.reset();
        }
    } else {
        drawMenus(screenSize);
    }
    ui.end();
}

void App::drawShipCard(vec2 center) {
    const sim::ShipStats& stats = sim::shipStats(ship);
    ui.panel(center, {250.0f, 150.0f});
    ui.text(center + vec2(0.0f, -108.0f), stats.name, 34.0f, kWhite, Align::Center, FontId::Bold);
    ui.text(center + vec2(0.0f, -78.0f), stats.tagline, 16.0f, kDim);
    struct Row {
        const char* label;
        float value;
        float lo, hi;
    };
    const Row rows[] = {
        {"VITESSE", stats.topSpeed, 0.97f, 1.03f},
        {"REPRISE", stats.accel, 0.8f, 1.3f},
        {"MANIABILITÉ", stats.handling, 0.75f, 1.35f},
        {"ADHÉRENCE", stats.grip, 0.5f, 2.0f},
        {"SOLIDITÉ", stats.sturdy, 0.7f, 1.6f},
    };
    float y = center.y - 42.0f;
    for (const Row& row : rows) {
        ui.text({center.x - 225.0f, y + 6.0f}, row.label, 15.0f, kDim, Align::Left);
        float t = std::clamp((row.value - row.lo) / (row.hi - row.lo), 0.05f, 1.0f);
        const int cells = 10;
        for (int i = 0; i < cells; i++) {
            float x = center.x - 40.0f + 26.0f * static_cast<float>(i);
            bool on = static_cast<float>(i) < t * cells;
            menuHud.quad({x + 5.0f, y - 8.0f}, {x + 25.0f, y - 8.0f}, {x + 20.0f, y + 8.0f},
                         {x, y + 8.0f}, on ? kCyan : vec4(0.15f, 0.3f, 0.5f, 0.35f));
        }
        y += 34.0f;
    }
}

void App::drawMenus(glm::vec2) {
    char buf[96];
    const vec2 big{230.0f, 40.0f};

    if (screen == Screen::Main) {
        ui.title({800.0f, 250.0f}, "ANTIGRAV", 150.0f);
        ui.text({800.0f, 300.0f}, "COURSE ANTIGRAVITÉ", 24.0f, kCyan);
        if (ui.button({800.0f, 440.0f}, big, "SOLO")) go(Screen::Solo);
        if (ui.button({800.0f, 540.0f}, big, "MULTIJOUEUR", true, kMagenta)) {
            notice = "";
            net.client(playerName, ship);
            lastDiscover = -10.0;
            go(Screen::Multi);
        }
        if (ui.button({800.0f, 640.0f}, big, "QUITTER", true, vec4(0.7f, 0.75f, 0.9f, 1.0f)))
            quit = true;
        if (ui.in.back) quit = true;
        return;
    }

    // Réglages communs au solo et au salon de l'hôte.
    auto raceSettings = [&](float x, float y, bool hostControls) {
        int d = 0;
        if (hostControls) {
            const int choices = songChoices();
            // Un nom de fichier long est coupé pour tenir entre les flèches.
            char label[24];
            const char* full = songLabel(songIndex);
            if (std::strlen(full) > 15) std::snprintf(label, sizeof(label), "%.13s...", full);
            else std::snprintf(label, sizeof(label), "%s", full);
            if ((d = ui.selector({x, y}, 330.0f, "MORCEAU", label)) != 0) {
                selectSong((songIndex + d + choices) % choices);
            }
            y += 72.0f;
        }
        if ((d = ui.selector({x, y}, 330.0f, "VAISSEAU", sim::shipStats(ship).name)) != 0) {
            ship = (ship + d + sim::kShipCount) % sim::kShipCount;
            if (net.role != net::Role::Off) net.setShip(ship);
            startAttract();
        }
        y += 72.0f;
        if (!hostControls) return y;
        std::snprintf(buf, sizeof(buf), opponents == 0 ? "Aucune" : "%d", opponents);
        if ((d = ui.selector({x, y}, 330.0f, "IA", buf)) != 0)
            opponents = (opponents + d + 8) % 8;
        y += 72.0f;
        if ((d = ui.selector({x, y}, 330.0f, "CARACTÈRES", kPersonalityLabels[personality])) != 0)
            personality = (personality + d + 5) % 5;
        return y + 72.0f;
    };

    if (screen == Screen::Solo) {
        ui.title({800.0f, 130.0f}, "PARTIE SOLO", 64.0f);
        ui.text({800.0f, 172.0f}, sim::trackInfo(track).tagline, 20.0f, kDim);
        float y = raceSettings(480.0f, 260.0f, true);
        drawShipCard({1180.0f, 372.0f});
        const bool openEntry = songIndex == songChoices() - 1;
        if (ui.button({480.0f, y + 30.0f}, {330.0f, 40.0f}, openEntry ? "CHOISIR UN FICHIER" : "LANCER LA COURSE", true, kYellow)) {
            if (openEntry) openSongDialog();
            else startSolo();
        }
        if (notice[0]) ui.text({480.0f, y + 78.0f}, notice, 18.0f, kYellow);
        if (ui.button({480.0f, y + 124.0f}, {330.0f, 32.0f}, "RETOUR", true,
                      vec4(0.7f, 0.75f, 0.9f, 1.0f)) ||
            ui.in.back)
            go(Screen::Main);
        return;
    }

    if (screen == Screen::Multi) {
        ui.title({800.0f, 130.0f}, "MULTIJOUEUR", 64.0f);
        ui.text({800.0f, 172.0f}, "Tous les joueurs doivent être sur le même réseau Wi-Fi", 19.0f,
                kDim);
        if (ui.button({480.0f, 270.0f}, {330.0f, 40.0f}, "HÉBERGER UNE PARTIE", true, kYellow)) {
            if (net.host(playerName, ship)) {
                acquireMulticastLock();
                publishSong();
                notice = "";
                go(Screen::Lobby);
                return;
            }
            notice = "Port réseau indisponible : une partie est déjà hébergée ici";
            net.client(playerName, ship);
        }
        if (ui.button({480.0f, 366.0f}, {330.0f, 40.0f}, "SAISIR UNE ADRESSE")) go(Screen::Address);
        if (ui.button({480.0f, 462.0f}, {330.0f, 32.0f}, "RETOUR", true,
                      vec4(0.7f, 0.75f, 0.9f, 1.0f)) ||
            ui.in.back) {
            net.close();
            go(Screen::Main);
            return;
        }
        ui.text({480.0f, 540.0f}, notice, 18.0f, kYellow);

        ui.panel({1180.0f, 420.0f}, {290.0f, 200.0f});
        ui.text({1180.0f, 262.0f}, "PARTIES TROUVÉES", 22.0f, kCyan, Align::Center, FontId::Bold);
        int shown = 0;
        for (int i = 0; i < net.foundCount && shown < 4; i++) {
            const net::FoundHost& h = net.found[i];
            if (now - h.seen > 4.0) continue;
            std::snprintf(buf, sizeof(buf), "%s  %d/%d%s", h.name, h.players, sim::kMaxVehicles,
                          h.racing ? "  en course" : "");
            if (ui.button({1180.0f, 320.0f + 78.0f * static_cast<float>(shown)}, {262.0f, 30.0f},
                          buf, !h.racing, kMagenta)) {
                net.join(h.ip, h.port);
                notice = "";
                go(Screen::Lobby);
                return;
            }
            shown++;
        }
        if (shown == 0) ui.text({1180.0f, 430.0f}, "Recherche en cours...", 20.0f, kDim);
        return;
    }

    if (screen == Screen::Address) {
        ui.title({800.0f, 130.0f}, "ADRESSE DE L'HÔTE", 56.0f);
        // Saisie au clavier physique, en plus du pavé à l'écran.
        size_t len = std::strlen(address);
        for (int i = 0; i < ui.in.typedCount && len + 1 < sizeof(address); i++) {
            address[len++] = ui.in.typed[i];
            address[len] = '\0';
        }
        if (ui.in.backspace && len > 0) address[--len] = '\0';

        ui.panel({800.0f, 232.0f}, {300.0f, 38.0f});
        ui.text({800.0f, 246.0f}, address[0] ? address : "192.168.1.10", 34.0f,
                address[0] ? kWhite : vec4(1.0f, 1.0f, 1.0f, 0.25f), Align::Center, FontId::Bold);
        const char* keys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "<"};
        for (int i = 0; i < 12; i++) {
            vec2 c{680.0f + 120.0f * static_cast<float>(i % 3),
                   340.0f + 84.0f * static_cast<float>(i / 3)};
            if (!ui.button(c, {52.0f, 34.0f}, keys[i])) continue;
            len = std::strlen(address);
            if (keys[i][0] == '<') {
                if (len > 0) address[len - 1] = '\0';
            } else if (len + 1 < sizeof(address)) {
                address[len] = keys[i][0];
                address[len + 1] = '\0';
            }
        }
        uint32_t ip = net::parseIp(address);
        if (ui.button({800.0f, 700.0f}, {230.0f, 38.0f}, "REJOINDRE", ip != 0, kYellow)) {
            net.join(ip, net::kPort);
            go(Screen::Lobby);
            return;
        }
        if (ui.button({800.0f, 790.0f}, {230.0f, 30.0f}, "RETOUR", true,
                      vec4(0.7f, 0.75f, 0.9f, 1.0f)) ||
            ui.in.back)
            go(Screen::Multi);
        return;
    }

    if (screen == Screen::Lobby) {
        bool hosting = net.role == net::Role::Host;
        ui.title({800.0f, 120.0f}, "SALON", 64.0f);
        const sim::RaceConfig& lobby = net.lobby;
        if (hosting)
            ui.text({800.0f, 162.0f}, "Les autres joueurs vous voient dans Multijoueur", 19.0f, kDim);
        else if (!net.joined)
            ui.text({800.0f, 162.0f}, "Connexion à l'hôte...", 19.0f, kYellow);
        else
            ui.text({800.0f, 162.0f}, net.hostInLobby ? "En attente du lancement par l'hôte"
                                                      : "Course en cours, vous jouerez la suivante",
                    19.0f, kDim);

        float y = raceSettings(480.0f, 250.0f, hosting);
        if (hosting) {
            // On ne part que lorsque tout le monde a reçu le morceau.
            const bool openEntry = songIndex == songChoices() - 1;
            const int peers = net.peerCount(), ready = net.peersReady();
            const bool everyone = publishedSong != 0 && ready == peers;
            if (ui.button({480.0f, y + 26.0f}, {330.0f, 40.0f}, openEntry ? "CHOISIR UN FICHIER" : "LANCER LA COURSE", true, kYellow)) {
                if (openEntry) openSongDialog();
                else if (everyone) {
                    startHosted();
                    return;
                }
            }
            if (notice[0]) {
                ui.text({480.0f, y + 74.0f}, notice, 18.0f, kYellow);
            } else if (!everyone && peers > 0) {
                std::snprintf(buf, sizeof(buf), "Envoi du morceau : %d joueur%s sur %d prêt%s", ready, ready > 1 ? "s" : "", peers,
                              ready > 1 ? "s" : "");
                ui.text({480.0f, y + 74.0f}, buf, 18.0f, kYellow);
            }
            y += 92.0f;
        } else if (songStatus[0]) {
            ui.text({480.0f, y + 4.0f}, songStatus, 18.0f, kYellow);
            y += 30.0f;
        } else if (notice[0]) {
            ui.text({480.0f, y + 4.0f}, notice, 18.0f, kYellow);
            y += 30.0f;
        }
        if (ui.button({480.0f, y + 20.0f}, {330.0f, 32.0f}, "QUITTER LE SALON", true,
                      vec4(0.7f, 0.75f, 0.9f, 1.0f)) ||
            ui.in.back) {
            leaveNetwork();
            net.client(playerName, ship);
            go(Screen::Multi);
            return;
        }

        ui.panel({1180.0f, 372.0f}, {290.0f, 182.0f});
        std::snprintf(buf, sizeof(buf), "JOUEURS  %d", lobby.racerCount);
        ui.text({1180.0f, 232.0f}, buf, 22.0f, kCyan, Align::Center, FontId::Bold);
        for (int i = 0; i < lobby.racerCount; i++) {
            float rowY = 280.0f + 34.0f * static_cast<float>(i);
            bool me = hosting ? i == 0 : i == net.mySlot;
            ui.text({920.0f, rowY}, lobby.racers[i].name, 21.0f, me ? kYellow : kWhite, Align::Left,
                    FontId::Bold);
            ui.text({1440.0f, rowY}, sim::shipStats(lobby.racers[i].ship).name, 18.0f, kDim,
                    Align::Right);
        }
        if (hosting && opponents > 0) {
            std::snprintf(buf, sizeof(buf), "+ %d IA", std::min(opponents,
                          sim::kMaxVehicles - lobby.racerCount));
            ui.text({1180.0f, 280.0f + 34.0f * static_cast<float>(lobby.racerCount) + 4.0f}, buf,
                    19.0f, kMagenta);
        }
    }
}

void App::fill(const gfx::FontAtlas& fonts, glm::vec2 screenSize, gfx::FrameParams& out) {
    race.fill(fonts, screenSize, out);
    if (screen == Screen::Race) {
        // Le bandeau de fin s'ajoute à l'interface de course.
        if (out.hud && !menuHud.verts.empty()) {
            race.hud.verts.insert(race.hud.verts.end(), menuHud.verts.begin(), menuHud.verts.end());
        }
    } else {
        out.hud = &menuHud;
    }
}

} // namespace game
