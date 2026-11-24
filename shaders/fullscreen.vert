#version 450

// Triangle unique couvrant l'écran, sans tampon de sommets.
layout(push_constant) uniform Push {
    vec4 rotation; // matrice 2x2 qui ramène l'écran physique dans le repère du jeu
    vec4 params;
} pc;

layout(location = 0) out vec2 uv;

void main() {
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
    gl_Position = vec4(p, 0.0, 1.0);
    uv = mat2(pc.rotation.xy, pc.rotation.zw) * p * 0.5 + 0.5;
}
