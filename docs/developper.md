# Développer

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

## Organisation du code

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
assets/music  musique des menus et des circuits, en Ogg Vorbis
android       projet Gradle pour Android
docs          documentation
```
