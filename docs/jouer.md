# Jouer

Chaque piste est générée à partir d'un morceau de musique. La course va de sa première note à sa dernière : pas de tours, et le classement se joue à l'arrivée.

- **Tempo.** Un trait barre la piste sur chaque temps, plus épais sur le premier temps de la mesure. Les plaques d'énergie tombent sur les temps forts, les barrières sur les temps accentués.
- **Intensité.** La piste plonge dans les passages intenses et remonte dans les passages calmes. Les virages se resserrent avec l'intensité.
- **Hargne.** Un morceau saturé et dense, du metal par exemple, donne une piste plus rapide, des virages deux fois plus fréquents, deux plaques par mesure, et une caméra qui encaisse chaque temps. Deux barrières ne se suivent jamais à moins de deux secondes. Une barrière ne suit jamais une plaque : après une plaque, son couloir reste libre une seconde et demie.
- **Décor.** Les bâtiments s'étirent avec les bandes de fréquences du morceau, comme un égaliseur, et les néons battent sur les temps.

## Courir sur ses propres morceaux

Dans **Solo**, le sélecteur MORCEAU propose les trois morceaux livrés, puis vos cinq derniers morceaux joués, du plus récent au plus ancien, puis « Ouvrir un fichier ». Cette dernière entrée ouvre le sélecteur de fichiers du système.

- **Formats.** MP3, Ogg Vorbis, FLAC et WAV. L'AAC, le M4A et l'Opus ne sont pas lus.
- **Durée.** 30 secondes au moins. Au-delà de 8 minutes, seules les 8 premières servent.
- **Derniers joués.** Le fichier ouvert est copié dans le dossier du jeu, qui garde les cinq derniers morceaux joués. Sur Linux, c'est `~/.local/share/antigrav/antigrav/songs`.
- **Décor.** Il dépend du morceau : la station pour un tempo rapide, la ville pour un son brillant, le canyon sinon.
- **Sans tempo net.** Si le jeu ne trouve pas de pulsation régulière, il cale la piste sur une grille fixe et sur l'intensité du morceau, et l'indique sous le titre.


## Jouer en rythme

- **Accord.** Le jeu juge la régularité de vos frappes sur le BOOST, pas leur alignement exact sur le morceau. Les deux premières frappes posent votre cadence. Ensuite, chaque frappe qui arrive une période après la précédente est un accord : un court boost gratuit, qui tient jusqu'à la frappe suivante sans garder le bouton enfoncé, et un peu d'énergie. Battre la mesure régulièrement suffit donc à rester en boost.
- **Cadence.** Elle doit rester cohérente avec le tempo du morceau : un temps, un demi-temps ou deux temps, à 16 % près. Une cadence régulière mais sans rapport avec le morceau affiche « HORS TEMPO ».
- **Frappe irrégulière.** Elle affiche « TROP TÔT » ou « TROP TARD » par rapport à votre propre cadence, coupe l'élan, remet la série à zéro et pose une nouvelle cadence. Sauter une frappe ne casse rien.
- **Signal.** Une lumière sur le dos du vaisseau monte entre deux frappes et culmine quand la suivante est due. Tant que vous n'avez pas posé de cadence, elle bat doucement avec le morceau. Elle est verte tant que la série tient, rouge sur une frappe irrégulière.
- **Bouton enfoncé.** Garder le BOOST enfoncé reste possible, mais ce boost-là se paie en énergie.
- **Jauge.** Elle ne se recharge presque plus seule : ce sont les plaques et les accords qui la remplissent. Elle sert au boost et à l'armure.
- **Armure.** Si la jauge est au moins à un tiers quand le vaisseau touche une barrière, elle encaisse le choc et perd ce tiers. Sinon c'est le crash. Il faut donc choisir entre dépenser en boost et garder de quoi encaisser.
- **Décharge.** Douze accords d'affilée, comptés par les pastilles sous le vaisseau, déclenchent une décharge qui part seule sur le vaisseau de devant. En tête de course, la série rend de l'énergie à la place. Un appui à côté du temps remet la série à zéro.
- **Parade.** Quand une décharge arrive, le jeu prévient. Être en plein accord à ce moment l'absorbe et rend de l'énergie. Sinon le vaisseau perd le contrôle une demi-seconde.

Tous les joueurs entendent le morceau au même instant. La ligne ÉCART donne l'avance du vaisseau sur la musique. Un élastique freine qui prend de l'avance et aspire qui traîne, donc le peloton reste groupé autour de la musique.

## Menus et commandes

Le menu principal propose la langue de l'interface : français, anglais ou espagnol. Au premier lancement, le jeu prend celle du système. Dans **Multijoueur**, le bouton PSEUDO ouvre la saisie du nom vu par les autres joueurs, 16 caractères au plus. Langue et pseudo sont gardés d'un lancement à l'autre.

Le jeu s'ouvre sur le menu. **Solo** règle le morceau, le vaisseau, le nombre d'IA de 0 à 7 et leur caractère. **Multijoueur** héberge une partie ou rejoint celles trouvées sur le réseau local.

La piste porte deux sortes d'éléments. Les plaques à chevrons jaunes rechargent la jauge d'énergie, les barrières rouges arrêtent net. Seul le cœur du vaisseau les touche : une aile peut passer au travers.

Pour jouer à plusieurs, tous les appareils doivent être sur le même réseau Wi-Fi, ou sur le partage de connexion de l'un d'eux. L'hôte choisit le morceau, les IA et lance la course. Si la découverte automatique ne trouve rien, saisir l'adresse IP de l'hôte. Le jeu utilise le port 47800, en UDP pour la course et en TCP pour l'envoi du morceau.

L'hôte peut choisir n'importe quel morceau, y compris un de ses fichiers. Le jeu l'envoie alors aux autres joueurs, avec son analyse, et la course ne peut partir que lorsque tout le monde l'a reçu. Le salon affiche l'avancement. Un fichier de plus de 40 Mo n'est pas envoyé. Le morceau reçu ne sert qu'à cette partie, il n'est pas gardé dans la bibliothèque.

| Action | Clavier | Manette | Écran |
|---|---|---|---|
| Tourner | flèches, ou A et D | stick gauche ou croix | boutons en bas à gauche |
| Boost | espace, flèche haut ou W | A ou gâchette droite | bouton BOOST |
| Menus | flèches, Entrée, Échap | croix, A, B | toucher |

En haut de l'écran, la barre sous la jauge montre où en est chaque vaisseau dans le morceau. Le cadre de droite montre les virages à venir.

La souris agit comme un doigt. Pendant une course, le bouton MENU, Échap ou le geste retour d'Android proposent de quitter la course.
