#version 450
#include "common.glsl"
#include "sky.glsl"

// Murets et rails de la piste. kind 0 : bande lumineuse. kind 1 : corps sombre.
layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 color;
layout(location = 4) flat in float kind;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 c;
    if (kind < 0.5) {
        // Les néons battent avec la musique.
        c = color.rgb * color.a * (1.0 + 0.7 * g.screen.w);
    } else {
        // Panneaux sombres avec un liseré tous les huit mètres.
        float seam = smoothstep(0.485, 0.5, abs(fract(uv.y / 8.0) - 0.5));
        c = vec3(0.004, 0.005, 0.010) + color.rgb * (0.03 * uv.x * uv.x + seam * 0.5);
    }
    outColor = vec4(applyFog(c, worldPos), 1.0);
}
