#version 450
#include "common.glsl"

// Effets dessinés en additif. kind 0 : flamme de réacteur, uv.x en travers,
// uv.y de la tuyère vers la queue. kind 1 : bulle de bouclier. kind 2 : orbe d'effet.
layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 color;
layout(location = 4) flat in float kind;

layout(location = 0) out vec4 outColor;

void main() {
    float visible = exp(-length(worldPos - g.camPos.xyz) * g.fog.a);

    if (pc.colorC.a < -0.5) {
        // Fantôme : la coque d'un vaisseau en hologramme. Contour lumineux,
        // liserés et réacteurs plus vifs, balayé de lignes qui défilent.
        // colorA porte teinte et intensité.
        vec3 n = normalize(normal);
        vec3 v = normalize(g.camPos.xyz - worldPos);
        float rim = pow(clamp(1.0 - abs(dot(n, v)), 0.0, 1.0), 1.5);
        float scan = 0.55 + 0.45 * smoothstep(0.3, 0.5, abs(fract(worldPos.y * 2.2 - g.camPos.w * 1.5) - 0.5));
        float edge = kind > 0.5 ? 0.9 : 0.0;
        vec3 c = pc.colorA.rgb * pc.colorA.a * (0.05 + 0.55 * rim + edge) * scan;
        outColor = vec4(safe(c * visible), 1.0);
        return;
    }
    if (kind > 1.5) {
        // Orbe d'effet : cœur diffus, contour vif. colorB porte teinte et intensité.
        vec3 n = normalize(normal);
        vec3 v = normalize(g.camPos.xyz - worldPos);
        // Borné : hors de [0, 1], pow renvoie un NaN que le bloom étale.
        float facing = clamp(abs(dot(n, v)), 0.0, 1.0);
        float rim = pow(1.0 - facing, 2.0);
        // Un orbe tout proche de la caméra s'efface, pour ne pas blanchir l'écran.
        float near = smoothstep(4.0, 45.0, length(worldPos - g.camPos.xyz));
        vec3 c = pc.colorB.rgb * pc.colorB.a * (0.10 * facing + 1.0 * rim) * near;
        outColor = vec4(safe(c * visible), 1.0);
        return;
    }
    if (kind > 0.5) {
        // Bulle de bouclier : brillante sur son contour, avec une trame qui défile.
        vec3 n = normalize(normal);
        vec3 v = normalize(g.camPos.xyz - worldPos);
        float rim = pow(clamp(1.0 - abs(dot(n, v)), 0.0, 1.0), 2.2);
        float grid = max(smoothstep(0.42, 0.5, abs(fract(uv.x * 20.0) - 0.5)),
                         smoothstep(0.42, 0.5, abs(fract(uv.y * 10.0 + g.camPos.w * 0.6) - 0.5)));
        vec3 c = pc.colorB.rgb * (rim * 2.2 + grid * 0.35 + 0.05) * pc.colorB.a;
        outColor = vec4(safe(c * visible), 1.0);
        return;
    }

    // Bornés : en MSAA, l'interpolation peut sortir de [0, 1] sur les bords, et
    // pow d'un nombre négatif donne un NaN que le bloom étale ensuite.
    float across = clamp(1.0 - abs(uv.x * 2.0 - 1.0), 0.0, 1.0);
    float along = clamp(1.0 - uv.y, 0.0, 1.0);
    float flicker = 0.85 + 0.15 * sin(g.camPos.w * 40.0 + uv.y * 30.0);
    float core = pow(across, 3.0) * pow(along, 2.2);
    vec3 c = mix(pc.colorB.rgb, vec3(1.0), pow(across, 10.0) * along * 0.6) * core * pc.colorB.a *
             flicker * 0.30;
    outColor = vec4(safe(c * visible), 1.0);
}
