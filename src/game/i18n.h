#pragma once

namespace game {

enum class Lang { Fr = 0, En, Es, Count };

// Langue de l'interface. Le français sert de clé : tr() reçoit le texte
// français et renvoie sa traduction, ou le texte tel quel s'il n'en a pas.
// Un nom de joueur ou de morceau passe donc au travers sans changer.
void setLang(Lang lang);
Lang lang();
// Langue du système, ou le français si elle n'est pas prise en charge.
Lang systemLang();
const char* langName(Lang lang);
const char* langCode(Lang lang);
Lang langFromCode(const char* code);

const char* tr(const char* french);

} // namespace game
