#version 450
#include "common.glsl"

layout(location = 0) out vec3 dir;

void main() {
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
    // Profondeur 0 : le plan lointain en profondeur inversée.
    gl_Position = vec4(p, 0.0, 1.0);
    dir = g.camForward.xyz + p.x * g.camRight.xyz - p.y * g.camUp.xyz;
}
