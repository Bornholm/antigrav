# Jouer

Chaque piste est générée à partir d'un morceau de musique. La course va de sa première note à sa dernière : pas de tours, et le classement se joue à l'arrivée.

- **Tempo.** Un trait barre la piste sur chaque temps, plus épais sur le premier temps de la mesure. Les boîtes à bonus tombent sur les temps forts, les barrières sur les temps accentués.
- **Intensité.** La piste plonge dans les passages intenses et remonte dans les passages calmes. Les virages se resserrent avec l'intensité.
- **Hargne.** Un morceau saturé et dense, du metal par exemple, donne une piste plus rapide, des virages deux fois plus fréquents, des boîtes à bonus plus nombreuses, et une caméra qui encaisse chaque temps. Deux barrières ne se suivent jamais à moins de deux secondes. Une barrière ne suit jamais une boîte à bonus : après une boîte, son couloir reste libre une seconde et demie.
- **Décor.** Les bâtiments s'étirent avec les bandes de fréquences du morceau, comme un égaliseur, et les néons battent sur les temps.

## Courir sur ses propres morceaux

Dans **Solo**, le sélecteur MORCEAU propose les trois morceaux livrés, puis vos cinq derniers morceaux joués, du plus récent au plus ancien, puis « Ouvrir un fichier ». Cette dernière entrée ouvre le sélecteur de fichiers du système.

- **Formats.** MP3, Ogg Vorbis, FLAC et WAV. L'AAC, le M4A et l'Opus ne sont pas lus.
- **Durée.** 30 secondes au moins. Au-delà de 8 minutes, seules les 8 premières servent.
- **Derniers joués.** Le fichier ouvert est copié dans le dossier du jeu, qui garde les cinq derniers morceaux joués. Sur Linux, c'est `~/.local/share/antigrav/antigrav/songs`.
- **Nom affiché.** Le titre et l'artiste lus dans les étiquettes du fichier, ou le nom du fichier s'il n'en a pas.
- **Écoute.** Dans les réglages de partie, le morceau sélectionné se joue derrière le menu.
- **Décor.** Il dépend du morceau : la station pour un tempo rapide, la ville pour un son brillant, le canyon sinon.
- **Sans tempo net.** Si le jeu ne trouve pas de pulsation régulière, il cale la piste sur une grille fixe et sur l'intensité du morceau, et l'indique sous le titre.


## Bonus

Des boîtes à bonus, des losanges jaunes dressés sur la piste, donnent un bonus à qui passe dessus les mains vides. On n'en porte qu'un à la fois. Le bouton de droite affiche son nom et le déclenche.

| Bonus | Effet |
|---|---|
| Tir | Part vers le vaisseau de devant et le suit. Une barrière l'arrête. |
| Mine | Posée derrière soi, elle attend le suivant pendant trente secondes. |
| Onde | Frappe tous les vaisseaux proches, devant comme derrière. |
| Turbo | Une pointe de vitesse de deux secondes et demie. |
| Bouclier | Encaisse le prochain choc, barrière comprise, puis s'éteint. Dure neuf secondes. |

Un vaisseau touché perd une bonne part de sa vitesse et le contrôle pendant près d'une seconde. Les derniers reçoivent plus souvent des tirs et des turbos, les premiers des mines et des boucliers.

Il n'y a ni jauge d'énergie ni bouton de boost : la vitesse suit la musique. Un élastique freine qui prend de l'avance sur elle et aspire qui traîne, donc le peloton reste groupé. La ligne ÉCART donne l'avance du vaisseau sur la musique.

## Menus et commandes

Le menu principal propose la langue de l'interface : français, anglais ou espagnol. Au premier lancement, le jeu prend celle du système. Dans **Multijoueur**, le bouton PSEUDO ouvre la saisie du nom vu par les autres joueurs, 16 caractères au plus. Langue et pseudo sont gardés d'un lancement à l'autre.

Le jeu s'ouvre sur le menu. **Solo** règle le morceau, le vaisseau, le nombre d'IA de 0 à 7 et leur caractère. **Multijoueur** héberge une partie ou rejoint celles trouvées sur le réseau local.

La piste porte deux sortes d'éléments. Les losanges jaunes sont des boîtes à bonus, les barrières rouges arrêtent net. Seul le cœur du vaisseau touche une barrière : une aile peut passer au travers.

Pour jouer à plusieurs, tous les appareils doivent être sur le même réseau Wi-Fi, ou sur le partage de connexion de l'un d'eux. L'hôte choisit le morceau, les IA et lance la course. Si la découverte automatique ne trouve rien, saisir l'adresse IP de l'hôte. Le jeu utilise le port 47800, en UDP pour la course et en TCP pour l'envoi du morceau.

L'hôte peut choisir n'importe quel morceau, y compris un de ses fichiers. Le jeu l'envoie alors aux autres joueurs, avec son analyse, et la course ne peut partir que lorsque tout le monde l'a reçu. Le salon affiche l'avancement. Un fichier de plus de 40 Mo n'est pas envoyé. Le morceau reçu ne sert qu'à cette partie, il n'est pas gardé dans la bibliothèque.

| Action | Clavier | Manette | Écran |
|---|---|---|---|
| Tourner | flèches, ou A et D | stick gauche ou croix | boutons en bas à gauche |
| Bonus | espace, flèche haut ou W | A ou gâchette droite | bouton de droite |
| Menus | flèches, Entrée, Échap | croix, A, B | toucher |

En haut de l'écran, la barre montre où en est chaque vaisseau dans le morceau. Le cadre de droite montre les virages à venir.

La souris agit comme un doigt. Pendant une course, le bouton MENU, Échap ou le geste retour d'Android proposent de quitter la course.
