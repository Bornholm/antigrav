#include "game/tutorial.h"

#include "game/hud.h"
#include "game/i18n.h"
#include "game/input.h"

#include <algorithm>
#include <cstdio>

namespace game {

TutorialPage tutorialPage(int index) {
    TutorialPage p;
    const bool touch = touchInterface();
    int n = 0;
    auto line = [&](const char* text) {
        if (n < TutorialPage::kLines) std::snprintf(p.lines[n++], sizeof(p.lines[0]), "%s", tr(text));
    };
    // Ligne qui nomme une touche du joueur.
    auto keyLine = [&](const char* format, Button a) {
        if (n < TutorialPage::kLines) std::snprintf(p.lines[n++], sizeof(p.lines[0]), tr(format), keyBindingName(a));
    };
    switch (std::clamp(index, 0, kTutorialPages - 1)) {
    case 0:
        p.title = "LA MUSIQUE EST LA PISTE";
        p.hud = kHudSongBar | kHudMinimap;
        line("Chaque morceau devient une piste, de sa première note à sa dernière.");
        line("Virages, montées et obstacles suivent son rythme et son intensité.");
        line("En haut, la barre montre où chacun en est dans le morceau.");
        break;
    case 1:
        p.title = "VOS MORCEAUX";
        p.hud = kHudSongBar;
        line("Dans Solo, la ligne MORCEAU ouvre la liste des morceaux.");
        line("CHOISIR UN FICHIER y prend un morceau de votre appareil : MP3, Ogg, FLAC ou WAV.");
        line("Le jeu en tire une piste et garde vos cinq derniers morceaux.");
        break;
    case 2:
        p.title = "PILOTER";
        p.hud = kHudButtons | kHudSpeed;
        p.loop = 8.0f;
        if (touch) {
            line("Touchez les flèches en bas de l'écran pour tourner.");
        } else if (n < TutorialPage::kLines) {
            std::snprintf(p.lines[n++], sizeof(p.lines[0]), tr("Tournez avec les flèches, ou %s et %s."),
                          keyBindingName(Button::Left), keyBindingName(Button::Right));
        }
        line("Le vaisseau accélère tout seul : vous ne gérez que la trajectoire.");
        line("Les murets des bords vous freinent.");
        break;
    case 3:
        p.title = "DÉPART TURBO";
        p.scene = TutorialScene::Countdown;
        p.hud = kHudButtons | kHudCountdown;
        p.loop = 2.5f;
        line(touch ? "Pendant le compte à rebours, un anneau se resserre sur le bouton du bonus."
                   : "Pendant le compte à rebours, un anneau se resserre sur le cadre en bas à droite.");
        if (touch) line("Touchez le bouton quand il s'allume : vous partez en turbo.");
        else keyLine("Appuyez sur %s quand il s'allume : vous partez en turbo.", Button::Use);
        line("Trop tôt, c'est raté pour cette course.");
        break;
    case 4:
        p.title = "BONUS";
        p.scene = TutorialScene::ItemBox;
        p.hud = kHudButtons;
        p.slow = 0.35f;
        p.loop = 3.5f;
        line("Passez sur les losanges jaunes pour ramasser un bonus.");
        line(touch ? "Vous n'en portez qu'un à la fois, son nom s'affiche au-dessus du bouton."
                   : "Vous n'en portez qu'un à la fois, son nom s'affiche en bas à droite.");
        if (touch) line("Touchez le bouton pour le déclencher.");
        else keyLine("Appuyez sur %s pour le déclencher.", Button::Use);
        break;
    case 5:
        p.title = "LES BONUS";
        p.items = true;
        line("Les boîtes donnent un bonus au hasard.");
        break;
    case 6:
        p.title = "BARRIÈRES ET TREMPLINS";
        p.scene = TutorialScene::Barrier;
        p.slow = 0.35f;
        p.loop = 3.5f;
        line("Les barrières cassent votre élan : passez par l'ouverture.");
        line("Seul le centre du vaisseau compte, une aile peut passer au travers.");
        line("Sur un tremplin, vous survolez barrières et mines.");
        break;
    case 7:
        p.title = "LE RYTHME";
        p.hud = kHudTimer | kHudStandings;
        line("ÉCART : votre avance sur la musique, en secondes.");
        line("Trop d'avance vous freine, du retard vous aspire.");
        line("Rester juste devant la musique rapporte le plus de points de TEMPO.");
        break;
    case 8:
        p.title = "FANTÔME";
        p.hud = kHudTimer | kHudStandings;
        p.ghost = true;
        p.loop = 10.0f;
        line("En solo, votre meilleur temps sur chaque morceau devient un fantôme.");
        line("Il rejoue votre trajectoire, sans toucher personne.");
        line("Sous le chrono, l'écart avec lui : vert en avance, rose en retard.");
        break;
    default:
        p.title = "SCORE";
        p.scoreTable = true;
        line("À l'arrivée, le score compte la place, le tempo, le pilotage et les bonus.");
        line("Chaque choc et chaque frottement coûte des points de pilotage.");
        line("La note S ne s'obtient qu'en difficulté Difficile.");
        break;
    }
    return p;
}

const char* itemDescription(int item) {
    static const char* const kDescriptions[] = {
        "",
        "File sur le vaisseau de devant",
        "Posée derrière vous, elle attend le suivant",
        "Frappe les vaisseaux tout proches",
        "Une pointe de vitesse",
        "Encaisse le prochain choc, barrière comprise",
        "Réservée au dernier, frappe tous ceux qu'elle dépasse",
    };
    return kDescriptions[std::clamp(item, 0, 6)];
}

} // namespace game
