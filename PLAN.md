# Antigrav, plan de développement

Jeu de course antigravité néon, multijoueur en Wi-Fi local, pour Android et Linux. Moteur maison bas niveau, sans Godot ni moteur généraliste.

## Objectif

Reproduire le rendu du mockup de référence à 60 images par seconde sur un Pixel 7 Pro, puis viser 120.

Ce que le mockup impose au moteur :

- une piste sombre et réfléchissante, bordée de rails cyan et magenta
- du bloom sur tout ce qui émet de la lumière
- des traînées de réacteur et des lignes de vitesse
- une ville néon à l'horizon, avec brume de distance
- un HUD net à la résolution native : énergie, position, tour, vitesse, minimap, noms des adversaires
- cinq boutons tactiles : gauche, droite, boost, missile, bouclier

## État d'avancement

Mis à jour le 24 novembre 2026.

| Phase | État | Détail |
|---|---|---|
| 0, socle | Fait, Android non exécuté | Build CMake pour Linux, APK Android de 7 Mo construit par Gradle. Vulkan 1.1, fenêtre et mode sans fenêtre, validation propre sur Linux. |
| 1, rendu | Fait sur Linux | Les quatre passes, HUD, trois thèmes visuels. À valider sur le Pixel. |
| 2, course solo | Fait sur Linux | Simulation à pas fixe, clavier, souris, manette, tours, départ, arrivée, son synthétisé. |
| 3, multijoueur | Fait, essayé en boucle locale | Découverte, salon, hôte autoritaire, prédiction et recalage côté client. Jamais essayé entre deux appareils sur un vrai Wi-Fi. |
| 4, contenu | Fait pour l'essentiel | Trois circuits, quatre vaisseaux, quatre personnalités d'IA, missile, bouclier, menus solo et multijoueur. |
| 5, finition | Pas commencé | |

Jamais exécuté à ce jour : tout ce qui est propre à Android, soit la rotation de l'écran appliquée par le jeu, la mise en arrière-plan, le tactile réel et le verrou multicast. La manette n'a pas été essayée non plus.

## Contenu

| Circuit | Topologie | Thème |
|---|---|---|
| Néon City | Grandes courbes rapides, 10,7 km, piste de 30 m | Ville de nuit, cyan et magenta |
| Canyon Solaire | Sinueux et vallonné, piste de 26 m | Défilé de pitons au crépuscule, ambre et rouge |
| Station Orbitale | Huit dont une branche enjambe l'autre, piste de 34 m | Vide spatial, anneaux à traverser, vert et bleu |

| Élément de piste | Effet | Parade |
|---|---|---|
| Plaque de boost, chevrons jaunes | Porte le vaisseau au-delà de la vitesse de boost, sans coût | Aucune, mais la vitesse gagnée fait dériver en virage |
| Champ de freinage, hachures rouges | Freine fort et vide la jauge tant qu'on y reste | L'éviter, ou le traverser bouclier levé |
| Barrière rouge | Choc : plus de la moitié de la vitesse perdue, 0,6 s sans contrôle | L'éviter ; un bouclier levé encaisse le choc et se brise |

Les éléments sont semés tous les 300 m environ, par un tirage fixe : un circuit a toujours la même disposition. Néon City a surtout des plaques et des barrières, Canyon Solaire un mélange des trois, Station Orbitale surtout des plaques et des champs. En virage, les champs occupent la corde et les plaques l'extérieur. Une barrière arrête aussi les missiles.

| Vaisseau | Caractère |
|---|---|
| Flèche | Équilibrée |
| Manta | Maniable, dérive peu, plus fragile |
| Aiguille | Meilleure pointe de vitesse, dérive fort, repart lentement |
| Jumeau | Repart vite, encaisse les impacts, moins rapide en pointe |

| Personnalité d'IA | Comportement |
|---|---|
| Agressif | Tire dès qu'il a une cible, se rabat sur ses voisins |
| Défensif | Garde de l'énergie pour son bouclier, ferme la porte à celui qui le suit |
| Pilote | Ne tire jamais, soigne sa trajectoire, dépense tout en boost |
| Imprévisible | Change de couloir, tire et accélère par à-coups |

## Équilibrage

Boost, missile et bouclier puisent dans la même jauge d'énergie. Trois règles évitent qu'une stratégie écrase les autres :

- Glisser en virage coûte de la vitesse, donc l'adhérence compense la pointe de vitesse.
- Un missile qui touche rend 20 % d'énergie au tireur, donc attaquer n'est pas une perte sèche.
- Un bouclier coûte 35 % et en rend 25 quand il absorbe un missile, donc se défendre au bon moment rapporte, mais se protéger à vide coûte cher.

