#include "game/app.h"

#include "core/log.h"
#include "game/i18n.h"

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

void App::later(std::function<void()> work) {
    if (!deferLoads) {
        work();
        return;
    }
    pending = std::move(work);
    pendingFrames = 0;
}

// Passe au morceau d'indice donné dans le sélecteur. Le libellé change tout
// de suite, le chargement se fait derrière l'écran de chargement.
void App::selectSong(int index) {
    songIndex = index;
    notice = "";
    if (index >= songChoices() - 1) return; // l'entrée « ouvrir » ne charge rien
    later([this, index] { selectSongNow(index); });
}

// Un morceau du joueur est décodé et analysé ici.
void App::selectSongNow(int index) {
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
    // Les filtres doivent rester valides jusqu'au retour du sélecteur.
    static SDL_DialogFileFilter filters[2];
    filters[0] = {tr("Musique"), "mp3;ogg;flac;wav"};
    filters[1] = {tr("Tous les fichiers"), "*"};
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
    if (path.empty()) return; // annulé
    if (screen == Screen::Songs) go(songReturn);
    later([this, path] { importSong(path); });
}

bool App::importSong(const std::string& path) {
    const int entry = songs.import(path);
    if (entry < 0) {
        notice = songs.error.c_str();
        return false;
    }
    selectSongNow(sim::kTrackCount + entry);
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
    if (inRace || pending) return;
    if (net.share.take(net.hostSongId)) {
        // Le décodage du morceau reçu se fait derrière l'écran de chargement.
        later([this] { installHostSong(); });
        return;
    }
    if (!net.share.fetching() || net.share.wantedId != net.hostSongId)
        net.share.fetch(net.hostIp, ntohs(net.hostPort), net.hostSongId);
    std::snprintf(songStatus, sizeof(songStatus), tr("Réception du morceau : %d %%"),
                  static_cast<int>(net.share.progress() * 100.0f));
}

void App::installHostSong() {
    {
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
    }
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
    settings.load();
    records.load();
    setLang(settings.lang != Lang::Count ? settings.lang : systemLang());
    setButtonsSwapped(settings.swapButtons);
    if (settings.name[0]) {
        std::snprintf(playerName, sizeof(playerName), "%s", settings.name);
    } else {
        // Pseudo par défaut, gardé ensuite : le joueur reste reconnaissable.
        std::snprintf(playerName, sizeof(playerName), "%s %02d", tr("Pilote"),
                      static_cast<int>(SDL_GetTicksNS() / 1000 % 90) + 10);
        std::snprintf(settings.name, sizeof(settings.name), "%s", playerName);
        settings.save();
    }
    if (deferLoads) {
        // Une image s'affiche tout de suite, le vrai chargement suit.
        race.initBlank();
        later([this] { startAttract(); });
    } else {
        startAttract();
    }
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
    base.difficulty = settings.difficulty;
    (void)humans;
    return base;
}

