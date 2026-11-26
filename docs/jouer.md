# Jouer

Chaque piste est générée à partir d'un morceau de musique. La course va de sa première note à sa dernière : pas de tours, et le classement se joue à l'arrivée.

- **Tempo.** Un trait barre la piste sur chaque temps, plus épais sur le premier temps de la mesure. Les boîtes à bonus tombent sur les temps forts, les barrières sur les temps accentués.
- **Intensité.** La piste plonge dans les passages intenses et remonte dans les passages calmes, avec des descentes plus raides que les montées. Sa couleur suit : froide quand le morceau est calme, elle vire à l'orange quand il chauffe. Les virages se resserrent avec l'intensité.
- **Grands moments.** Avant chaque montée brutale du morceau, la piste grimpe pendant sept secondes et bascule au sommet pile sur le drop. Un tremplin y attend : tous les vaisseaux décollent, survolent barrières et mines pendant un peu plus d'une seconde, puis retombent dans la descente.
- **Vrilles.** Dans un morceau qui cogne, la piste fait par moments un tour complet sur elle-même, sur huit temps.
- **Hargne.** Un morceau saturé et dense, du metal par exemple, donne une piste plus rapide, des virages deux fois plus fréquents, des boîtes à bonus plus nombreuses, et une caméra qui encaisse chaque temps. Deux barrières ne se suivent jamais à moins de deux secondes. Une barrière ne suit jamais une boîte à bonus : après une boîte, son couloir reste libre une seconde et demie.
- **Décor.** Les bâtiments s'étirent avec les bandes de fréquences du morceau, comme un égaliseur, et les néons battent sur les temps.
- **Décor vivant.** Des véhicules circulent au-dessus de la ville sur trois altitudes, quelques bandes de créatures ailées traversent le canyon, des drones dérivent autour de la station.

## Courir sur ses propres morceaux

Dans **Solo**, la ligne MORCEAU ouvre la liste des morceaux : à gauche les morceaux livrés, coupe par coupe, à droite vos cinq derniers morceaux joués, du plus récent au plus ancien. CHOISIR UN FICHIER ouvre le sélecteur de fichiers du système.

- **Formats.** MP3, Ogg Vorbis, FLAC et WAV. L'AAC, le M4A et l'Opus ne sont pas lus.
- **Durée.** 30 secondes au moins. Au-delà de 8 minutes, seules les 8 premières servent.
- **Derniers joués.** Le fichier ouvert est copié dans le dossier du jeu, qui garde les cinq derniers morceaux joués. Sur Linux, c'est `~/.local/share/soundwave/soundwave/songs`.
- **Nom affiché.** Le titre et l'artiste lus dans les étiquettes du fichier, ou le nom du fichier s'il n'en a pas.
- **Écoute.** Dans les réglages de partie, le morceau sélectionné se joue derrière le menu.
- **Décor.** Il dépend du morceau : la station pour un tempo rapide, la ville pour un son brillant, le canyon sinon.
- **Sans tempo net.** Si le jeu ne trouve pas de pulsation régulière, il cale la piste sur une grille fixe et sur l'intensité du morceau, et l'indique sous le titre.


## Départ turbo

Sur un départ arrêté, appuyer sur le bouton de bonus juste avant le signal, dans les trois dernières dixièmes de seconde du compte à rebours, lance le vaisseau en turbo pendant une seconde et demie. « DÉPART TURBO ! » s'affiche sous le « GO ! ».

Le bouton de bonus aide à viser : pendant le compte à rebours, un anneau se resserre autour de lui et le touche à l'ouverture de la fenêtre. Le bouton s'allume alors en bleu avec le mot TURBO, c'est le moment d'appuyer. Un départ raté le passe en rose avec le mot RATÉ. Au clavier et à la manette, la même aide s'affiche sur le cadre du bonus, en bas à droite.