`build/balance` joue 864 courses entre pilotes automatiques et échoue si un vaisseau ou une personnalité gagne plus de 40 % ou moins de 10 % des courses. Dernière mesure, sur les trois circuits :

| À personnalité égale | Victoires | À vaisseau égal | Victoires |
|---|---|---|---|
| Flèche | 18 % | Agressif | 24 % |
| Manta | 26 % | Défensif | 21 % |
| Aiguille | 31 % | Pilote | 25 % |
| Jumeau | 25 % | Imprévisible | 31 % |

Limite de cette mesure : elle oppose des IA entre elles. Elle ne dit rien d'une stratégie qu'un joueur humain inventerait et qu'aucune personnalité ne joue. Les IA esquivent presque toujours les barrières : un joueur humain en touchera davantage, ce qui avantage en pratique les vaisseaux solides.

Mesures sur la machine de développement, GPU Intel Arrow Lake, par image :

| Résolution de la 3D | Temps GPU | Reflet | Scène | Bloom | Composition |
|---|---|---|---|---|---|
| 1920 par 1080 | 2,2 ms | 0,24 | 1,43 | 0,26 | 0,27 |
| 3120 par 1440, celle du Pixel | 4,1 ms | 0,37 | 2,70 | 0,49 | 0,53 |
| 2184 par 1008, échelle 0,7 | 2,4 ms | 0,24 | 1,43 | 0,26 | 0,48 |

Ces chiffres viennent du mode sans fenêtre, où le GPU tourne à pleine fréquence. En fenêtre à 60 Hz, le même rendu prend environ 5 ms parce que le GPU baisse sa fréquence. Aucune mesure n'existe encore sur le Pixel.

## Hypothèses

Elles viennent de moi, pas de vous. Corrigez celles qui sont fausses, elles changent le planning.

- Une personne à plein temps, à l'aise en C++ et déjà familière d'une API graphique.
- Les durées sont des ordres de grandeur, pas des engagements.
- Les modèles 3D des vaisseaux n'existent pas encore.

## Appareils de référence

| Cible | Appareil | GPU | Vulkan |
|---|---|---|---|
| Android | Pixel 7 Pro | Mali-G710, architecture à tuiles | 1.3 |
| Linux | Machine de développement | Intel Arrow Lake intégré, pilote Mesa, session Wayland | 1.4 |

Aucun téléphone bas de gamme n'est visé pour la v1. Les deux GPU sont des puces intégrées limitées par le remplissage de pixels, donc les mêmes optimisations servent aux deux.

Le moteur se contente de Vulkan 1.1, pour tourner aussi sur les téléphones dont le pilote n'a jamais dépassé cette version, un Redmi Note 10 Pro par exemple. Il n'utilise donc ni le rendu dynamique ni la synchronisation 2 : passes de rendu et framebuffers classiques, sur un seul chemin de code quel que soit le GPU. Le coût à l'exécution est nul, l'image rendue est identique au pixel près. Si le GPU n'offre pas le format HDR 11-11-10 ou la profondeur 32 bits, le moteur se rabat sur RGBA16F et sur une profondeur 24 ou 16 bits.

## Stack

| Rôle | Choix | Raison |
|---|---|---|
| Langage | C++20, compilé avec clang | Le NDK Android fournit clang, donc un seul compilateur pour les deux cibles |
| Build | CMake et Ninja | Le même `CMakeLists.txt` sert à Linux et au projet Gradle Android |
| Plateforme | SDL3, version 3.2 ou plus | Fenêtre, tactile, manette, son, cycle de vie Android |
| Rendu | Vulkan en direct | Un seul chemin de rendu pour Android et Linux, contrôle complet des passes |
| Chargement Vulkan | volk | Charge les fonctions Vulkan sans dépendre du loader à l'édition de liens |
| Mémoire GPU | Vulkan Memory Allocator | Évite d'écrire un allocateur |
| Shaders | GLSL compilé en SPIR-V au build avec glslc | Aucune compilation de shader au lancement |
| Maths | glm | Suffisant, à remplacer seulement si le profileur le demande |
| Réseau | Sockets UDP POSIX | Même code sur Android et Linux, aucune dépendance |
| Son | Flux audio SDL3 et mixeur maison | Peu de voix simultanées, pas besoin de plus |
| Profilage | Tracy, RenderDoc, Android GPU Inspector | CPU, passes GPU sur Linux, compteurs Mali sur le Pixel |

Vérifié dans la documentation SDL3 : Android SDK 36, NDK r28c ou plus, niveau d'API minimal 21, intégration CMake par le dossier `android-project`, création de surface Vulkan par `SDL_Vulkan_CreateSurface` avec une fenêtre `SDL_WINDOW_VULKAN`. Le jeu se compile en bibliothèque partagée nommée `main` sur Android.

### Pourquoi Vulkan en direct

