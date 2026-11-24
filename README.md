# Antigrav

Jeu de course antigravité néon pour Android et Linux, sur un moteur maison en C++20 et Vulkan 1.3. Le plan de développement est dans [PLAN.md](PLAN.md).

## Compiler sur Linux

Il faut clang, CMake, Ninja, SDL3, glslc et un pilote Vulkan 1.3. Les autres dépendances sont téléchargées à la configuration.

```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
./build/antigrav
```

## Jouer

Le jeu s'ouvre sur le menu. **Solo** règle le circuit, le vaisseau, le nombre d'IA de 0 à 7, leur caractère et le nombre de tours. **Multijoueur** héberge une partie ou rejoint celles trouvées sur le réseau local.

La piste porte trois sortes d'éléments. Les plaques à chevrons jaunes accélèrent, les champs hachurés rouges freinent et vident la jauge, les barrières rouges arrêtent net. Le bouclier protège des champs et encaisse une barrière.

Pour jouer à plusieurs, tous les appareils doivent être sur le même réseau Wi-Fi, ou sur le partage de connexion de l'un d'eux. L'hôte choisit le circuit, les IA et lance la course. Si la découverte automatique ne trouve rien, saisir l'adresse IP de l'hôte. Le jeu utilise le port UDP 47800.

| Action | Clavier | Manette | Écran |
|---|---|---|---|
| Tourner | flèches, ou A et D | stick gauche ou croix | boutons en bas à gauche |
| Boost | espace, flèche haut ou W | A ou gâchette droite | bouton BOOST |
| Missile | E | X | bouton rose |
| Bouclier | Q | B | bouton bleu |
| Menus | flèches, Entrée, Échap | croix, A, B | toucher |

La souris agit comme un doigt. Pendant une course, le bouton MENU, Échap ou le geste retour d'Android proposent de quitter la course.

## Options

| Option | Effet |
|---|---|
| `--race` | démarre directement une course, sans menu |
| `--track N`, `--ship N` | circuit de 0 à 2, vaisseau de 0 à 3 |
| `--autopilot` | l'ordinateur pilote aussi le vaisseau du joueur |
| `--grid-start`, `--rolling-start` | départ arrêté ou lancé |
| `--size LxH` | taille de la fenêtre ou de l'image |
| `--scale N` | échelle de la 3D par rapport à la sortie, 0.7 par exemple |
| `--stats` | affiche les temps GPU et le nombre d'images par seconde |
| `--no-vsync` | ne pas attendre l'écran, pour mesurer |
| `--no-bloom`, `--no-reflections`, `--no-hud`, `--no-audio` | couper un effet |
| `--validation`, `--no-validation` | couches de validation Vulkan |
| `--headless` | rendre sans fenêtre |
| `--frames N` | s'arrêter après N images |
| `--time S` | démarrer à S secondes, mode sans fenêtre |
| `--out chemin.png` | écrire la dernière image ; avec `%04d` dans le chemin, toutes les images |
| `--wav chemin.wav` | écrire le son de la course, mode sans fenêtre |
| `--screen nom` | mode sans fenêtre : capturer un écran de menu, `main`, `solo`, `multi`, `adresse` ou `salon` |
| `--host-auto N`, `--join-auto`, `--realtime` | essai réseau sans interface, voir plus bas |

## Tests

```sh
ctest --test-dir build
```

Trois suites : `sim` pour la simulation, `net` pour le réseau en boucle locale, y compris avec 25 % de paquets perdus, et `balance` qui échoue si un vaisseau ou une personnalité d'IA domine.

Essai réseau entre deux instances sur la même machine, chacune filmée :

```sh
./build/antigrav --headless --screen salon --host-auto 2 --frames 540 --fps 30 --realtime --out "out/h/f_%04d.png" &
./build/antigrav --headless --screen multi --join-auto --frames 510 --fps 30 --realtime --out "out/c/f_%04d.png"
```

## Captures et vidéos

Le mode sans fenêtre avance la course d'un pas fixe, donc deux exécutions produisent les mêmes images.

```sh
./build/antigrav --headless --frames 1 --time 14 --out out/capture.png
./build/antigrav --headless --frames 300 --size 1280x720 --out "out/frames/f_%04d.png"
ffmpeg -framerate 60 -i out/frames/f_%04d.png -c:v libx264 -pix_fmt yuv420p out/course.mp4
```

## Validation Vulkan

`tools/run.sh` lance le jeu avec les couches de validation. Si le système ne les fournit pas, le script utilise celles extraites dans `.cache/vk-layers/usr`, par exemple depuis le paquet `vulkan-validation-layers` de la distribution.

## Organisation

```
src/core      journalisation
src/gfx       Vulkan : périphérique, swapchain, pipelines, passes de rendu, interface 2D
src/sim       circuits, vaisseaux, IA et simulation, sans dépendance au rendu
src/net       réseau local : découverte, salon, instantanés, prédiction
src/game      menus, course, commandes, caméra, son, interface
tools         lanceur avec validation, outil d'équilibrage
tests         tests de la simulation et du réseau
shaders       GLSL, compilé en SPIR-V et inclus dans le binaire
assets/fonts  police Orbitron, licence OFL
android       projet Gradle pour Android
```

## Compiler pour Android

Il faut le SDK Android avec la plateforme 36, le NDK 27.2.12479018, CMake 3.22.1 du SDK et un JDK 17.

```sh
echo "sdk.dir=$HOME/Android/Sdk" > android/local.properties
cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk gradle assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

SDL est compilé avec le jeu. Pour éviter de le retélécharger, placer l'archive `SDL3-3.4.16.tar.gz` dans `.cache/sdl/`.