Un appui dans la dernière seconde mais trop tôt affiche « TROP TÔT » et fait rater le départ turbo, même si l'on rappuie ensuite au bon moment. Appuyer plusieurs fois dans la fenêtre le fait rater aussi. Un appui avant la dernière seconde ne compte pas. Les IA tentent aussi leur chance : souvent en difficile, rarement en facile.

## Bonus

Des boîtes à bonus, de grands losanges jaunes surmontés d'un faisceau, donnent un bonus à qui passe dessus les mains vides. On n'en porte qu'un à la fois. Le bouton de droite affiche son nom et le déclenche.

| Bonus | Effet |
|---|---|
| Tir | Part vers le vaisseau de devant et le suit. Une barrière l'arrête. |
| Mine | Posée derrière soi, elle attend le suivant pendant trente secondes. |
| Onde | Frappe tous les vaisseaux proches, devant comme derrière. |
| Turbo | Une pointe de vitesse de deux secondes et demie. |
| Bouclier | Encaisse le prochain choc, barrière comprise, puis s'éteint. Dure neuf secondes. |
| Comète | Réservée au dernier. Elle remonte toute la piste et frappe chaque vaisseau qu'elle dépasse, jusqu'au meneur. Seul un bouclier l'arrête. |

Chaque bonus a son effet lumineux et son bruit : halo sur les tirs, mines qui clignotent, onde de choc qui enfle, explosion sur un vaisseau touché. Un voile de couleur passe sur l'écran quand on ramasse un bonus ou qu'on encaisse un choc.

Un vaisseau touché perd une bonne part de sa vitesse et le contrôle pendant près d'une seconde. Les derniers reçoivent plus souvent des tirs et des turbos, les premiers des mines et des boucliers. Le tout dernier a une chance sur cinq de tirer une comète, de quoi rebattre les cartes.

Il n'y a ni jauge d'énergie ni bouton de boost : la vitesse suit la musique. Un élastique freine qui prend de l'avance sur elle et aspire qui traîne, donc le peloton reste groupé. La ligne ÉCART donne l'avance du vaisseau sur la musique.

## Coupes

Une coupe enchaîne trois courses sur un même style musical, contre cinq IA qui restent les mêmes d'une course à l'autre. Chaque coupe a son décor : ses couleurs, son ciel, sa brume.

| Coupe | Style | Courses | Décor |
|---|---|---|---|
| Néon | Électro rétro | Néon City, Canyon Solaire, Station Orbitale | Nuit violette, crépuscule orange, vide spatial |
| Acier | Metal industriel | Fonderie, Chaîne de montage, Haut fourneau | Une bouche de four sur l'horizon, des colonnes de fumée, des braises |
| Abysse | Dubstep | La Fosse, Méduses, Léviathan | Les fonds marins : la surface qui miroite, des rais de lumière, du plancton |

Chaque arrivée rapporte des points selon la place : 10, 7, 5, 4, 3 puis 2. Le classement général s'affiche entre les courses. À la fin, la coupe donne une place et une note, celle du score moyen des trois courses. Le menu des coupes garde la meilleure note et la meilleure place de chacune.

Le ciel des coupes Acier et Abysse réagit aux graves du morceau : la bouche de four et les rais de lumière battent avec la basse.

En multijoueur, l'hôte lance une coupe depuis la liste des morceaux du salon, avec le bouton COURIR LA COUPE. Le classement compte tous les joueurs et les IA. Il s'affiche chez chacun après chaque course, puis tout le monde revient au salon et l'hôte lance la suivante. L'hôte attribue les points au moment où il quitte l'écran de fin de course. Un joueur qui rejoint le salon en cours de coupe entre au classement à sa première course. Les records de coupe ne sont tenus qu'en solo.

Les neuf morceaux se courent aussi un par un, en solo comme en multijoueur. Dans la liste des morceaux, le réglage COUPE feuillette les trois coupes.

## Score et note

