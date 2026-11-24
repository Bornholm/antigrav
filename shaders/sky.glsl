// Ciel procédural. Sert au fond, au reflet de la piste et à l'éclairage
// d'ambiance des vaisseaux, donc les trois restent cohérents.

float skyStars(vec3 d, float density) {
    vec3 q = d * 220.0;
    vec3 cell = floor(q);
    float star = hash21(cell.xy + cell.z * 13.7);
    return smoothstep(1.0 - density, 1.0, star) * smoothstep(0.6, 0.0, length(fract(q) - 0.5));
}

vec3 skyColor(vec3 d) {
    vec3 zenith = g.skyZenith.rgb;
    vec3 mid = g.skyMid.rgb;
    vec3 haze = g.skyHaze.rgb;
    float style = g.skyHaze.a;

    float up = max(d.y, 0.0);
    vec3 c = mix(mid, zenith, smoothstep(0.0, 0.55, up));
    c = mix(c, haze, exp(-abs(d.y) * 10.0));
    // Sous l'horizon : la brume s'éteint vers le noir.
    c *= mix(1.0, 0.12, smoothstep(0.0, -0.35, d.y));

    if (style < 0.5) {
        // Grand anneau lumineux posé sur l'horizon.
        vec3 ringDir = normalize(vec3(0.22, 0.13, -1.0));
        float ang = acos(clamp(dot(d, ringDir), -1.0, 1.0));
        float ring = exp(-abs(ang - 0.155) * 260.0);
        float side = dot(d - ringDir, vec3(1.0, 0.0, 0.0));
        vec3 ringColor = mix(MAGENTA, CYAN, smoothstep(0.02, 0.14, side));
        c += ringColor * ring * 5.0 * smoothstep(-0.01, 0.03, d.y);
        c += vec3(0.7, 0.8, 1.0) * skyStars(d, 0.008) * smoothstep(0.15, 0.6, d.y) * 0.45;
    } else if (style < 1.5) {
        // Soleil couchant strié, dégradé du jaune au rose.
        vec3 sunDir = normalize(vec3(0.12, 0.16, -1.0));
        const float radius = 0.24;
        float ang = acos(clamp(dot(d, sunDir), -1.0, 1.0));
        float y = (d.y - sunDir.y) / radius;
        float bands = y < 0.25 ? step(0.38 + 0.25 * y, fract(y * 5.5)) : 1.0;
        float disc = smoothstep(radius, radius - 0.004, ang) * bands * step(0.0, d.y);
        vec3 sunColor = mix(vec3(1.0, 0.12, 0.42), vec3(1.0, 0.80, 0.18), smoothstep(-0.9, 0.8, y));
        c = mix(c, sunColor * 2.4, disc);
        c += sunColor * 0.22 * exp(-max(ang - radius, 0.0) * 7.0) * smoothstep(-0.05, 0.1, d.y);
        c += vec3(1.0, 0.9, 0.8) * skyStars(d, 0.006) * smoothstep(0.35, 0.8, d.y) * 0.35;
    } else {
        // Planète géante dont on ne voit que le limbe, et champ d'étoiles dense.
        vec3 planetDir = normalize(vec3(-0.75, -0.30, -1.0));
        const float radius = 0.95;
        float ang = acos(clamp(dot(d, planetDir), -1.0, 1.0));
        float inside = smoothstep(radius, radius - 0.01, ang);
        float limb = exp(-abs(ang - radius) * 34.0);
        float lit = smoothstep(-0.2, 0.9, dot(normalize(d - planetDir), normalize(vec3(0.6, 0.8, 0.2))));
        vec3 body = vec3(0.004, 0.012, 0.022) + CYAN * 0.020 * (0.5 + 0.5 * sin(d.y * 60.0 + d.x * 9.0));
        c = mix(c + vec3(0.8, 0.9, 1.0) * skyStars(d, 0.022) * 0.7, body, inside);
        c += mix(MAGENTA, CYAN, lit) * limb * (0.5 + 1.8 * lit);
        // Aurore lente en altitude.
        float aurora = smoothstep(0.25, 0.7, d.y) *
                       (0.5 + 0.5 * sin(d.x * 7.0 + sin(d.z * 5.0 + g.camPos.w * 0.15) * 2.0));
        c += CYAN * aurora * 0.035 * (1.0 - inside);
    }
    return c;
}
