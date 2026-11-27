#include "game/tips.h"

#include "game/i18n.h"
#include "game/input.h"

#include <cstdio>

namespace game {

TipText tipText(Tip tip, const Tips& tips) {
    TipText t;
    const bool touch = touchInterface();
    auto body = [&](const char* text) { std::snprintf(t.body, sizeof(t.body), "%s", tr(text)); };
    auto detail = [&](const char* text) { std::snprintf(t.detail, sizeof(t.detail), "%s", tr(text)); };
    // Détail des bonus : comment les déclencher, tant que le joueur ne l'a jamais fait.
    auto useDetail = [&] {
        if (tips.has(Tip::Used)) return;
        if (touch) detail("Touchez le bouton du bonus pour le déclencher");
        else std::snprintf(t.detail, sizeof(t.detail), tr("Appuyez sur %s pour le déclencher"), keyBindingName(Button::Use));
    };
    switch (tip) {
    case Tip::StartTurbo:
        t.title = tr("DÉPART TURBO");
        if (touch) body("Touchez le bouton du bonus quand l'anneau se referme");
        else std::snprintf(t.body, sizeof(t.body), tr("Appuyez sur %s quand l'anneau se referme"), keyBindingName(Button::Use));
        detail("Trop tôt, c'est raté");
        break;
    case Tip::Steer:
        t.title = tr("PILOTAGE");
        if (touch) body("Touchez les flèches pour tourner");
        else std::snprintf(t.body, sizeof(t.body), tr("Tournez avec les flèches, ou %s et %s"), keyBindingName(Button::Left),
                           keyBindingName(Button::Right));
        detail("Les murets vous freinent : restez sur la piste");
        break;
    case Tip::Ghost:
        t.title = tr("FANTÔME");
        body("Votre meilleur temps sur ce morceau. Battez-le !");
        detail("L'écart avec lui s'affiche sous le chrono");
        break;
    case Tip::Tempo:
        t.title = tr("RYTHME");
        body("La piste suit la musique : restez à son rythme");
        detail("Trop d'avance vous freine, le retard vous aspire");
        break;
    case Tip::Shot:
        t.title = tr("TIR");
        body("Il file sur le vaisseau de devant");
        useDetail();
        break;
    case Tip::Mine:
        t.title = tr("MINE");
        body("Posée derrière vous, elle attend le suivant");
        useDetail();
        break;
    case Tip::Pulse:
        t.title = tr("ONDE");
        body("Elle frappe les vaisseaux tout proches");
        useDetail();
        break;
    case Tip::Turbo:
        t.title = tr("TURBO");
        body("Une pointe de vitesse");
        useDetail();
        break;
    case Tip::Shield:
        t.title = tr("BOUCLIER");
        body("Il encaisse le prochain choc, barrière comprise");
        useDetail();
        break;
    case Tip::Comet:
        t.title = tr("COMÈTE");
        body("Réservée au dernier : elle frappe tous ceux qu'elle dépasse");
        useDetail();
        break;
    case Tip::Barrier:
        t.title = tr("BARRIÈRE");
        body("Passez à côté : elle casse votre élan");
        break;
    case Tip::Jump:
        t.title = tr("TREMPLIN");
        body("Passez dessus : en l'air, vous survolez barrières et mines");
        break;
    default: break;
    }
    return t;
}

} // namespace game