Chaque course finit sur un score sur 10 000 et une note : S à partir de 9 000, A à partir de 7 500, B à partir de 6 000, C à partir de 4 000, D en dessous. Le score ne dépend pas de la durée du morceau.

| Part | Points | Ce qui compte |
|---|---|---|
| Place | 4 000 | Le classement à l'arrivée. Premier, tout ; dernier, rien. |
| Tempo | 3 000 | Le temps passé à hauteur de la musique. La part est pleine à une demi-seconde d'avance et nulle à trois secondes de retard. |
| Pilotage | 2 000 | Les chocs encaissés et le temps passé à frotter un muret, rapportés à la minute. |
| Bonus | 1 000 | Les bonus ramassés. Huit par minute font le plein. |

La part de place dépend de la difficulté : elle est multipliée par 0,5 en facile et par 0,7 en normale. Une course parfaite plafonne alors à 8 000 et 8 800 points : la note S ne s'obtient qu'en difficile. Seul en piste, elle vaut la moitié.

À l'arrivée, le morceau s'efface et une courte musique le remplace : sur le podium, une nappe grave de synthé qui s'ouvre sur un accord tenu ; sinon, un accord mineur qui s'affaisse et s'étouffe. Le podium compte une place jusqu'à trois concurrents, deux à partir de quatre, trois à partir de six. Les vaisseaux freinent après la ligne et s'arrêtent sur la piste.

En solo, le jeu garde le meilleur score de chaque morceau. Sa note s'affiche dans la liste des morceaux, et l'écran de fin dit si le record est battu.

## Difficulté

Le réglage DIFFICULTÉ, en solo et dans le salon de l'hôte, a trois niveaux. En difficile, les IA lisent la piste de loin et ne ratent pas une occasion d'utiliser un bonus. En normale, elles anticipent un peu moins, ont des moments d'inattention et laissent passer un tiers des occasions. En facile, elles voient les barrières tard, décrochent près d'une seconde sur trois et n'utilisent leurs bonus qu'une fois sur trois.

## Menus et commandes

L'écran **Options** du menu principal règle la langue de l'interface (français, anglais ou espagnol) et le pseudo vu par les autres joueurs, 16 caractères au plus. Par défaut, sur Android comme sur Linux, le jeu suit la langue du système : le choix « Auto » du réglage LANGUE. Une langue non traduite donne l'anglais. Sur un écran tactile, le réglage DIRECTION place les boutons de direction à gauche ou à droite, le bouton de bonus passant de l'autre côté. Le pseudo se change aussi dans **Multijoueur**. Ces réglages sont gardés d'un lancement à l'autre.

Le réglage QUALITÉ vaut « Auto » par défaut, avec entre parenthèses le niveau retenu. Au premier lancement, le jeu estime ce niveau d'après l'appareil : haute avec une carte graphique dédiée, moyenne avec un GPU intégré de PC, et sur téléphone selon la mémoire, haute à partir de 7 Go, moyenne à partir de 4,5 Go, basse en dessous. Pendant le jeu, il mesure la charge : si la résolution doit descendre à 60 % ou moins pendant trois secondes, le niveau baisse d'un cran ; si le GPU reste sous 40 % de son temps disponible pendant vingt secondes à pleine résolution, il monte d'un cran. Une montée qui ne tient pas est annulée et le jeu ne réessaie plus de monter jusqu'au lancement suivant. Le niveau retenu est gardé pour les lancements suivants. Choisir Basse, Moyenne ou Haute fixe le niveau et coupe ce réglage automatique.

Le réglage CADENCE fonctionne de la même façon. En « Auto », le jeu vise la fréquence de l'écran, par exemple 120 images par seconde sur un écran à 120 Hz. Si même la qualité basse ne suffit pas à la tenir, il plafonne à 60 images par seconde, ce qui double le temps disponible pour chaque image et lui permet de remonter en qualité. Il ne repasse à la cadence de l'écran que si le GPU tiendrait largement celle-ci. Un écran à 60 Hz n'est jamais plafonné. Choisir « Maximale » ou « 60 images/s » fixe la cadence.