Avec moins de cent appels de dessin par image, le coût CPU de l'API compte peu. Le temps d'image se joue sur le GPU, dans le remplissage de pixels du bloom et du reflet. Vulkan donne la main sur ce qui pèse sur un GPU mobile à tuiles comme le Mali-G710 :

- choisir pour chaque attachement s'il est chargé et conservé, ce qui évite des copies entre la mémoire des tuiles et la mémoire principale
- garder le MSAA 4x et la profondeur en mémoire transitoire, jamais écrits en mémoire principale
- appliquer la rotation de l'écran dans le jeu, ce qui retire une passe de composition au système sur un jeu en paysage
- construire tous les pipelines au chargement et les mettre en cache sur disque, donc aucune saccade en course

Le prix est du code d'initialisation plus long. L'API GPU de SDL3 serait plus courte à écrire et repose aussi sur Vulkan sur ces deux plateformes. Je la garde comme solution de repli si le socle Vulkan dépasse la durée prévue en phase 0.

### Ce que le moteur ne contient pas

Pas d'éditeur de scène, pas de physique généraliste, pas d'éclairage PBR, pas d'ombres, pas d'animation squelettique, pas de langage de script. Chaque absence est volontaire.

## Architecture du rendu

Une image se dessine en quatre passes.

1. **Reflet.** Les objets émissifs sont redessinés sous la piste, dans une cible à demi-résolution. Les objets vivent en coordonnées de piste : distance le long de la spline, décalage latéral, hauteur. Le reflet est donc le même objet avec la hauteur inversée. Cela fonctionne dans les virages et les dévers, là où un miroir plan échouerait.
2. **Scène.** Ciel, ville, piste, rails, vaisseaux, traînées, dans une cible HDR en `B10G11R11_UFLOAT` avec MSAA 4x. Le shader de la piste lit la cible du reflet, la floute selon la rugosité et la dose avec un terme de Fresnel.
3. **Bloom.** Chaîne de réductions puis d'agrandissements, du demi au trente-deuxième de résolution, avec un filtre double.
4. **Composition.** Tone mapping, ajout du bloom, puis HUD à la résolution native, directement dans l'image de la swapchain.

Règles de performance appliquées dès la première ligne :

- La 3D est rendue à une échelle dynamique, entre 0,6 et 1,0 de la résolution native. L'écran du Pixel fait 3120 par 1440, la résolution native complète est inutile pour une image aussi floue de vitesse.
- La piste, les rails et la ville sont dessinés par instances.
- Les tampons d'uniformes restent mappés en permanence. Aucune allocation pendant une course.
- Le texte du HUD utilise un atlas de champs de distance, un seul appel de dessin pour tout le HUD.
- La ville est un panorama pré-rendu plus quelques couches de boîtes émissives instanciées. La brume de distance est calculée dans le shader.
- Textures compressées en ASTC sur Android et en BC7 sur Linux, produites par le pipeline d'assets.

Budgets à tenir sur le Pixel 7 Pro, à mesurer et non à supposer :

| Mesure | Budget |
|---|---|
| Temps GPU par image | 8 ms, ce qui laisse 120 Hz atteignable |
| Temps CPU par image | 4 ms |
| Appels de dessin | 100 |
| Triangles | 150 000, reflet compris |
| Chargement d'un circuit | 2 s |
| Session de 20 minutes | pas de chute d'images par échauffement |

Le Tensor G2 chauffe vite. Le test de 20 minutes compte autant que le test de pointe.

## Simulation

- Pas fixe à 60 Hz. Le rendu interpole entre deux états, donc la simulation ne dépend pas de la fréquence de l'écran.
- Véhicules cinématiques sur la spline. Collisions par tests de segments, rebond mou entre vaisseaux.
- État de jeu en tableaux plats, copiable d'un bloc. La prédiction réseau en dépend : il faut pouvoir sauvegarder et rejouer un état.
- La simulation n'appelle ni le rendu ni SDL. Elle se compile et se teste seule sur Linux.

## Réseau

- L'hôte fait autorité. Les clients prédisent leur propre vaisseau et se réconcilient avec les snapshots.
- Simulation à 60 Hz, snapshots à 20 ou 30 Hz, adversaires interpolés.
- Missiles, boucliers et collisions entre joueurs sont décidés par l'hôte seul.
- Découverte par broadcast UDP. Sur Android, cela demande un `MulticastLock`, obtenu par un appel JNI.
- Saisie manuelle de l'adresse IP en secours, pour les routeurs qui isolent les clients.
- Pas de lockstep déterministe. Les flottants diffèrent entre ARM et x86.

## Phases

### Phase 0, socle : 2 à 3 semaines

