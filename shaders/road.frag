#version 450
#include "common.glsl"
#include "sky.glsl"

// Surface de la piste : asphalte sombre et mouillé, marquages lumineux, et
// reflet lu dans la cible où la scène a été dessinée en miroir.
layout(set = 1, binding = 0) uniform sampler2D reflectionTex;

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv; // x décalage latéral en mètres, y distance en mètres
layout(location = 3) in vec4 color; // x demi-largeur de la piste
layout(location = 4) flat in float kind;

layout(location = 0) out vec4 outColor;

float lineMask(float x, float center, float halfWidth) {
    float d = abs(x - center) - halfWidth;
    float aa = fwidth(x);
    return 1.0 - smoothstep(-aa, aa, d);
}

void main() {
    float halfWidth = color.x;
    float x = uv.x;
    float ax = abs(x);

    vec3 n = normalize(normal);
    vec3 toCam = g.camPos.xyz - worldPos;
    float dist = length(toCam);
    vec3 v = toCam / dist;
    float nv = clamp(dot(n, v), 0.0, 1.0);

    // Plaques plus ou moins humides, étirées dans le sens de la piste.
    float wet = hash21(floor(vec2(x * 1.5, uv.y * 0.08)));
    wet = mix(0.75, 1.0, wet);

    // Reflet : plusieurs lectures étalées verticalement à l'écran, ce qui donne
    // les traînées allongées d'une chaussée mouillée.
    vec2 suv = gl_FragCoord.xy * g.screen.xy;
    float spread = mix(0.004, 0.030, 1.0 - nv) * (0.6 + 0.4 * wet);
    vec3 refl = vec3(0.0);
    const float weights[5] = float[](0.30, 0.25, 0.20, 0.15, 0.10);
    for (int i = 0; i < 5; i++) {
        float t = float(i) / 4.0;
        refl += texture(reflectionTex, suv + vec2(0.0, -spread * t)).rgb * weights[i];
    }
    float fresnel = 0.06 + 0.94 * pow(1.0 - nv, 4.0);
    vec3 c = vec3(0.004, 0.005, 0.010) + refl * fresnel * wet * 1.6;

    // Marquages émissifs.
    float edge = lineMask(ax, halfWidth - 0.55, 0.22);
    float inner = lineMask(ax, halfWidth - 2.6, 0.07);
    float dash = step(fract(uv.y / 16.0), 0.45);
    float lane = lineMask(ax, halfWidth / 3.0, 0.05) * dash;
    float center = lineMask(ax, 0.0, 0.04) * step(fract(uv.y / 8.0), 0.3);

    c += CYAN * edge * 7.0;
    c += MAGENTA * inner * 4.0;
    c += MAGENTA * lane * 0.9;
    c += vec3(0.5, 0.6, 1.0) * center * 0.25;

    // Halo des bandes de bord sur l'asphalte.
    c += CYAN * exp(-max(halfWidth - 0.55 - ax, 0.0) * 1.1) * 0.10;

    outColor = vec4(applyFog(c, worldPos), 1.0);
}
