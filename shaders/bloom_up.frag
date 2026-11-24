#version 450
#include "post.glsl"

// Agrandissement par deux, ajouté en additif au niveau supérieur.
layout(set = 0, binding = 0) uniform sampler2D src;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 t = pc.params.xy;
    vec3 c = texture(src, uv + vec2(-t.x * 2.0, 0.0)).rgb;
    c += texture(src, uv + vec2(t.x * 2.0, 0.0)).rgb;
    c += texture(src, uv + vec2(0.0, -t.y * 2.0)).rgb;
    c += texture(src, uv + vec2(0.0, t.y * 2.0)).rgb;
    c += texture(src, uv + vec2(-t.x, t.y)).rgb * 2.0;
    c += texture(src, uv + vec2(t.x, t.y)).rgb * 2.0;
    c += texture(src, uv + vec2(-t.x, -t.y)).rgb * 2.0;
    c += texture(src, uv + vec2(t.x, -t.y)).rgb * 2.0;
    outColor = vec4(c / 12.0 * pc.params.z, 1.0);
}
