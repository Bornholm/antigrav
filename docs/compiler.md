# Compiler

## Linux

Il faut clang, CMake, Ninja, SDL3, glslc et un pilote Vulkan 1.1 ou plus récent. Les autres dépendances sont téléchargées à la configuration.

```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
./build/soundwave
```

Si la distribution ne fournit pas SDL3, ajouter `-DSOUNDWAVE_BUNDLED_SDL=ON` : CMake télécharge SDL et le lie en statique dans l'exécutable.

## Android

Il faut le SDK Android avec la plateforme 36, le NDK 27.2.12479018, CMake 3.22.1 du SDK et un JDK 17.

```sh
echo "sdk.dir=$HOME/Android/Sdk" > android/local.properties
cd android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk gradle assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

La version de debug s'appelle « Soundwave Debug » et porte l'identifiant `dev.soundwave.game.debug`. Elle s'installe donc à côté de la version publiée, sans la remplacer.

SDL est compilé avec le jeu. Pour éviter de le retélécharger, placer l'archive `SDL3-3.4.16.tar.gz` dans `.cache/sdl/`.

Pour signer l'APK avec une clé fixe, voir [Publier](publier.md).
