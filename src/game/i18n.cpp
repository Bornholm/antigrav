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
    {"COURSE ANTIGRAVITÉ", "ANTIGRAVITY RACING", "CARRERAS ANTIGRAVEDAD"},
    {"SOLO", "SOLO", "UN JUGADOR"},
    {"MULTIJOUEUR", "MULTIPLAYER", "MULTIJUGADOR"},
    {"QUITTER", "QUIT", "SALIR"},
    {"LANGUE", "LANGUAGE", "IDIOMA"},
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
    {"ARMURE", "ARMOUR", "ESCUDO"},
    {"PRÊTE", "READY", "LISTO"},
    {"VIDE", "EMPTY", "VACÍO"},
    {"ÉNERGIE", "ENERGY", "ENERGÍA"},
    {"BOOST", "BOOST", "TURBO"},
    {"EN RYTHME", "ON THE BEAT", "AL RITMO"},
    {"TROP TÔT", "TOO EARLY", "DEMASIADO PRONTO"},
    {"TROP TARD", "TOO LATE", "DEMASIADO TARDE"},
    {"HORS TEMPO", "OFF TEMPO", "FUERA DE TEMPO"},
    {"DÉCHARGE ! PAREZ EN RYTHME", "INCOMING! PARRY ON THE BEAT", "¡DESCARGA! BLOQUEA AL RITMO"},
    {"GO !", "GO!", "¡YA!"},
    {"COURSE TERMINÉE", "RACE OVER", "CARRERA TERMINADA"},
    {"POSITION %d / %d", "POSITION %d / %d", "POSICIÓN %d / %d"},
    {"MENU", "MENU", "MENÚ"},
    {"CONTINUER", "CONTINUE", "CONTINUAR"},
    {"QUITTER LA COURSE ?", "QUIT THE RACE?", "¿SALIR DE LA CARRERA?"},
    {"REPRENDRE", "RESUME", "SEGUIR"},
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
    return found == Lang::Count ? (count > 0 ? Lang::En : Lang::Fr) : found;
}

const char* tr(const char* french) {
    if (current == Lang::Fr || !french || !french[0]) return french;
    for (const Entry& e : kEntries)
        if (!std::strcmp(e.fr, french)) return current == Lang::En ? e.en : e.es;
    return french;
}

} // namespace game
