#version 450
#include "common.glsl"
#include "sky.glsl"

// Décor. kind de 0 à 999 : façade de tour, kind sert de graine aléatoire et
// color.a porte la hauteur. kind < 0 : panneau lumineux. kind >= 1000 : roche.
layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv; // mètres le long de la façade, mètres de hauteur
layout(location = 3) in vec4 color;
layout(location = 4) flat in float kind;

layout(location = 0) out vec4 outColor;

void main() {
    if (dot(g.mirrorClip.xyz, worldPos) + g.mirrorClip.w < 0.0) discard;
    vec3 c;
    if (kind >= 1000.0) {
        // Roche : aplats sombres éclairés par le soleil, strates incandescentes.
        vec3 n = normalize(normal);
        vec3 v = normalize(g.camPos.xyz - worldPos);
        vec3 sunDir = normalize(vec3(0.12, 0.16, -1.0));
        float lit = max(dot(n, sunDir), 0.0);
        float seed = kind - 1000.0;
        float strata = floor(uv.y / 9.0);
        float glow = step(0.78, hash11(strata * 1.7 + seed)) *
                     (1.0 - smoothstep(0.0, 0.9, abs(fract(uv.y / 9.0) - 0.5) * 9.0));
        c = vec3(0.030, 0.014, 0.012) * (0.35 + 0.65 * hash11(strata + seed * 3.0));
        c += g.skyHaze.rgb * lit * 0.16;
        c += CYAN * glow * 1.6;
        c += g.skyHaze.rgb * pow(1.0 - max(dot(n, v), 0.0), 3.0) * 0.10;
    } else if (kind < 0.0) {
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
    outColor = vec4(safe(applyFog(c, worldPos)) * g.mirrorFade.x, 1.0);
}
