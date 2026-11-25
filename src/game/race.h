#pragma once

#include "game/hud.h"
#include "game/input.h"
#include "game/synth.h"
#include "gfx/renderer.h"
#include "sim/session.h"

namespace game {

using sim::kHuman;
using sim::RaceConfig;
using sim::RacerSetup;

// Une course à l'écran : la session de simulation, plus la caméra de
// poursuite, le son et l'interface.
struct Race : sim::Session {
    float accumulator = 0.0f;
    // Client réseau : chaque pas passe par le code réseau, qui prédit et corrige.
    void (*clientTick)(void* user, Race& race, const sim::Input& local) = nullptr;
    void* clientUser = nullptr;

    std::vector<glm::vec2> minimap;
    gfx::HudBatch hud;
    Controls controls;
    // Synthétiseur à tenir informé de l'état de la course. Peut rester nul.
    Synth* synth = nullptr;
    // Texte de mesures à afficher en bas de l'écran, fourni par la boucle principale.
    const char* statsLine = "";
    // Effets visuels des bonus et des chocs : des orbes lumineux qui enflent
    // et s'éteignent, accrochés à un vaisseau ou posés sur la piste.
    struct Fx {
        int follow = -1;          // vaisseau suivi, ou -1 pour un point de la piste
        float dist = 0.0f, lateral = 0.0f, height = 0.0f;
        float age = 0.0f, life = 0.0f;
        glm::vec3 color{1.0f};
        float from = 0.0f, to = 0.0f; // rayon au début et à la fin, en mètres
        float gain = 1.0f;
    };
    static constexpr int kMaxFx = 24;
    Fx fx[kMaxFx];
    void spawnFx(const Fx& effect);
    // Bruits et effets déclenchés par ce que le dernier pas a changé.
    void react(const sim::World& before);
    // Voile de couleur sur tout l'écran, quand le joueur ramasse ou encaisse.
    glm::vec3 flashColor{0.0f};
    float flash = 0.0f;
    float hitShake = 0.0f;
    float beatKick = 0.0f; // secousse de la caméra sur le temps, selon la hargne du morceau
    float bands[8] = {}; // niveaux de la musique, lissés pour l'affichage
    bool keepSong = false; // le morceau vient de l'hôte : ne pas le réanalyser
    int theme = 0;      // décor de la piste
    int songSerial = 0; // change quand la piste change : le rendu la reconstruit alors
    bool pinToSync = false; // tient le vaisseau local sur le point de synchronisation
    bool showHud = true;

    // Caméra lissée.
    float camLateral = 0.0f;
    float camFov = 1.12f;
    float camShake = 0.0f;
    // Écarts de correction réseau, résorbés en douceur à l'affichage.
    float smoothDist[sim::kMaxVehicles] = {};
    float smoothLateral[sim::kMaxVehicles] = {};

    void init(const RaceConfig& config, int localIndex);
    // Piste courte et sans musique, prête en un instant : le fond de l'écran
    // de chargement au lancement du jeu.
    void initBlank();
    // Avance la simulation du temps écoulé depuis la dernière image.
    void update(float dt, const Controls& input);
    // Caméra, son et lissages, à appeler une fois par image.
    void updatePresentation(float dt);
    // screen : taille de l'écran vue par le jeu, en pixels.
    void fill(const gfx::FontAtlas& fonts, glm::vec2 screen, gfx::FrameParams& out);

    bool finished() const { return world.vehicles[localIndex].finished; }
    // Caméra en orbite autour du vaisseau local, pour les menus. Angle en radians.
    bool orbit = false;
    float orbitAngle = 0.0f;
    // Décale le vaisseau dans l'image : x vers la droite, y vers le bas, en mètres.
    glm::vec2 orbitShift{0.0f};
};

// Course d'exemple : un joueur et trois IA de personnalités différentes.
RaceConfig defaultConfig(int track, bool gridStart);

} // namespace game
