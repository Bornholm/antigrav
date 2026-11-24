#version 450
#include "common.glsl"
#include "sky.glsl"

// Ville. kind >= 0 : façade, kind sert de graine aléatoire et color.a porte
// la hauteur de la tour. kind < 0 : panneau lumineux.
layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv; // mètres le long de la façade, mètres de hauteur
layout(location = 3) in vec4 color;
layout(location = 4) flat in float kind;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 c;
    if (kind < 0.0) {
        float scan = 0.75 + 0.25 * sin(uv.y * 2.4);
        c = color.rgb * color.a * scan;
    } else {
        float height = color.a;
        vec2 cellSize = vec2(7.0, 5.0);
        vec2 cell = floor(uv / cellSize);
        vec2 f = fract(uv / cellSize);
        float lit = step(0.80, hash21(cell + kind * 31.0));
        float window = step(0.15, f.x) * step(f.x, 0.85) * step(0.25, f.y) * step(f.y, 0.75);
        vec3 tint = mix(CYAN, MAGENTA, step(0.5, hash21(cell.yx + kind)));
        c = vec3(0.004, 0.004, 0.012);
        c += tint * lit * window * 0.55;
        // Bandeau lumineux au sommet et arête verticale.
        float crown = smoothstep(height - 5.0, height - 3.5, uv.y);
        c += color.rgb * crown * 3.0;
        c += color.rgb * step(uv.x, 0.9) * 1.6 * step(0.5, hash11(kind * 7.0));
    }
    outColor = vec4(applyFog(c, worldPos), 1.0);
}
