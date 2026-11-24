#version 450
#include "common.glsl"
#include "sky.glsl"

// Surface de la piste : asphalte sombre et mouillé, marquages lumineux, et
// reflet lu dans la cible où la scène a été dessinée en miroir.
layout(set = 1, binding = 0) uniform sampler2D reflectionTex;

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv; // x décalage latéral en mètres, y distance en mètres
layout(location = 3) in vec4 color; // x demi-largeur de la piste, y longueur du circuit
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

    // Vu par en dessous, quand une branche du circuit enjambe l'autre : tablier
    // sombre, avec le liseré de bord pour le deviner.
    if (dot(n, v) < 0.0) {
        float rim = 1.0 - smoothstep(0.0, 1.6, halfWidth - ax);
        vec3 under = vec3(0.004, 0.005, 0.010) + CYAN * rim * 1.5 +
                     MAGENTA * step(0.92, fract(uv.y / 40.0)) * 0.6;
        outColor = vec4(applyFog(under, worldPos), 1.0);
        return;
    }
    float nv = clamp(dot(n, v), 0.0, 1.0);

    // Plaques plus ou moins humides, étirées dans le sens de la piste.
    vec2 wp = vec2(x * 0.9, uv.y * 0.05);
    vec2 wi = floor(wp), wf = fract(wp);
    wf = wf * wf * (3.0 - 2.0 * wf);
    float wet = mix(mix(hash21(wi), hash21(wi + vec2(1.0, 0.0)), wf.x),
                    mix(hash21(wi + vec2(0.0, 1.0)), hash21(wi + vec2(1.0, 1.0)), wf.x), wf.y);
    wet = mix(0.70, 1.0, wet);

    // Reflet : plusieurs lectures étalées verticalement à l'écran, ce qui donne
    // les traînées allongées d'une chaussée mouillée.
    vec2 suv = gl_FragCoord.xy * g.screen.xy;
    float spread = mix(0.010, 0.060, 1.0 - nv) * (0.6 + 0.4 * wet);
    // Décalage aléatoire par pixel : cinq lectures régulières dessineraient
    // cinq copies décalées du reflet, le bruit les fond en un flou.
    float jitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    vec3 refl = vec3(0.0);
    float total = 0.0;
    for (int i = 0; i < 5; i++) {
        float t = (float(i) + jitter) / 5.0;
        float w = 1.0 - 0.7 * t;
        refl += texture(reflectionTex, suv + vec2(0.0, -spread * t)).rgb * w;
        total += w;
    }
    refl /= total;
    float fresnel = 0.05 + 0.95 * pow(1.0 - nv, 3.0);
    vec3 c = vec3(0.003, 0.004, 0.008) + refl * fresnel * wet * 1.1;

    // Marquages émissifs.
    float edge = lineMask(ax, halfWidth - 0.55, 0.22);
    float inner = lineMask(ax, halfWidth - 2.6, 0.07);
    // Tirets longs : à 340 m/s, un motif court clignoterait d'une image à l'autre.
    float dash = step(fract(uv.y / 96.0), 0.55);
    float lane = lineMask(ax, halfWidth / 3.0, 0.05) * dash;
    float center = lineMask(ax, 0.0, 0.04) * step(fract(uv.y / 48.0), 0.4);

    // Filets lumineux épars dans le sens de la marche : ils donnent la vitesse.
    float band = floor(x * 1.3);
    float pick = hash11(band + 17.0);
    float along = fract(uv.y / 150.0 + pick * 7.0);
    float streak = step(0.80, pick) * lineMask(fract(x * 1.3), 0.5, 0.035 * 1.3) *
                   smoothstep(0.0, 0.15, along) * smoothstep(0.55, 0.35, along);
    vec3 streakColor = mix(CYAN, MAGENTA, step(0.5, hash11(band * 3.1)));

    c += CYAN * edge * 7.0;
    c += MAGENTA * inner * 4.0;
    c += MAGENTA * lane * 0.9;
    c += vec3(0.5, 0.6, 1.0) * center * 0.25;
    c += streakColor * streak * 0.55;

    // Ligne de départ : damier sur quatre mètres.
    float toLine = min(abs(uv.y), abs(color.y - uv.y));
    if (toLine < 2.0 && ax < halfWidth - 1.0) {
        vec2 cell = floor(vec2(x, uv.y) / 1.0);
        c += vec3(1.0, 0.95, 0.85) * mod(cell.x + cell.y, 2.0) * 1.6;
    }

    // Halo des bandes de bord sur l'asphalte.
    c += CYAN * exp(-max(halfWidth - 0.55 - ax, 0.0) * 1.1) * 0.10;

    outColor = vec4(applyFog(c, worldPos), 1.0);
}
