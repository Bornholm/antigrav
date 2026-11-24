#version 450
#include "common.glsl"
#include "sky.glsl"

// Vaisseau. kind 0 : coque. kind 1 : liseré, couleur colorA. kind 2 : réacteur,
// couleur colorB.
layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 color;
layout(location = 4) flat in float kind;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 c;
    if (kind > 1.5) {
        // Tuyère : cœur blanc, bord coloré.
        float core = 1.0 - smoothstep(0.0, 0.9, length(uv * 2.0 - 1.0));
        c = mix(pc.colorB.rgb, vec3(1.0), core * 0.7) * pc.colorB.a * (0.6 + core);
    } else if (kind > 0.5) {
        c = pc.colorA.rgb * pc.colorA.a;
    } else {
        vec3 n = normalize(normal);
        vec3 v = normalize(g.camPos.xyz - worldPos);
        float nv = clamp(dot(n, v), 0.0, 1.0);
        vec3 env = skyColor(reflect(-v, n));
        float fresnel = 0.08 + 0.92 * pow(1.0 - nv, 4.0);
        // Coque sombre et brillante, éclairée par le ciel et par ses liserés.
        c = color.rgb * (0.020 + 0.05 * max(n.y, 0.0));
        c += env * fresnel * 1.4;
        c += pc.colorA.rgb * 0.020 * (1.0 - nv);
    }
    outColor = vec4(applyFog(c, worldPos), 1.0);
}