Sur Windows, le jeu cherche au démarrage si une nouvelle version est publiée sur GitHub. Si c'est le cas, un bouton NOUVELLE VERSION apparaît sous le menu principal et ouvre la page de téléchargement. Le réglage MISES À JOUR des options coupe cette recherche. Les versions de développement, dont le numéro n'est pas celui d'une release, ne cherchent pas. Sur Android, le Play Store se charge des mises à jour.

Deux réglages allègent le jeu sur un appareil modeste.

- **Qualité.** Haute par défaut. Moyenne réduit l'anticrénelage, le halo et la définition de la 3D à 85 %. Basse coupe en plus les reflets sur la piste, descend la 3D à 70 %, raccourcit la distance de vue et retire la moitié des objets volants.
- **Cadence.** Maximale suit l'écran. 60 images/s plafonne l'affichage, pour un téléphone qui chauffe ou une batterie à ménager.

À tous les niveaux, le jeu réduit de lui-même la définition de la 3D quand les images arrivent en retard, jusqu'à la moitié, et la remonte quand la charge retombe. L'interface reste nette.

Au clavier ou à la manette, les boutons tactiles ne sont pas affichés. Le bonus porté apparaît en bas à droite.

Le jeu s'ouvre sur le menu. **Solo** règle le morceau, le vaisseau, le nombre d'IA de 0 à 7 et leur caractère. **Multijoueur** héberge une partie ou rejoint celles trouvées sur le réseau local.

La piste porte deux sortes d'éléments. Les losanges jaunes sont des boîtes à bonus, les barrières rouges arrêtent net. Seul le cœur du vaisseau touche une barrière : une aile peut passer au travers.

Pour jouer à plusieurs, tous les appareils doivent être sur le même réseau Wi-Fi, ou sur le partage de connexion de l'un d'eux. L'hôte choisit le morceau, les IA et lance la course. Si la découverte automatique ne trouve rien, saisir l'adresse IP de l'hôte. Le jeu utilise le port 47800, en UDP pour la course et en TCP pour l'envoi du morceau.

L'hôte peut choisir n'importe quel morceau, y compris un de ses fichiers. Le jeu l'envoie alors aux autres joueurs, avec son analyse, et la course ne peut partir que lorsque tout le monde l'a reçu. Le salon affiche l'avancement. Un fichier de plus de 40 Mo n'est pas envoyé. Le morceau reçu ne sert qu'à cette partie, il n'est pas gardé dans la bibliothèque.

| Action | Clavier | Manette | Écran |
|---|---|---|---|
| Tourner | flèches, ou A et D | stick gauche ou croix | boutons en bas à gauche |
| Bonus | espace, flèche haut ou W | A ou gâchette droite | bouton de droite |
| Menus | flèches, Entrée, Échap | croix, A, B | toucher |

Les lettres A, D et W désignent une position sur le clavier, celle d'un clavier américain. Sur un clavier français, ce sont les touches Q, D et Z. L'écran **Options**, ligne CLAVIER, affiche le nom réel des touches et permet d'en choisir d'autres : toucher une commande, puis appuyer sur la touche voulue. Échap annule, PAR DÉFAUT rétablit les touches d'origine. Les flèches et la barre d'espace restent actives dans tous les cas.

En haut de l'écran, la barre montre où en est chaque vaisseau dans le morceau. Le cadre de droite montre les virages à venir.

La souris agit comme un doigt. Pendant une course, le bouton MENU, Échap ou le geste retour d'Android proposent de quitter la course. En solo, ce menu met la course en pause : les vaisseaux et la musique s'arrêtent, et REPRENDRE les relance au même instant. En multijoueur la course continue derrière le menu, puisqu'elle ne peut pas s'arrêter pour les autres joueurs.

Le jeu retient le dernier vaisseau choisi et le repropose au lancement suivant.