- Dépôt, CMake, build Linux et build Android depuis le même arbre.
- Un triangle Vulkan sur Linux avec les couches de validation, puis sur le Pixel.
- Boucle principale, pause et reprise de l'application sur Android, recréation de la swapchain.
- Tracy et RenderDoc branchés.

Critère de sortie : la même image s'affiche sur les deux cibles, et l'application survit à dix mises en arrière-plan.

### Phase 1, prototype de rendu : 4 à 6 semaines

C'est la phase qui décide du projet.

- Piste générée depuis une spline, rails émissifs.
- Un vaisseau provisoire, traînées de réacteur.
- Les quatre passes : reflet, scène HDR, bloom, composition.
- Ciel et ville.
- HUD statique à l'image du mockup.
- Caméra qui suit la spline à vitesse constante.

Critère de sortie : une capture d'écran posée à côté du mockup tient la comparaison, et les budgets du tableau sont respectés sur le Pixel. Si le rendu ne tient pas, on le règle ici, pas après avoir écrit le jeu.

### Phase 2, course solo : 4 à 6 semaines

- Contrôles tactiles, clavier et manette.
- Modèle de vol, boost et jauge d'énergie.
- Tours, positions, ligne d'arrivée, minimap.
- Trois adversaires pilotés par une IA simple qui suit la spline.
- Son : moteur, boost, musique.

Critère de sortie : une course de trois tours contre l'IA donne envie d'en refaire une.

### Phase 3, multijoueur local : 5 à 7 semaines

- Découverte, lobby minimal, connexion à deux puis à quatre.
- Prédiction, réconciliation, interpolation.
- Outil de simulation de latence et de perte de paquets sur Linux, pour tester sans deux téléphones.
- Tests sur un routeur domestique et sur un téléphone en partage de connexion.

Critère de sortie : deux téléphones et un PC Linux finissent la même course avec le même classement.

### Phase 4, contenu : 6 à 10 semaines

- Missile et bouclier, puis d'autres objets jusqu'à six au plus.
- Distribution des objets pondérée par la position.
- Trois circuits, quatre vaisseaux.
- Menus, réglages, reconnexion, IA de remplissage.

### Phase 5, finition et sortie : 4 à 6 semaines

- Équilibrage à partir de statistiques enregistrées en local.
- Tests sur d'autres téléphones que le Pixel.
- Paquet Google Play, AppImage ou Flatpak pour Linux.

## Arborescence prévue

```
antigrav/
  CMakeLists.txt
  src/
    platform/   fenêtre, entrées, son, cycle de vie
    gfx/        Vulkan, passes, pipelines
    sim/        piste, véhicules, objets, sans dépendance au rendu
    net/        découverte, protocole, prédiction
    game/       états de jeu, HUD, menus
  shaders/      GLSL, compilé en SPIR-V au build
  assets/       sources des modèles, textures, sons
  tools/        conversion d'assets, simulateur de latence
  android/      copie de l'android-project de SDL3
  third_party/  SDL3, volk, VMA, glm, Tracy
```

## Risques

1. **Le socle Vulkan prend plus longtemps que prévu.** Repli sur l'API GPU de SDL3, décidé à la fin de la phase 0.
2. **Le rendu atteint 60 Hz mais pas 120.** Acceptable pour la v1. L'échelle de résolution dynamique sert de soupape.
3. **L'échauffement fait chuter les images en longue session.** Mesuré dès la phase 1, corrigé par une échelle de résolution pilotée par l'état thermique.
4. **Les modèles 3D.** Le mockup montre des vaisseaux détaillés. Sans graphiste, il faudra des formes plus simples ou des modèles achetés.
5. **Wi-Fi Android.** Broadcasts filtrés ou clients isolés selon les routeurs, d'où la saisie d'adresse IP.
6. **Pilotes Vulkan des autres téléphones.** Tout est validé sur un seul appareil jusqu'à la phase 5.

## Décisions ouvertes

- Le langage. Le plan suppose C++20. Rust ou Zig donneraient les mêmes performances, avec un outillage Android moins direct.
- La taille de l'équipe et le temps disponible, qui fixent les durées.
- L'origine des modèles 3D et de la musique.
- Un téléphone bas de gamme de référence, si la v1 doit tourner ailleurs que sur du haut de gamme.
- Le niveau d'API Android minimal, à fixer en phase 0.
- Le découpage de la piste en tronçons. Aujourd'hui tout le circuit est dessiné à chaque image, soit environ 70 000 triangles pour les murets. C'est sans effet mesurable sur le GPU Intel, à revoir si le Pixel le demande.

## Outillage de capture

Le binaire accepte un mode sans fenêtre qui rend un nombre donné d'images dans une cible hors écran et écrit une capture PNG. Il sert à comparer le rendu au mockup, à produire des vidéos avec ffmpeg et à tester le rendu sans écran.
