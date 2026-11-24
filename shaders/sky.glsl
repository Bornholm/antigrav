// Ciel procédural. Sert au fond, au reflet de la piste et à l'éclairage
// d'ambiance des vaisseaux, donc les trois restent cohérents.

vec3 skyColor(vec3 d) {
    vec3 zenith = vec3(0.003, 0.004, 0.018);
    vec3 mid = vec3(0.022, 0.008, 0.060);
    vec3 haze = vec3(0.150, 0.030, 0.230);

    float up = max(d.y, 0.0);
    vec3 c = mix(mid, zenith, smoothstep(0.0, 0.55, up));
    c = mix(c, haze, exp(-abs(d.y) * 10.0));
    // Sous l'horizon : la brume s'éteint vers le noir.
    c *= mix(1.0, 0.12, smoothstep(0.0, -0.35, d.y));

    // Grand anneau lumineux posé sur l'horizon.
    vec3 ringDir = normalize(vec3(0.22, 0.13, -1.0));
    float ang = acos(clamp(dot(d, ringDir), -1.0, 1.0));
    float ring = exp(-abs(ang - 0.155) * 260.0);
    float side = dot(d - ringDir, vec3(1.0, 0.0, 0.0));
    vec3 ringColor = mix(MAGENTA, CYAN, smoothstep(0.02, 0.14, side));
    c += ringColor * ring * 5.0 * smoothstep(-0.01, 0.03, d.y);

    // Étoiles, seulement en hauteur.
    vec3 q = d * 220.0;
    vec3 cell = floor(q);
    float star = hash21(cell.xy + cell.z * 13.7);
    float twinkle = smoothstep(0.985, 1.0, star) * smoothstep(0.6, 0.0, length(fract(q) - 0.5));
    c += vec3(0.7, 0.8, 1.0) * twinkle * smoothstep(0.08, 0.4, d.y) * 0.9;

    return c;
}
