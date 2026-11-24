#version 450
#include "common.glsl"

// Un seul format de sommet pour toute la scène.
// Géométrie de piste : pos est un point de la surface, normal l'axe haut local
// et height la hauteur au-dessus. Le reflet inverse la hauteur, ce qui reste
// exact dans les virages et les dévers.
// Maillages : height vaut zéro et le reflet passe par la matrice model.
// Décor : voir plus bas.
layout(location = 0) in vec3 inPos;
layout(location = 1) in float inHeight;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in float inKind;
layout(location = 4) in vec2 inUv;
layout(location = 5) in vec4 inColor;

layout(location = 0) out vec3 worldPos;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec2 uv;
layout(location = 3) out vec4 color;
layout(location = 4) flat out float kind;

void main() {
    vec3 p;
    if (pc.colorB.w > 1.5) {
        // Décor en égaliseur : chaque bâtiment suit une bande de fréquences
        // et s'étire avec elle. inHeight porte sa bande et la hauteur du
        // sommet au-dessus de sa base.
        float band = floor(inHeight / 4096.0);
        float above = inHeight - band * 4096.0;
        float level = band < 3.5 ? g.bandsLow[int(band)] : g.bandsHigh[int(band) - 4];
        p = inPos + vec3(0.0, above * (pc.colorB.w - 1.5) * level, 0.0);
    } else {
        p = inPos + inNormal * (inHeight * g.camRight.w);
    }
    vec4 world = pc.model * vec4(p, 1.0);
    worldPos = world.xyz;
    normal = mat3(pc.model) * inNormal;
    uv = inUv;
    color = inColor;
    kind = inKind;
    gl_Position = g.viewProj * world;
}
