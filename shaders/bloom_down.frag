#version 450
#include "post.glsl"

// Réduction par deux, filtre double : un échantillon central et quatre
// diagonaux. params.xy : taille d'un texel de la source. params.z : seuil,
// appliqué seulement à la première réduction.
layout(set = 0, binding = 0) uniform sampler2D src;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

vec3 prefilter(vec3 c) {
    float threshold = pc.params.z;
    if (threshold <= 0.0) return c;
    float brightness = max(c.r, max(c.g, c.b));
    float knee = threshold * 0.5;
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-4);
    float contribution = max(soft, brightness - threshold) / max(brightness, 1e-4);
    return c * contribution;
}

void main() {
    vec2 t = pc.params.xy;
    vec3 c = texture(src, uv).rgb * 4.0;
    c += texture(src, uv + vec2(-t.x, -t.y)).rgb;
    c += texture(src, uv + vec2(t.x, -t.y)).rgb;
    c += texture(src, uv + vec2(-t.x, t.y)).rgb;
    c += texture(src, uv + vec2(t.x, t.y)).rgb;
    // Plafond : un pixel aberrant ne doit pas noyer toute l'image.
    outColor = vec4(min(prefilter(c / 8.0), vec3(48.0)), 1.0);
}
