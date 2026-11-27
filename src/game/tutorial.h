#pragma once

#include <cstdint>

namespace game {

// Tutoriel du menu principal : une suite de pages, chacune posée sur une
// scène de course jouée par le moteur, avec un texte fixe et les seuls
// éléments d'interface dont elle parle.
enum class TutorialScene : uint8_t {
    Cruise,    // course lancée, au quart du morceau
    Countdown, // compte à rebours sur la grille, le joueur peut tenter son départ turbo
    ItemBox,   // à l'approche d'une boîte à bonus, au ralenti
    Barrier,   // à l'approche d'une barrière, au ralenti
    Dodge,     // un tir arrive par derrière, le vaisseau l'esquive d'une vrille
};

struct TutorialPage {
    const char* title = "";
    static constexpr int kLines = 3;
    char lines[kLines][160] = {};
    TutorialScene scene = TutorialScene::Cruise;
    uint32_t hud = 0;        // éléments d'interface affichés, voir HudPart
    float slow = 1.0f;       // vitesse de la scène, 1 pour le temps réel
    float loop = 6.0f;       // secondes de course avant que la scène reparte
    bool items = false;      // la page présente la liste des bonus
    bool ghost = false;      // un fantôme court avec le joueur
    bool scoreTable = false; // la page détaille le barème du score
};

constexpr int kTutorialPages = 11;

// Page du tutoriel dans la langue du jeu, avec les commandes de l'appareil :
// boutons à l'écran au doigt, touches choisies au clavier.
TutorialPage tutorialPage(int index);

// Description d'un bonus pour la page qui les présente, à traduire.
const char* itemDescription(int item);

} // namespace game
