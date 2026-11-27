#include "game/i18n.h"

#include <SDL3/SDL.h>

#include <cstring>

namespace game {

namespace {

Lang current = Lang::Fr;

struct Entry {
    const char* fr;
    const char* en;
    const char* es;
};

// Les chaînes de format gardent leurs arguments dans le même ordre.
const Entry kEntries[] = {
    // Menu principal
    {"BRISEZ LE MUR DU SON", "BREAK THE SOUND BARRIER", "ROMPE LA BARRERA DEL SONIDO"},
    {"SOLO", "SOLO", "UN JUGADOR"},
    {"MULTIJOUEUR", "MULTIPLAYER", "MULTIJUGADOR"},
    {"QUITTER", "QUIT", "SALIR"},
    {"LANGUE", "LANGUAGE", "IDIOMA"},
    {"OPTIONS", "OPTIONS", "OPCIONES"},
    {"DIRECTION", "STEERING", "DIRECCIÓN"},
    {"CLAVIER", "KEYBOARD", "TECLADO"},
    {"GAUCHE", "LEFT", "IZQUIERDA"},
    {"DROITE", "RIGHT", "DERECHA"},
    {"Appuyez sur une touche", "Press a key", "Pulse una tecla"},
    {"Les flèches et la barre d'espace restent actives", "The arrow keys and the space bar always work", "Las flechas y la barra espaciadora siguen activas"},
    {"PAR DÉFAUT", "DEFAULTS", "POR DEFECTO"},
    {"NOUVELLE VERSION %s", "NEW VERSION %s", "NUEVA VERSIÓN %s"},
    {"MISES À JOUR", "UPDATES", "ACTUALIZACIONES"},
    {"Vérifier", "Check", "Buscar"},
    {"Ne pas vérifier", "Don't check", "No buscar"},
    {"À gauche", "Left", "Izquierda"},
    {"À droite", "Right", "Derecha"},
    {"QUALITÉ", "QUALITY", "CALIDAD"},
    {"Basse", "Low", "Baja"},
    {"Moyenne", "Medium", "Media"},
    {"Haute", "High", "Alta"},
    {"CADENCE", "FRAME RATE", "FOTOGRAMAS"},
    {"60 images/s", "60 fps", "60 fps"},
    {"Maximale", "Maximum", "Máxima"},
    {"CHOISIR UN MORCEAU", "CHOOSE A SONG", "ELEGIR UNA CANCIÓN"},
    {"INCLUS", "INCLUDED", "INCLUIDAS"},
    {"RÉCENTS", "RECENT", "RECIENTES"},
    {"Vos morceaux apparaîtront ici", "Your songs will appear here", "Tus canciones aparecerán aquí"},
    {"MP3, Ogg, FLAC ou WAV", "MP3, Ogg, FLAC or WAV", "MP3, Ogg, FLAC o WAV"},
    // Réglages de partie
    {"PARTIE SOLO", "SOLO RACE", "CARRERA EN SOLITARIO"},
    {"MORCEAU", "TRACK", "TEMA"},
    {"VAISSEAU", "SHIP", "NAVE"},
    {"IA", "AI", "IA"},
    {"Aucune", "None", "Ninguna"},
    {"CARACTÈRES", "PERSONALITIES", "CARACTERES"},
    {"Variées", "Mixed", "Variados"},
    {"Agressif", "Aggressive", "Agresivo"},
    {"Défensif", "Defensive", "Defensivo"},
    {"Pilote", "Racer", "Piloto"},
    {"Imprévisible", "Erratic", "Imprevisible"},
    {"LANCER LA COURSE", "START RACE", "INICIAR CARRERA"},
    {"CHOISIR UN FICHIER", "CHOOSE A FILE", "ELEGIR UN ARCHIVO"},
    {"Ouvrir un fichier...", "Open a file...", "Abrir un archivo..."},
    {"RETOUR", "BACK", "VOLVER"},
    {"CHARGEMENT...", "LOADING...", "CARGANDO..."},
    {"Musique", "Music", "Música"},
    {"Tous les fichiers", "All files", "Todos los archivos"},
    // Vaisseaux
    {"Flèche", "Arrow", "Flecha"},
    {"Aiguille", "Needle", "Aguja"},
    {"Jumeau", "Twin", "Gemelo"},
    {"Équilibrée, sans point faible", "Balanced, no weak spot", "Equilibrada, sin punto débil"},
    {"Agile, tient les virages", "Agile, holds the corners", "Ágil, se agarra en las curvas"},
    {"Pointe de vitesse, dérive fort", "Top speed, drifts wide", "Velocidad punta, derrapa mucho"},
    {"Lourd, repart vite, encaisse", "Heavy, quick off the mark, tough", "Pesada, arranca rápido, aguanta"},
    {"VITESSE", "SPEED", "VELOCIDAD"},
    {"REPRISE", "PICKUP", "ACELERACIÓN"},
    {"MANIABILITÉ", "HANDLING", "MANEJO"},
    {"ADHÉRENCE", "GRIP", "AGARRE"},
    {"SOLIDITÉ", "STURDINESS", "RESISTENCIA"},
    // Morceaux livrés
    {"Canyon Solaire", "Solar Canyon", "Cañón Solar"},
    {"Station Orbitale", "Orbital Station", "Estación Orbital"},
    {"Big beat, basse acide", "Big beat, acid bass", "Big beat, bajo ácido"},
    {"Drum and bass froide", "Cold drum and bass", "Drum and bass frío"},
    {"Morceau du joueur", "Player's track", "Tema del jugador"},
    {"%.0f BPM, %d:%02d", "%.0f BPM, %d:%02d", "%.0f BPM, %d:%02d"},
    {"Sans tempo net, %d:%02d", "No clear tempo, %d:%02d", "Sin tempo claro, %d:%02d"},
    // Multijoueur
    {"Tous les joueurs doivent être sur le même réseau Wi-Fi", "All players must be on the same Wi-Fi network",
     "Todos los jugadores deben estar en la misma red Wi-Fi"},
    {"HÉBERGER UNE PARTIE", "HOST A GAME", "CREAR UNA PARTIDA"},
    {"SAISIR UNE ADRESSE", "ENTER AN ADDRESS", "INTRODUCIR UNA DIRECCIÓN"},
    {"PSEUDO", "NICKNAME", "APODO"},
    {"VOTRE PSEUDO", "YOUR NICKNAME", "TU APODO"},
    {"VALIDER", "CONFIRM", "ACEPTAR"},
    {"16 caractères au plus", "16 characters at most", "16 caracteres como máximo"},
    {"PARTIES TROUVÉES", "GAMES FOUND", "PARTIDAS ENCONTRADAS"},
    {"Recherche en cours...", "Searching...", "Buscando..."},
    {"%s  %d/%d%s", "%s  %d/%d%s", "%s  %d/%d%s"},
    {"  en course", "  racing", "  en carrera"},
    {"Port réseau indisponible : une partie est déjà hébergée ici", "Network port unavailable: a game is already hosted here",
     "Puerto de red no disponible: ya hay una partida creada aquí"},
    {"ADRESSE DE L'HÔTE", "HOST ADDRESS", "DIRECCIÓN DEL ANFITRIÓN"},
    {"REJOINDRE", "JOIN", "UNIRSE"},
    {"SALON", "LOBBY", "SALA"},
    {"Les autres joueurs vous voient dans Multijoueur", "Other players can see you in Multiplayer",
     "Los demás jugadores te ven en Multijugador"},
    {"Connexion à l'hôte...", "Connecting to the host...", "Conectando con el anfitrión..."},
    {"En attente du lancement par l'hôte", "Waiting for the host to start", "Esperando a que el anfitrión inicie"},
    {"Course en cours, vous jouerez la suivante", "Race in progress, you will join the next one",
     "Carrera en curso, jugarás la siguiente"},
    {"QUITTER LE SALON", "LEAVE LOBBY", "SALIR DE LA SALA"},
    {"JOUEURS  %d", "PLAYERS  %d", "JUGADORES  %d"},
    {"+ %d IA", "+ %d AI", "+ %d IA"},
    {"Connexion à l'hôte perdue", "Connection to the host lost", "Conexión con el anfitrión perdida"},
    {"Réception du morceau : %d %%", "Receiving the track: %d %%", "Recibiendo el tema: %d %%"},
    {"Envoi du morceau : %d sur %d prêts", "Sending the track: %d of %d ready", "Enviando el tema: %d de %d listos"},
    {"Morceau reçu abîmé", "Received track is damaged", "El tema recibido está dañado"},
    // Course
    {"POS", "POS", "POS"},
    {"FIN", "END", "FIN"},
    {"TEMPS", "TIME", "TIEMPO"},
    {"ÉCART", "GAP", "DIF."},
    {"TIR", "SHOT", "DISPARO"},
    {"MINE", "MINE", "MINA"},
    {"ONDE", "PULSE", "ONDA"},
    {"TURBO", "TURBO", "TURBO"},
    {"BOUCLIER", "SHIELD", "ESCUDO"},
    {"COMÈTE", "COMET", "COMETA"},
    {"TIR EN APPROCHE", "INCOMING SHOT", "DISPARO ENTRANTE"},
    {"TIR EN APPROCHE !", "INCOMING SHOT!", "¡DISPARO ENTRANTE!"},
    {"GO !", "GO!", "¡YA!"},
    {"DÉPART TURBO !", "ROCKET START!", "¡SALIDA TURBO!"},
    {"TROP TÔT", "TOO EARLY", "DEMASIADO PRONTO"},
    {"RATÉ", "MISSED", "FALLADO"},
    {"COURSE TERMINÉE", "RACE OVER", "CARRERA TERMINADA"},
    {"POSITION %d / %d", "POSITION %d / %d", "POSICIÓN %d / %d"},
    {"SCORE %d", "SCORE %d", "PUNTOS %d"},
    {"PLACE", "PLACE", "PUESTO"},
    {"TEMPO", "TEMPO", "TEMPO"},
    {"PILOTAGE", "DRIVING", "PILOTAJE"},
    {"BONUS", "ITEMS", "BONUS"},
    {"NOUVEAU RECORD", "NEW RECORD", "NUEVO RÉCORD"},
    {"RECORD %d (%c)", "BEST %d (%c)", "RÉCORD %d (%c)"},
    {"TEMPS %s", "TIME %s", "TIEMPO %s"},
    {"MEILLEUR TEMPS", "BEST TIME", "MEJOR TIEMPO"},
    {"MEILLEUR %s", "BEST %s", "MEJOR %s"},
    // Fantôme et initiation
    {"FANTÔME", "GHOST", "FANTASMA"},
    {"Affiché", "Shown", "Visible"},
    {"Masqué", "Hidden", "Oculto"},
    // Tutoriel
    {"TUTORIEL", "TUTORIAL", "TUTORIAL"},
    {"SUIVANT", "NEXT", "SIGUIENTE"},
    {"PRÉCÉDENT", "BACK", "ANTERIOR"},
    {"FERMER", "CLOSE", "CERRAR"},
    {"SCORE", "SCORE", "PUNTUACIÓN"},
    {"LA MUSIQUE EST LA PISTE", "THE MUSIC IS THE TRACK", "LA MÚSICA ES LA PISTA"},
    {"Chaque morceau devient une piste, de sa première note à sa dernière.", "Every song becomes a track, from its first note to its last.", "Cada tema se convierte en una pista, de la primera nota a la última."},
    {"Virages, montées et obstacles suivent son rythme et son intensité.", "Turns, climbs and obstacles follow its beat and its intensity.", "Curvas, subidas y obstáculos siguen su ritmo y su intensidad."},
    {"En haut, la barre montre où chacun en est dans le morceau.", "The bar at the top shows where everyone is in the song.", "Arriba, la barra muestra dónde va cada uno en el tema."},
    {"PILOTER", "STEERING", "PILOTAR"},
    {"Touchez les flèches en bas de l'écran pour tourner.", "Tap the arrows at the bottom of the screen to steer.", "Toca las flechas de abajo para girar."},
    {"Tournez avec les flèches, ou %s et %s.", "Steer with the arrow keys, or %s and %s.", "Gira con las flechas, o %s y %s."},
    {"Le vaisseau accélère tout seul : vous ne gérez que la trajectoire.", "The ship speeds up on its own: you only pick the line.", "La nave acelera sola: tú solo eliges la trazada."},
    {"Les murets des bords vous freinent. À vous d'essayer !", "The side walls slow you down. Give it a try!", "Los muros de los bordes te frenan. ¡Pruébalo!"},
    {"DÉPART TURBO", "TURBO START", "SALIDA TURBO"},
    {"Pendant le compte à rebours, un anneau se resserre sur le bouton du bonus.", "During the countdown, a ring closes in on the item button.", "Durante la cuenta atrás, un anillo se cierra sobre el botón del objeto."},
    {"Pendant le compte à rebours, un anneau se resserre sur le cadre en bas à droite.", "During the countdown, a ring closes in on the box at the bottom right.", "Durante la cuenta atrás, un anillo se cierra sobre el recuadro de abajo a la derecha."},
    {"Touchez le bouton quand il s'allume : vous partez en turbo.", "Tap the button when it lights up: you start with a turbo.", "Toca el botón cuando se ilumine: sales con turbo."},
    {"Appuyez sur %s quand il s'allume : vous partez en turbo.", "Press %s when it lights up: you start with a turbo.", "Pulsa %s cuando se ilumine: sales con turbo."},
    {"Trop tôt, c'est raté pour cette course. À vous d'essayer !", "Too early and it's lost for this race. Give it a try!", "Demasiado pronto y lo pierdes en esta carrera. ¡Pruébalo!"},
    {"Passez sur les losanges jaunes pour ramasser un bonus.", "Drive over the yellow diamonds to pick up an item.", "Pasa por los rombos amarillos para recoger un objeto."},
    {"Vous n'en portez qu'un à la fois, son nom s'affiche sur le bouton.", "You carry one at a time, its name shows on the button.", "Solo llevas uno a la vez, su nombre aparece en el botón."},
    {"Vous n'en portez qu'un à la fois, son nom s'affiche en bas à droite.", "You carry one at a time, its name shows at the bottom right.", "Solo llevas uno a la vez, su nombre aparece abajo a la derecha."},
    {"Touchez le bouton pour le déclencher.", "Tap the button to use it.", "Toca el botón para usarlo."},
    {"Appuyez sur %s pour le déclencher.", "Press %s to use it.", "Pulsa %s para usarlo."},
    {"LES BONUS", "ITEMS", "LOS OBJETOS"},
    {"Les boîtes donnent un bonus au hasard.", "Boxes give a random item.", "Las cajas dan un objeto al azar."},
    {"File sur le vaisseau de devant", "Homes in on the ship ahead", "Persigue a la nave de delante"},
    {"Posée derrière vous, elle attend le suivant", "Dropped behind you, it waits for the next ship", "Queda detrás de ti y espera a la siguiente nave"},
    {"Frappe les vaisseaux tout proches", "Hits the ships right next to you", "Golpea a las naves más cercanas"},
    {"Une pointe de vitesse", "A burst of speed", "Un acelerón"},
    {"Encaisse le prochain choc, barrière comprise", "Absorbs the next hit, barriers included", "Absorbe el próximo golpe, barreras incluidas"},
    {"Réservée au dernier, frappe tous ceux qu'elle dépasse", "Only for the last ship, hits everyone it passes", "Solo para el último, golpea a todos los que adelanta"},
    {"BARRIÈRES ET TREMPLINS", "BARRIERS AND RAMPS", "BARRERAS Y RAMPAS"},
    {"Les barrières cassent votre élan : passez par l'ouverture.", "Barriers kill your speed: go through the gap.", "Las barreras te quitan la velocidad: pasa por el hueco."},
    {"Seul le centre du vaisseau compte, une aile peut passer au travers.", "Only the ship's core counts, a wing can pass through.", "Solo cuenta el núcleo de la nave, un ala puede atravesarla."},
    {"Sur un tremplin, vous survolez barrières et mines.", "On a ramp, you fly over barriers and mines.", "En una rampa, sobrevuelas barreras y minas."},
    {"LE RYTHME", "THE BEAT", "EL RITMO"},
    {"ÉCART : votre avance sur la musique, en secondes.", "GAP: how far ahead of the music you are, in seconds.", "DIF.: tu ventaja sobre la música, en segundos."},
    {"Trop d'avance vous freine, du retard vous aspire.", "Too far ahead slows you down, falling behind pulls you along.", "Ir muy adelantado te frena, quedarte atrás te impulsa."},
    {"Rester juste devant la musique rapporte le plus de points de TEMPO.", "Staying just ahead of the music earns the most TEMPO points.", "Ir justo por delante de la música da más puntos de TEMPO."},
    {"En solo, votre meilleur temps sur chaque morceau devient un fantôme.", "In solo, your best time on each song becomes a ghost.", "En solitario, tu mejor tiempo en cada tema se convierte en un fantasma."},
    {"Il rejoue votre trajectoire, sans toucher personne.", "It replays your line without touching anyone.", "Repite tu trazada sin tocar a nadie."},
    {"Sous le chrono, l'écart avec lui : vert en avance, rose en retard.", "Under the timer, your gap to it: green ahead, pink behind.", "Bajo el cronómetro, la diferencia con él: verde delante, rosa detrás."},
    {"À l'arrivée, le score compte la place, le tempo, le pilotage et les bonus.", "At the finish, the score counts place, tempo, driving and items.", "En la meta, la puntuación cuenta el puesto, el tempo, el pilotaje y los objetos."},
    {"Chaque choc et chaque frottement coûte des points de pilotage.", "Every hit and every scrape costs driving points.", "Cada golpe y cada roce cuesta puntos de pilotaje."},
    {"La note S ne s'obtient qu'en difficulté Difficile.", "The S grade is only reachable on Hard.", "La nota S solo se consigue en Difícil."},
    {"DIFFICULTÉ", "DIFFICULTY", "DIFICULTAD"},
    {"COUPES", "CUPS", "COPAS"},
    {"COUPE", "CUP", "COPA"},
    {"Coupe Néon", "Neon Cup", "Copa Neón"},
    {"Coupe Acier", "Steel Cup", "Copa Acero"},
    {"Coupe Abysse", "Abyss Cup", "Copa Abismo"},
    {"Néon", "Neon", "Neón"},
    {"Acier", "Steel", "Acero"},
    {"Abysse", "Abyss", "Abismo"},
    {"Électro rétro", "Retro electro", "Electro retro"},
    {"Metal industriel", "Industrial metal", "Metal industrial"},
    {"Dubstep", "Dubstep", "Dubstep"},
    {"Trois courses, des points à chaque arrivée", "Three races, points at every finish", "Tres carreras, puntos en cada llegada"},
    {"Pas encore courue", "Not raced yet", "Aún sin correr"},
    {"Place %d", "Place %d", "Puesto %d"},
    {"PLACE %d / %d", "PLACE %d / %d", "PUESTO %d / %d"},
    {"LANCER LA COUPE", "START THE CUP", "INICIAR LA COPA"},
    {"CLASSEMENT FINAL", "FINAL STANDINGS", "CLASIFICACIÓN FINAL"},
    {"APRÈS LA COURSE %d / %d", "AFTER RACE %d / %d", "TRAS LA CARRERA %d / %d"},
    {"SCORE MOYEN %d", "AVERAGE SCORE %d", "PUNTUACIÓN MEDIA %d"},
    {"TERMINER", "FINISH", "TERMINAR"},
    {"COURSE SUIVANTE", "NEXT RACE", "SIGUIENTE CARRERA"},
    {"ABANDONNER", "GIVE UP", "ABANDONAR"},
    {"COURIR LA COUPE", "RACE THE CUP", "CORRER LA COPA"},
    {"En attente du classement de l'hôte", "Waiting for the host's standings", "Esperando la clasificación del anfitrión"},
    {"course", "race", "carrera"},
    {"Fonderie", "Foundry", "Fundición"},
    {"Chaîne de montage", "Assembly Line", "Cadena de montaje"},
    {"Haut fourneau", "Blast Furnace", "Alto horno"},
    {"La Fosse", "The Trench", "La Fosa"},
    {"Méduses", "Jellyfish", "Medusas"},
    {"Léviathan", "Leviathan", "Leviatán"},
    {"Metal industriel martelé", "Pounding industrial metal", "Metal industrial machacón"},
    {"Metal industriel mitraillé", "Machine-gun industrial metal", "Metal industrial ametrallado"},
    {"Metal industriel furieux", "Furious industrial metal", "Metal industrial furioso"},
    {"Dubstep lourd", "Heavy dubstep", "Dubstep pesado"},
    {"Dubstep mélodique", "Melodic dubstep", "Dubstep melódico"},
    {"Drumstep brutal", "Brutal drumstep", "Drumstep brutal"},
    {"Facile", "Easy", "Fácil"},
    {"Normale", "Normal", "Normal"},
    {"Difficile", "Hard", "Difícil"},
    {"MENU", "MENU", "MENÚ"},
    {"CONTINUER", "CONTINUE", "CONTINUAR"},
    {"QUITTER LA COURSE ?", "QUIT THE RACE?", "¿SALIR DE LA CARRERA?"},
    {"REPRENDRE", "RESUME", "SEGUIR"},
    {"PAUSE", "PAUSED", "PAUSA"},
    // Morceaux du joueur
    {"Fichier illisible", "Cannot read the file", "No se puede leer el archivo"},
    {"Fichier trop gros : 80 Mo au plus", "File too large: 80 MB at most", "Archivo demasiado grande: 80 MB como máximo"},
    {"Morceau refusé : ", "Track rejected: ", "Tema rechazado: "},
    {"Morceau reçu refusé : ", "Received track rejected: ", "Tema recibido rechazado: "},
    {"Copie du morceau impossible", "Cannot copy the track", "No se puede copiar el tema"},
    {"Morceau trop court : 30 secondes au moins", "Track too short: 30 seconds at least",
     "Tema demasiado corto: 30 segundos como mínimo"},
    {"Morceau introuvable, impossible de l'envoyer", "Track not found, cannot send it",
     "Tema no encontrado, no se puede enviar"},
    {"Morceau trop gros pour le multijoueur : 40 Mo au plus", "Track too large for multiplayer: 40 MB at most",
     "Tema demasiado grande para multijugador: 40 MB como máximo"},
    {"Morceau reçu illisible", "Cannot read the received track", "No se puede leer el tema recibido"},
    {"fichier illisible", "unreadable file", "archivo ilegible"},
    {"fichier introuvable", "file not found", "archivo no encontrado"},
    {"fichier audio vide", "empty audio file", "archivo de audio vacío"},
    {"format audio non reconnu", "unknown audio format", "formato de audio desconocido"},
    {"format AAC ou M4A non pris en charge", "AAC and M4A are not supported", "AAC y M4A no son compatibles"},
    {"encodage WAV non pris en charge", "unsupported WAV encoding", "codificación WAV no compatible"},
    {"fichier WAV invalide", "invalid WAV file", "archivo WAV no válido"},
    {"fichier MP3 invalide", "invalid MP3 file", "archivo MP3 no válido"},
    {"fichier FLAC invalide", "invalid FLAC file", "archivo FLAC no válido"},
    {"fichier Ogg Vorbis invalide", "invalid Ogg Vorbis file", "archivo Ogg Vorbis no válido"},
};

} // namespace

void setLang(Lang lang) { current = lang < Lang::Count ? lang : Lang::Fr; }
Lang lang() { return current; }

const char* langName(Lang lang) {
    static const char* const names[] = {"Français", "English", "Español"};
    return names[static_cast<int>(lang < Lang::Count ? lang : Lang::Fr)];
}

const char* langCode(Lang lang) {
    static const char* const codes[] = {"fr", "en", "es"};
    return codes[static_cast<int>(lang < Lang::Count ? lang : Lang::Fr)];
}

Lang langFromCode(const char* code) {
    for (int i = 0; i < static_cast<int>(Lang::Count); i++)
        if (code && !std::strncmp(code, langCode(static_cast<Lang>(i)), 2)) return static_cast<Lang>(i);
    return Lang::Count;
}

Lang systemLang() {
    Lang found = Lang::Count;
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    // Première langue du système que le jeu connaît.
    for (int i = 0; locales && i < count && found == Lang::Count; i++)
        if (locales[i] && locales[i]->language) found = langFromCode(locales[i]->language);
    SDL_free(locales);
    // Hors de nos trois langues, l'anglais est le choix le plus lisible.
    if (found == Lang::Count) found = count > 0 ? Lang::En : Lang::Fr;
    static bool logged = false;
    if (!logged) {
        logged = true;
        SDL_Log("Langue du système : %d langue(s) préférée(s), interface en %s", count, langName(found));
    }
    return found;
}

const char* tr(const char* french) {
    if (current == Lang::Fr || !french || !french[0]) return french;
    for (const Entry& e : kEntries)
        if (!std::strcmp(e.fr, french)) return current == Lang::En ? e.en : e.es;
    return french;
}

} // namespace game
