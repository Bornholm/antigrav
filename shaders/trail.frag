#version 450
#include "common.glsl"

// Flamme de réacteur, dessinée en additif. uv.x en travers, uv.y de la
// tuyère vers la queue.
layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 color;
layout(location = 4) flat in float kind;

layout(location = 0) out vec4 outColor;

void main() {
    // Bornés : en MSAA, l'interpolation peut sortir de [0, 1] sur les bords, et
    // pow d'un nombre négatif donne un NaN que le bloom étale ensuite.
    float across = clamp(1.0 - abs(uv.x * 2.0 - 1.0), 0.0, 1.0);
    float along = clamp(1.0 - uv.y, 0.0, 1.0);
    float flicker = 0.85 + 0.15 * sin(g.camPos.w * 40.0 + uv.y * 30.0);
    float core = pow(across, 3.0) * pow(along, 2.2);
    vec3 c = mix(pc.colorB.rgb, vec3(1.0), pow(across, 8.0) * along) * core * pc.colorB.a *
             flicker;
    float visible = exp(-length(worldPos - g.camPos.xyz) * g.fog.a);
    outColor = vec4(c * visible, 1.0);
}
