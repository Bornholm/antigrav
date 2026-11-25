#version 450
#include "common.glsl"
#include "sky.glsl"

layout(location = 0) in vec3 dir;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(safe(skyColor(normalize(dir))), 1.0);
}
