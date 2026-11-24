# Jouer

Chaque piste est générée à partir d'un morceau de musique. La course va de sa première note à sa dernière : pas de tours, et le classement se joue à l'arrivée. Les plaques de boost tombent sur le premier temps de chaque mesure, les obstacles sur les temps accentués, et les néons battent avec la musique.

## Courir sur ses propres morceaux

Dans **Solo**, le sélecteur MORCEAU propose les trois morceaux livrés, puis les morceaux déjà ouverts, puis « Ouvrir un fichier ». Cette dernière entrée ouvre le sélecteur de fichiers du système.

- **Formats.** MP3, Ogg Vorbis, FLAC et WAV. L'AAC, le M4A et l'Opus ne sont pas lus.
- **Durée.** 30 secondes au moins. Au-delà de 8 minutes, seules les 8 premières servent.
- **Bibliothèque.** Le fichier ouvert est copié dans le dossier du jeu, qui garde les 12 derniers. Sur Linux, c'est `~/.local/share/antigrav/antigrav/songs`.
- **Décor.** Il dépend du morceau : la station pour un tempo rapide, la ville pour un son brillant, le canyon sinon.
- **Sans tempo net.** Si le jeu ne trouve pas de pulsation régulière, il cale la piste sur une grille fixe et sur l'intensité du morceau, et l'indique sous le titre.

En multijoueur, seuls les morceaux livrés sont proposés pour l'instant.

## Jouer en rythme

- **Accord.** Lancer le boost pile sur un temps donne un accord : le boost ne coûte rien pendant une demi-seconde, pousse plus fort et rend un peu d'énergie. « EN RYTHME » s'affiche. Un accord est possible toutes les deux secondes environ.
- **Plaques.** Chaque plaque de boost prise rend aussi un peu d'énergie.
- **Jauge.** Elle ne se recharge presque plus seule. Boost, missile et bouclier se paient surtout en jouant en rythme.

Tous les joueurs entendent le morceau au même instant. La ligne ÉCART donne l'avance du vaisseau sur la musique. Un élastique freine qui prend de l'avance et aspire qui traîne, donc le peloton reste groupé autour de la musique.

## Menus et commandes

Le jeu s'ouvre sur le menu. **Solo** règle le morceau, le vaisseau, le nombre d'IA de 0 à 7 et leur caractère. **Multijoueur** héberge une partie ou rejoint celles trouvées sur le réseau local.

La piste porte trois sortes d'éléments. Les plaques à chevrons jaunes accélèrent, les champs hachurés rouges freinent et vident la jauge, les barrières rouges arrêtent net. Le bouclier protège des champs et encaisse une barrière.

Pour jouer à plusieurs, tous les appareils doivent être sur le même réseau Wi-Fi, ou sur le partage de connexion de l'un d'eux. L'hôte choisit le morceau, les IA et lance la course. Si la découverte automatique ne trouve rien, saisir l'adresse IP de l'hôte. Le jeu utilise le port UDP 47800.

| Action | Clavier | Manette | Écran |
|---|---|---|---|
| Tourner | flèches, ou A et D | stick gauche ou croix | boutons en bas à gauche |
| Boost | espace, flèche haut ou W | A ou gâchette droite | bouton BOOST |
| Missile | E | X | bouton rose |
| Bouclier | Q | B | bouton bleu |
| Menus | flèches, Entrée, Échap | croix, A, B | toucher |

En haut de l'écran, la barre sous la jauge montre où en est chaque vaisseau dans le morceau. Le cadre de droite montre les virages à venir.

La souris agit comme un doigt. Pendant une course, le bouton MENU, Échap ou le geste retour d'Android proposent de quitter la course.
