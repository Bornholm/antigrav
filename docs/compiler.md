# Compiler

## Linux

Il faut clang, CMake, Ninja, SDL3, glslc et un pilote Vulkan 1.1 ou plus récent. Les autres dépendances sont téléchargées à la configuration.

```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
./build/soundwave
```

Si la distribution ne fournit pas SDL3, ajouter `-DSOUNDWAVE_BUNDLED_SDL=ON` : CMake télécharge SDL et le lie en statique dans l'exécutable.

## Windows

La version Windows se compile depuis Linux, avec MinGW-w64. Sur Ubuntu 24.04, il faut les paquets `cmake`, `ninja-build`, `glslc` et `g++-mingw-w64-x86-64-posix`, plus `wine64` pour lancer les tests.

```sh
cmake -S . -B build-windows -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows
ctest --test-dir build-windows
```

Le résultat est `build-windows/soundwave.exe`, un exécutable seul : SDL et la bibliothèque standard y sont liés en statique. Il lui faut Windows 10 ou plus récent, en 64 bits, et un pilote Vulkan 1.1.

Sur une autre distribution, `tools/windows/Dockerfile` reproduit cet environnement :

```sh
docker build -t soundwave-mingw tools/windows
docker run --rm -u $(id -u):$(id -g) -v $PWD:/src \
  -e HOME=/src/build-windows/home -e WINEPREFIX=/src/build-windows/home/wine \
  soundwave-mingw bash -c 'mkdir -p $HOME && cmake -S . -B build-windows -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build-windows && ctest --test-dir build-windows'
```

Les sockets passent par `src/net/sock.h`, qui donne les mêmes appels sur POSIX et sur Winsock.

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
