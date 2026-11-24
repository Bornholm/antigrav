#version 450

// Image de démarrage du socle : dégradé de ciel et horizon néon.
layout(push_constant) uniform Push {
    vec4 rotation;
    vec4 params; // x = temps, y = rapport largeur / hauteur
} pc;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

void main() {
    float t = pc.params.x;
    vec2 p = (uv - 0.5) * vec2(pc.params.y, 1.0);

    vec3 top = vec3(0.012, 0.010, 0.045);
    vec3 low = vec3(0.070, 0.012, 0.120);
    vec3 sky = mix(low, top, smoothstep(-0.1, 0.5, -p.y));

    float horizon = exp(-abs(p.y) * 60.0);
    vec3 line = mix(vec3(0.0, 0.9, 1.0), vec3(1.0, 0.1, 0.9), 0.5 + 0.5 * sin(p.x * 3.0 + t));

    float ring = abs(length(p - vec2(0.12, -0.08)) - 0.16);
    vec3 arc = vec3(1.0, 0.25, 0.85) * exp(-ring * 160.0) * step(p.y, 0.0);

    color = vec4(sky + line * horizon + arc, 1.0);
}
