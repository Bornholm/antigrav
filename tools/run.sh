#!/bin/sh
# Lance le jeu avec les couches de validation Vulkan extraites dans .cache,
# quand le système ne les fournit pas. Exemple :
#   tools/run.sh --headless --frames 1 --out out/shot.png
root=$(cd "$(dirname "$0")/.." && pwd)
layers="$root/.cache/vk-layers/usr"
if [ -d "$layers" ]; then
    export VK_ADD_LAYER_PATH="$layers/share/vulkan/explicit_layer.d"
    export LD_LIBRARY_PATH="$layers/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
exec "$root/build/soundwave" --validation "$@"
