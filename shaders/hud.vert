#version 450

// Interface 2D. Les positions sont en pixels du repère du jeu, origine en
// haut à gauche.
layout(push_constant) uniform Push {
    vec4 rotation; // même matrice que la passe de composition
    vec4 params;   // xy taille de l'écran vue par le jeu
} pc;

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec4 inColor2;
layout(location = 4) in vec4 inParams;
layout(location = 5) in float inMode;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 color;
layout(location = 2) out vec4 color2;
layout(location = 3) flat out vec4 params;
layout(location = 4) flat out float mode;

vec4 toLinear(vec4 c) { return vec4(pow(c.rgb, vec3(2.2)), c.a); }

void main() {
    vec2 ndc = inPos / pc.params.xy * 2.0 - 1.0;
    // La matrice fait passer de l'écran physique au repère du jeu. Elle est
    // orthonormale, donc sa transposée fait le chemin inverse.
    mat2 r = mat2(pc.rotation.xy, pc.rotation.zw);
    gl_Position = vec4(transpose(r) * ndc, 0.0, 1.0);
    uv = inUv;
    color = toLinear(inColor);
    color2 = toLinear(inColor2);
    params = inParams;
    mode = inMode;
}
