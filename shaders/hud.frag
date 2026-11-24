#version 450

// Interface 2D. Sortie en alpha prémultiplié.
// mode 0 : aplat. mode 1 : texte en champ de distance. mode 2 : rectangle
// arrondi avec bordure et halo. mode 3 : trait à bouts ronds.
layout(set = 0, binding = 0) uniform sampler2D atlas;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 color;
layout(location = 2) in vec4 color2;
layout(location = 3) flat in vec4 params;
layout(location = 4) flat in float mode;

layout(location = 0) out vec4 outColor;

float sdRoundBox(vec2 p, vec2 halfSize, float radius) {
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main() {
    vec4 c;
    if (mode < 0.5) {
        c = vec4(color.rgb * color.a, color.a);
    } else if (mode < 1.5) {
        float d = texture(atlas, uv).r;
        float w = max(fwidth(d) * 0.75, 0.01);
        float fill = smoothstep(0.5 - w, 0.5 + w, d) * color.a;
        float halo = smoothstep(0.15, 0.5, d) * color2.a * (1.0 - fill);
        c = vec4(color.rgb * fill + color2.rgb * halo, max(fill, halo * 0.6));
    } else if (mode < 2.5) {
        // params : demi-largeur, demi-hauteur, rayon, épaisseur de bordure.
        float d = sdRoundBox(uv, params.xy, params.z);
        float inside = 1.0 - smoothstep(-0.75, 0.75, d);
        float border = inside * smoothstep(-params.w - 0.75, -params.w + 0.75, d);
        float fillA = (inside - border) * color.a;
        // Le halo s'annule avant le bord du quad, sinon son contour se voit.
        float fade = clamp(1.0 - d / 22.0, 0.0, 1.0);
        float halo = exp(-max(d, 0.0) / 6.0) * fade * fade * (1.0 - inside) * color2.a * 0.40;
        float borderA = border * color2.a;
        c = vec4(color.rgb * fillA + color2.rgb * (borderA + halo), fillA + borderA + halo * 0.5);
    } else {
        // params : demi-longueur, rayon, intensité du halo.
        vec2 p = vec2(max(abs(uv.x) - params.x, 0.0), uv.y);
        float d = length(p) - params.y;
        float line = 1.0 - smoothstep(-0.75, 0.75, d);
        float fade = clamp(1.0 - d / 15.0, 0.0, 1.0);
        float halo = exp(-max(d, 0.0) / 4.0) * fade * fade * (1.0 - line) * params.z;
        float a = line * color.a;
        c = vec4(color.rgb * (a + halo * color.a), a + halo * 0.4 * color.a);
    }
    outColor = c;
}
