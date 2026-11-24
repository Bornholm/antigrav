# Antigrav

Jeu de course antigravité néon pour Android et Linux, sur un moteur maison en C++20 et Vulkan 1.3. Le plan de développement est dans [PLAN.md](PLAN.md).

## Compiler sur Linux

Il faut clang, CMake, Ninja, SDL3, glslc et un pilote Vulkan 1.3. Les autres dépendances sont téléchargées à la configuration.

```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
./build/antigrav
```

## Commandes

| Action | Clavier | Écran |
|---|---|---|
| Tourner | flèches gauche et droite, ou A et D | boutons en bas à gauche |
| Boost | espace, flèche haut ou W | bouton BOOST |
| Quitter | Échap | |

La souris agit comme un doigt sur les boutons.

## Options

| Option | Effet |
|---|---|
| `--autopilot` | l'ordinateur pilote aussi le vaisseau du joueur |
| `--size LxH` | taille de la fenêtre ou de l'image |
| `--scale N` | échelle de la 3D par rapport à la sortie, 0.7 par exemple |
| `--no-vsync` | ne pas attendre l'écran, pour mesurer |
| `--no-bloom`, `--no-reflections`, `--no-hud` | couper un effet |
| `--validation`, `--no-validation` | couches de validation Vulkan |
| `--headless` | rendre sans fenêtre |
| `--frames N` | s'arrêter après N images |
| `--time S` | démarrer la course à S secondes, mode sans fenêtre |
| `--out chemin.png` | écrire la dernière image ; avec `%04d` dans le chemin, toutes les images |

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
src/sim       circuit et simulation des vaisseaux, sans dépendance au rendu
src/game      course, commandes, caméra, mise en page de l'interface
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
