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
        c = color.rgb * color.a;
    } else {
        // Panneaux sombres avec un liseré tous les huit mètres.
        float seam = 1.0 - smoothstep(0.0, 0.06, abs(fract(uv.y / 8.0) - 0.5) - 0.47);
        c = vec3(0.006, 0.007, 0.014) + color.rgb * 0.05 * uv.x + color.rgb * seam * 0.25;
    }
    outColor = vec4(applyFog(c, worldPos), 1.0);
}
