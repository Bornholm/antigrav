#version 450
#include "post.glsl"

// Dernière passe : scène plus bloom, tone mapping, vignette.
// params.x : force du bloom. params.y : exposition.
layout(set = 0, binding = 0) uniform sampler2D sceneTex;
layout(set = 0, binding = 1) uniform sampler2D bloomTex;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

// Approximation de la courbe ACES par Krzysztof Narkowicz.
vec3 tonemap(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 c = texture(sceneTex, uv).rgb + texture(bloomTex, uv).rgb * pc.params.x;
    c *= pc.params.y;
    vec2 q = uv - 0.5;
    c *= 1.0 - dot(q, q) * 0.55;
    outColor = vec4(tonemap(c), 1.0);
}