namespace {

// Nom de fichier d'un chemin : la clé d'un morceau du joueur.
std::string baseName(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace

std::string App::songKey(int index) const {
    if (index < sim::kTrackCount) return "inclus:" + std::to_string(index);
    const int entry = index - sim::kTrackCount;
    if (entry < static_cast<int>(songs.entries.size())) return baseName(songs.entries[entry].path);
    return "";
}

std::string App::raceSongKey() const {
    if (track != sim::kCustomTrack) return "inclus:" + std::to_string(track);
    return baseName(songs.loadedPath);
}

void App::startQuickRace(const RaceConfig& config, bool autopilot) {
    track = config.track;
    scoreSaved = false;
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
    // Le morceau lancé passe en tête des derniers joués.
    if (track == sim::kCustomTrack && !songs.loadedPath.empty()) {
        const int entry = songs.markPlayed(songs.loadedPath);
        if (entry >= 0) songIndex = sim::kTrackCount + entry;
    }
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
    if (track == sim::kCustomTrack && !songs.loadedPath.empty()) {
        const int entry = songs.markPlayed(songs.loadedPath);
        if (entry >= 0) songIndex = sim::kTrackCount + entry;
    }
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
    race.lightDecor = settings.quality == 0;
    // Travail en attente : le message de chargement a eu une image pour
    // s'afficher, on peut bloquer.
    if (pending && pendingFrames++ >= 1) {
        std::function<void()> work = std::move(pending);
        pending = nullptr;
        work();
    }
    const bool busy = static_cast<bool>(pending);
    static const UiInput quiet{};
    pollSongDialog();
    race.statsLine = statsLine;
    // Thème dans les menus. En course, le morceau du circuit, calé sur le
    // temps de course : il démarre au signal du départ.
    if (music) {
        // À l'arrivée, le morceau s'efface devant la musique de fin.
        music->setDuck(screen == Screen::Race && race.finished() ? 0.0f : 1.0f);
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
        } else if ((screen == Screen::Solo || screen == Screen::Lobby || screen == Screen::Songs) && !busy && race.songSerial != -2) {
            // Dans les réglages de partie, on entend le morceau choisi : il
            // suit la démonstration qui tourne derrière le menu.
            music->sync(Music::kFirstTrack + race.config.track, race.world.raceTime + race.accumulator, true);
        } else {
            music->play(Music::kTheme);
        }
    }
    // La démonstration derrière les menus repart quand son morceau est fini.
    if (screen != Screen::Race && !inRace && race.finished() && !busy) startAttract();

    // Réseau : lit les paquets, puis réagit aux changements d'état.
    net.poll(now, inRace ? &race : nullptr);
    if (net.role == net::Role::Client) {
        receiveSong();
        // Le départ attend que le morceau de l'hôte soit installé.
        if (net.startPending && (net.hostSongId == 0 || net.hostSongId == appliedSong) && !pending) {
            net.startPending = false;
            // La piste se construit derrière l'écran de chargement.
            later([this] {
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
            });
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
    ui.begin(menuHud, busy ? quiet : input, static_cast<float>(now));
    if (screen == Screen::Race) {
        bool over = race.finished();
        // Le score d'une course en solo va aux records du morceau.
        if (race.scored && !scoreSaved) {
            scoreSaved = true;
            if (net.role == net::Role::Off && !race.autopilot) {
                const std::string key = raceSongKey();
                race.bestScore = records.get(key);
                race.newRecord = records.submit(key, race.score.total);
            }
        }
        auto abandon = [&] {
            if (net.role != net::Role::Off) leaveNetwork();
            else startAttract();
            go(Screen::Main);
        };
        if (over) {
            if (ui.button({800.0f, 606.0f}, {190.0f, 38.0f}, "CONTINUER", true, kYellow)) {
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
    if (pending) {
        menuHud.anchor = gfx::Anchor::Center;
        menuHud.vAnchor = gfx::VAnchor::Middle;
        ui.panel({800.0f, 450.0f}, {300.0f, 62.0f});
        ui.text({800.0f, 463.0f}, "CHARGEMENT...", 34.0f, kWhite, Align::Center, FontId::Bold);
    }
    ui.end();
}

void App::drawShipCard(vec2 center) {
    const sim::ShipStats& stats = sim::shipStats(ship);
    ui.panel(center, {250.0f, 184.0f});
    ui.text(center + vec2(0.0f, -128.0f), stats.name, 40.0f, kWhite, Align::Center, FontId::Bold);
    ui.text(center + vec2(0.0f, -92.0f), stats.tagline, 19.0f, kDim);
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
    float y = center.y - 44.0f;
    for (const Row& row : rows) {
        ui.text({center.x - 228.0f, y + 7.0f}, row.label, 20.0f, kDim, Align::Left);
        float t = std::clamp((row.value - row.lo) / (row.hi - row.lo), 0.05f, 1.0f);
        const int cells = 10;
        for (int i = 0; i < cells; i++) {
            float x = center.x - 34.0f + 26.0f * static_cast<float>(i);
            bool on = static_cast<float>(i) < t * cells;
            menuHud.quad({x + 5.0f, y - 10.0f}, {x + 25.0f, y - 10.0f}, {x + 20.0f, y + 10.0f},
                         {x, y + 10.0f}, on ? kCyan : vec4(0.15f, 0.3f, 0.5f, 0.35f));
        }
        y += 42.0f;
    }
}

namespace {

// Coupe un texte UTF-8 à un nombre de caractères, sans casser un caractère.
std::string clip(const char* text, int maxChars) {
    int chars = 0;
    const char* p = text;
    for (; *p; p++) {
        if ((static_cast<unsigned char>(*p) & 0xC0) == 0x80) continue;
        if (chars == maxChars) break;
        chars++;
    }
    if (!*p) return text;
    return std::string(text, static_cast<size_t>(p - text)) + "...";
}

const vec4 kQuiet{0.7f, 0.75f, 0.9f, 1.0f};
// Les lignes d'un menu : hauteur d'un pouce, et de quoi ne pas en toucher deux.
const float kRow = 96.0f;
const float kColumn = 360.0f; // demi-largeur de la colonne de réglages

} // namespace

void App::drawMenus(glm::vec2) {
    char buf[96];

    if (screen == Screen::Main) {
        ui.veil(0.30f);
        ui.title({800.0f, 240.0f}, "SOUNDWAVE", 150.0f);
        ui.text({800.0f, 318.0f}, "COURSE MUSICALE", 26.0f, kCyan);
        if (ui.button({800.0f, 440.0f}, {280.0f, 48.0f}, "SOLO")) go(Screen::Solo);
        if (ui.button({800.0f, 556.0f}, {280.0f, 48.0f}, "MULTIJOUEUR", true, kMagenta)) {
            notice = "";
            net.client(playerName, ship);
            lastDiscover = -10.0;
            go(Screen::Multi);
        }
        if (ui.button({655.0f, 672.0f}, {135.0f, 40.0f}, "OPTIONS", true, kQuiet)) go(Screen::Options);
        if (ui.button({945.0f, 672.0f}, {135.0f, 40.0f}, "QUITTER", true, kQuiet)) quit = true;
        if (ui.in.back) quit = true;
        return;
    }
    ui.veil(0.58f);

    auto editName = [&](Screen from) {
        std::snprintf(nameDraft, sizeof(nameDraft), "%s", playerName);
        if (window) SDL_StartTextInput(window);
        nameReturn = from;
        go(Screen::Name);
    };

    if (screen == Screen::Options) {
        ui.title({800.0f, 96.0f}, "OPTIONS", 64.0f);
        float y = touchInterface() ? 208.0f : 250.0f;
        // Premier choix : suivre la langue du système, affichée entre parenthèses.
        const bool autoLang = settings.lang == Lang::Count;
        char langLabel[48];
        std::snprintf(langLabel, sizeof(langLabel), "Auto (%s)", langName(lang()));
        if (int d = ui.selector({800.0f, y}, 400.0f, "LANGUE", autoLang ? langLabel : langName(lang())); d != 0) {
            // Ordre du sélecteur : automatique, puis chaque langue.
            const int count = static_cast<int>(Lang::Count) + 1;
            const int at = autoLang ? 0 : static_cast<int>(settings.lang) + 1;
            const int next = (at + d + count) % count;
            settings.lang = next == 0 ? Lang::Count : static_cast<Lang>(next - 1);
            setLang(next == 0 ? systemLang() : settings.lang);
            settings.save();
        }
        y += kRow;
        if (ui.link({800.0f, y}, 400.0f, "PSEUDO", playerName)) {
            editName(Screen::Options);
            return;
        }
        y += kRow;
        // Côté des boutons de direction, pour qui joue au doigt.
        if (touchInterface()) {
            if (ui.selector({800.0f, y}, 400.0f, "DIRECTION", buttonsSwapped() ? "À droite" : "À gauche") != 0) {
                setButtonsSwapped(!buttonsSwapped());
                settings.swapButtons = buttonsSwapped();
                settings.save();
            }
            y += kRow;
        }
        // Charge du rendu, pour les appareils modestes.
        static const char* const kQualities[] = {"Basse", "Moyenne", "Haute"};
        if (int d = ui.selector({800.0f, y}, 400.0f, "QUALITÉ", kQualities[std::clamp(settings.quality, 0, 2)]); d != 0) {
            settings.quality = (settings.quality + d + 3) % 3;
            settings.save();
        }
        y += kRow;
        if (ui.selector({800.0f, y}, 400.0f, "CADENCE", settings.cap60 ? "60 images/s" : "Maximale") != 0) {
            settings.cap60 = !settings.cap60;
            settings.save();
        }
        y += kRow;
        if (ui.button({800.0f, y + 14.0f}, {400.0f, 40.0f}, "RETOUR", true, kQuiet) || ui.in.back) go(Screen::Main);
        return;
    }

    // Réglages communs au solo et au salon de l'hôte.
    auto raceSettings = [&](float x, float y, bool hostControls) {
        int d = 0;
        if (hostControls) {
            if (ui.link({x, y}, kColumn, "MORCEAU", clip(songLabel(songIndex), 20).c_str())) {
                songReturn = screen;
                go(Screen::Songs);
            }
            y += kRow;
        }
        if ((d = ui.selector({x, y}, kColumn, "VAISSEAU", sim::shipStats(ship).name)) != 0) {
            ship = (ship + d + sim::kShipCount) % sim::kShipCount;
            if (net.role != net::Role::Off) net.setShip(ship);
            startAttract();
        }
        y += kRow;
        if (!hostControls) return y;
        std::snprintf(buf, sizeof(buf), opponents == 0 ? "Aucune" : "%d", opponents);
        if ((d = ui.selector({x, y}, kColumn, "IA", buf)) != 0)
            opponents = (opponents + d + 8) % 8;
        y += kRow;
        if ((d = ui.selector({x, y}, kColumn, "CARACTÈRES", kPersonalityLabels[personality])) != 0)
            personality = (personality + d + 5) % 5;
        y += kRow;
        static const char* const kDifficulties[] = {"Facile", "Normale", "Difficile"};
        if ((d = ui.selector({x, y}, kColumn, "DIFFICULTÉ", kDifficulties[std::clamp(settings.difficulty, 0, 2)])) != 0) {
            settings.difficulty = (settings.difficulty + d + 3) % 3;
            settings.save();
        }
        return y + kRow;
    };

    if (screen == Screen::Songs) {
        ui.title({800.0f, 110.0f}, "CHOISIR UN MORCEAU", 56.0f);
        const float left = 440.0f, right = 1160.0f;
        const vec2 half{330.0f, 38.0f};
        ui.text({left, 196.0f}, "INCLUS", 22.0f, kCyan, Align::Center, FontId::Bold);
        ui.text({right, 196.0f}, "RÉCENTS", 22.0f, kCyan, Align::Center, FontId::Bold);
        auto entry = [&](int index, vec2 at) {
            const bool current = index == songIndex;
            const bool chosen = ui.button(at, half, clip(songLabel(index), 24).c_str(), true, current ? kYellow : kCyan);
            // Meilleure note obtenue sur ce morceau, au bout de la ligne.
            if (const int best = records.get(songKey(index)); best > 0) {
                const char grade[2] = {gradeOf(best), '\0'};
                ui.text(at + vec2(half.x - 34.0f, 12.0f), grade, 34.0f, gradeColor(gradeOf(best)), Align::Center, FontId::Bold);
            }
            if (!chosen) return false;
            if (!current) selectSong(index);
            go(songReturn);
            return true;
        };
        for (int i = 0; i < sim::kTrackCount; i++)
            if (entry(i, {left, 262.0f + 92.0f * static_cast<float>(i)})) return;
        if (ui.button({left, 262.0f + 92.0f * sim::kTrackCount + 14.0f}, half, "CHOISIR UN FICHIER", true, kMagenta))
            openSongDialog();
        if (ui.button({left, 262.0f + 92.0f * (sim::kTrackCount + 1) + 28.0f}, {330.0f, 36.0f}, "RETOUR", true, kQuiet) ||
            ui.in.back) {
            go(songReturn);
            return;
        }
        const int mine = static_cast<int>(songs.entries.size());
        for (int i = 0; i < mine; i++)
            if (entry(sim::kTrackCount + i, {right, 262.0f + 92.0f * static_cast<float>(i)})) return;
        if (mine == 0) {
            ui.panel({right, 400.0f}, {330.0f, 176.0f});
            ui.text({right, 396.0f}, "Vos morceaux apparaîtront ici", 22.0f, kDim);
            ui.text({right, 430.0f}, "MP3, Ogg, FLAC ou WAV", 19.0f, kDim);
        }
        return;
    }

    if (screen == Screen::Solo) {
        ui.title({800.0f, 110.0f}, "PARTIE SOLO", 60.0f);
        ui.text({800.0f, 152.0f}, sim::trackInfo(track).tagline, 22.0f, kDim);
        float y = raceSettings(470.0f, 236.0f, true);
        drawShipCard({1190.0f, 380.0f});
        if (ui.button({470.0f, y + 10.0f}, {kColumn, 46.0f}, "LANCER LA COURSE", true, kYellow))
            later([this] { startSolo(); });
        if (notice[0]) ui.text({1190.0f, 620.0f}, notice, 20.0f, kYellow);
        if (ui.button({470.0f, y + 112.0f}, {kColumn, 36.0f}, "RETOUR", true, kQuiet) || ui.in.back)
            go(Screen::Main);
        return;
    }

    if (screen == Screen::Multi) {
        ui.title({800.0f, 110.0f}, "MULTIJOUEUR", 60.0f);
        ui.text({800.0f, 152.0f}, "Tous les joueurs doivent être sur le même réseau Wi-Fi", 22.0f, kDim);
        if (ui.button({470.0f, 250.0f}, {kColumn, 44.0f}, "HÉBERGER UNE PARTIE", true, kYellow)) {
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
        if (ui.button({470.0f, 250.0f + kRow}, {kColumn, 40.0f}, "SAISIR UNE ADRESSE")) go(Screen::Address);
        if (ui.link({470.0f, 250.0f + 2.0f * kRow}, kColumn, "PSEUDO", playerName)) {
            editName(Screen::Multi);
            return;
        }
        if (ui.button({470.0f, 250.0f + 3.0f * kRow + 10.0f}, {kColumn, 36.0f}, "RETOUR", true, kQuiet) || ui.in.back) {
            net.close();
            go(Screen::Main);
            return;
        }
        ui.text({470.0f, 250.0f + 4.0f * kRow}, notice, 20.0f, kYellow);

        ui.panel({1190.0f, 412.0f}, {250.0f, 216.0f});
        ui.text({1190.0f, 242.0f}, "PARTIES TROUVÉES", 24.0f, kCyan, Align::Center, FontId::Bold);
        int shown = 0;
        for (int i = 0; i < net.foundCount && shown < 4; i++) {
            const net::FoundHost& h = net.found[i];
            if (now - h.seen > 4.0) continue;
            std::snprintf(buf, sizeof(buf), "%s  %d/%d%s", h.name, h.players, sim::kMaxVehicles,
                          h.racing ? tr("  en course") : "");
            if (ui.button({1190.0f, 306.0f + 86.0f * static_cast<float>(shown)}, {228.0f, 36.0f},
                          buf, !h.racing, kMagenta)) {
                net.join(h.ip, h.port);
                notice = "";
                go(Screen::Lobby);
                return;
            }
            shown++;
        }
        if (shown == 0) ui.text({1190.0f, 430.0f}, "Recherche en cours...", 22.0f, kDim);
        return;
    }

    if (screen == Screen::Name) {
        // Tout tient dans la moitié haute : le clavier du téléphone couvre le bas.
        ui.title({800.0f, 90.0f}, "VOTRE PSEUDO", 52.0f);
        // Seize caractères au plus, parmi ceux que la police sait dessiner.
        auto length = [](const char* text) {
            int count = 0;
            for (const char* p = text; *p; p++) count += (static_cast<unsigned char>(*p) & 0xC0) != 0x80;
            return count;
        };
        size_t len = std::strlen(nameDraft);
        for (const char* p = ui.in.text; *p;) {
            const unsigned char c = static_cast<unsigned char>(*p);
            const int bytes = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
            // Latin-1 seulement : un octet, ou deux octets commençant par C2 ou C3.
            const bool drawable = (bytes == 1 && c >= 32 && c != '=') || (bytes == 2 && (c == 0xC2 || c == 0xC3) && p[1]);
            if (drawable && length(nameDraft) < 16 && len + static_cast<size_t>(bytes) < sizeof(nameDraft)) {
                std::memcpy(nameDraft + len, p, static_cast<size_t>(bytes));
                len += static_cast<size_t>(bytes);
                nameDraft[len] = '\0';
            }
            for (int i = 0; i < bytes && *p; i++) p++;
        }
        if (ui.in.backspace && len > 0) {
            // Retire le dernier caractère entier, pas seulement son dernier octet.
            do len--;
            while (len > 0 && (static_cast<unsigned char>(nameDraft[len]) & 0xC0) == 0x80);
            nameDraft[len] = '\0';
        }
        ui.panel({800.0f, 196.0f}, {340.0f, 48.0f});
        const bool blink = static_cast<int>(now * 2.0) % 2 == 0;
        std::snprintf(buf, sizeof(buf), "%s%s", nameDraft, blink ? "_" : " ");
        ui.text({800.0f, 212.0f}, buf, 42.0f, kWhite, Align::Center, FontId::Bold);
        ui.text({800.0f, 278.0f}, "16 caractères au plus", 20.0f, kDim);
        auto leave = [&] {
            if (window) SDL_StopTextInput(window);
            go(nameReturn);
        };
        // Un pseudo vide ou fait d'espaces n'est pas accepté.
        bool blank = true;
        for (const char* p = nameDraft; *p; p++) blank = blank && *p == ' ';
        if (ui.button({975.0f, 356.0f}, {165.0f, 42.0f}, "VALIDER", !blank, kYellow) || (ui.in.accept && !blank)) {
            std::snprintf(playerName, sizeof(playerName), "%s", nameDraft);
            std::snprintf(settings.name, sizeof(settings.name), "%s", playerName);
            settings.save();
            if (nameReturn == Screen::Multi) net.client(playerName, ship); // le réseau reprend le nouveau nom
            leave();
            return;
        }
        if (ui.button({625.0f, 356.0f}, {165.0f, 42.0f}, "RETOUR", true, kQuiet) || ui.in.back) leave();
        return;
    }

    if (screen == Screen::Address) {
        ui.title({800.0f, 90.0f}, "ADRESSE DE L'HÔTE", 52.0f);
        // Saisie au clavier physique, en plus du pavé à l'écran.
        size_t len = std::strlen(address);
        for (int i = 0; i < ui.in.typedCount && len + 1 < sizeof(address); i++) {
            address[len++] = ui.in.typed[i];
            address[len] = '\0';
        }
        if (ui.in.backspace && len > 0) address[--len] = '\0';

        ui.panel({800.0f, 190.0f}, {340.0f, 44.0f});
        ui.text({800.0f, 205.0f}, address[0] ? address : "192.168.1.10", 38.0f,
                address[0] ? kWhite : vec4(1.0f, 1.0f, 1.0f, 0.25f), Align::Center, FontId::Bold);
        const char* keys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "<"};
        for (int i = 0; i < 12; i++) {
            vec2 c{655.0f + 145.0f * static_cast<float>(i % 3),
                   300.0f + 92.0f * static_cast<float>(i / 3)};
            if (!ui.button(c, {64.0f, 39.0f}, keys[i])) continue;
            len = std::strlen(address);
            if (keys[i][0] == '<') {
                if (len > 0) address[len - 1] = '\0';
            } else if (len + 1 < sizeof(address)) {
                address[len] = keys[i][0];
                address[len + 1] = '\0';
            }
        }
        uint32_t ip = net::parseIp(address);
        if (ui.button({975.0f, 690.0f}, {165.0f, 44.0f}, "REJOINDRE", ip != 0, kYellow)) {
            net.join(ip, net::kPort);
            go(Screen::Lobby);
            return;
        }
        if (ui.button({625.0f, 690.0f}, {165.0f, 44.0f}, "RETOUR", true, kQuiet) || ui.in.back) go(Screen::Multi);
        return;
    }

    if (screen == Screen::Lobby) {
        bool hosting = net.role == net::Role::Host;
        ui.title({800.0f, 100.0f}, "SALON", 60.0f);
        const sim::RaceConfig& lobby = net.lobby;
        if (hosting)
            ui.text({800.0f, 142.0f}, "Les autres joueurs vous voient dans Multijoueur", 22.0f, kDim);
        else if (!net.joined)
            ui.text({800.0f, 142.0f}, "Connexion à l'hôte...", 22.0f, kYellow);
        else
            ui.text({800.0f, 142.0f}, net.hostInLobby ? "En attente du lancement par l'hôte"
                                                      : "Course en cours, vous jouerez la suivante",
                    22.0f, kDim);

        // L'état du salon s'écrit sous la liste des joueurs.
        const char* status = "";
        float y = raceSettings(470.0f, 226.0f, hosting);
        if (hosting) {
            // On ne part que lorsque tout le monde a reçu le morceau.
            const int peers = net.peerCount(), ready = net.peersReady();
            const bool everyone = publishedSong != 0 && ready == peers;
            if (ui.button({470.0f, y + 10.0f}, {kColumn, 46.0f}, "LANCER LA COURSE", everyone && !pending, kYellow)) {
                later([this] { startHosted(); });
                return;
            }
            if (notice[0]) {
                status = notice;
            } else if (!everyone && peers > 0) {
                std::snprintf(buf, sizeof(buf), tr("Envoi du morceau : %d sur %d prêts"), ready, peers);
                status = buf;
            }
            y += 112.0f;
        } else if (songStatus[0]) {
            status = songStatus;
        } else if (notice[0]) {
            status = notice;
        }
        if (ui.button({470.0f, y}, {kColumn, 36.0f}, "QUITTER LE SALON", true, kQuiet) || ui.in.back) {
            leaveNetwork();
            net.client(playerName, ship);
            go(Screen::Multi);
            return;
        }

        ui.panel({1190.0f, 370.0f}, {250.0f, 184.0f});
        char head[48];
        std::snprintf(head, sizeof(head), tr("JOUEURS  %d"), lobby.racerCount);
        ui.text({1190.0f, 226.0f}, head, 24.0f, kCyan, Align::Center, FontId::Bold);
        for (int i = 0; i < lobby.racerCount; i++) {
            float rowY = 276.0f + 38.0f * static_cast<float>(i);
            bool me = hosting ? i == 0 : i == net.mySlot;
            ui.text({964.0f, rowY}, lobby.racers[i].name, 26.0f, me ? kYellow : kWhite, Align::Left,
                    FontId::Bold);
            ui.text({1416.0f, rowY}, sim::shipStats(lobby.racers[i].ship).name, 22.0f, kDim,
                    Align::Right);
        }
        if (hosting && opponents > 0 && lobby.racerCount < sim::kMaxVehicles) {
            char more[32];
            std::snprintf(more, sizeof(more), tr("+ %d IA"), std::min(opponents,
                          sim::kMaxVehicles - lobby.racerCount));
            ui.text({1190.0f, 276.0f + 38.0f * static_cast<float>(lobby.racerCount) + 4.0f}, more,
                    23.0f, kMagenta);
        }
        if (status[0]) ui.text({1190.0f, 596.0f}, status, 20.0f, kYellow);
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
