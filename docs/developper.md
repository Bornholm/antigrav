# Développer

## Tests

```sh
ctest --test-dir build
```

Quatre suites : `music` pour l'analyse audio sur des signaux synthétiques, `sim` pour la simulation, la génération des pistes et les règles de rythme, `net` pour le réseau en boucle locale, y compris avec 25 % de paquets perdus, et `balance`.

Cette dernière joue 864 courses entre IA sur les pistes des morceaux livrés et échoue si un vaisseau ou une personnalité gagne plus de 40 % ou moins de 10 % des courses.

Essai réseau entre deux instances sur la même machine, chacune filmée :

```sh
./build/soundwave --headless --screen salon --host-auto 2 --frames 540 --fps 30 --realtime --out "out/h/f_%04d.png" &
./build/soundwave --headless --screen multi --join-auto --frames 510 --fps 30 --realtime --out "out/c/f_%04d.png"
```

## Analyse d'un morceau

`build/analyze` lit un fichier MP3, Ogg Vorbis, FLAC ou WAV et affiche ce que le jeu en tire : tempo, temps, attaques, sections.

```sh
./build/analyze assets/music/neon.ogg
./build/analyze morceau.wav --clicks clics.wav --image analyse.png
./build/analyze morceau.wav --image zoom.png --from 30 --to 50
```

`--clicks` écrit le morceau avec un clic sur chaque temps détecté, plus aigu sur le premier temps de la mesure. C'est le moyen de juger l'analyse à l'oreille. `--image` dessine, de haut en bas, les sections et les temps, le spectrogramme, les attaques dans les aigus, les médiums et les graves, puis la courbe d'énergie. `--beats` liste les temps.

## Captures et vidéos

Le mode sans fenêtre avance la course d'un pas fixe, donc deux exécutions produisent les mêmes images.

```sh
./build/soundwave --headless --frames 1 --time 14 --out out/capture.png
./build/soundwave --headless --frames 300 --size 1280x720 --out "out/frames/f_%04d.png"
ffmpeg -framerate 60 -i out/frames/f_%04d.png -c:v libx264 -pix_fmt yuv420p out/course.mp4
```

## Validation Vulkan

`tools/run.sh` lance le jeu avec les couches de validation. Si le système ne les fournit pas, le script utilise celles extraites dans `.cache/vk-layers/usr`, par exemple depuis le paquet `vulkan-validation-layers` de la distribution.

## Organisation du code

```
src/core      journalisation
src/gfx       Vulkan : périphérique, swapchain, pipelines, passes de rendu, interface 2D
src/music     décodage et analyse des morceaux : tempo, temps, attaques, sections
src/sim       circuits, vaisseaux, IA et simulation, sans dépendance au rendu
src/net       réseau local : découverte, salon, instantanés, prédiction
src/game      menus, course, commandes, caméra, son, interface
tools         lanceur avec validation, outil d'équilibrage, analyse d'un morceau
tests         tests de la simulation et du réseau
shaders       GLSL, compilé en SPIR-V et inclus dans le binaire
assets/fonts  police Orbitron, licence OFL
assets/music  musique des menus et des circuits, en Ogg Vorbis
android       projet Gradle pour Android
docs          documentation
```
