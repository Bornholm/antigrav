# Environnement de compilation croisée vers Windows, le même que celui de la
# CI : Ubuntu 24.04 et MinGW-w64. Wine sert à lancer les tests, et le pilote
# Vulkan logiciel de Mesa à faire une capture du jeu sans carte graphique.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates cmake ninja-build git glslc zip \
      g++-mingw-w64-x86-64-posix wine64 libvulkan1 mesa-vulkan-drivers \
 && rm -rf /var/lib/apt/lists/*
# Écran virtuel : Wine ne charge Vulkan qu'avec un pilote d'affichage.
RUN apt-get update \
 && apt-get install -y --no-install-recommends xvfb xauth \
 && rm -rf /var/lib/apt/lists/*
ENV WINEDEBUG=-all
WORKDIR /src
