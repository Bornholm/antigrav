// Ciel procédural. Sert au fond, au reflet de la piste et à l'éclairage
// d'ambiance des vaisseaux, donc les trois restent cohérents.

float skyStars(vec3 d, float density) {
    vec3 q = d * 220.0;
    vec3 cell = floor(q);
    float star = hash21(cell.xy + cell.z * 13.7);
    return smoothstep(1.0 - density, 1.0, star) * smoothstep(0.6, 0.0, length(fract(q) - 0.5));
}

// Bruit lissé à une dimension, pour les fumées et les rais de lumière.
float skyNoise(float x) {
    float i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(hash11(i), hash11(i + 1.0), f);
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
    } else if (style > 3.5) {
        // Fonds marins : la surface miroite au zénith, des rais de lumière
        // descendent, du plancton dérive. Le tout respire avec les graves.
        float t = g.camPos.w;
        float bass = g.bandsLow.x;
        float azimuth = atan(d.x, -d.z);
        float rays = skyNoise(azimuth * 5.0 + t * 0.07) * skyNoise(azimuth * 11.0 - t * 0.05 + 7.0);
        rays = pow(rays, 1.5) * smoothstep(0.02, 0.75, d.y);
        c += mix(CYAN, vec3(0.8, 1.0, 0.95), 0.35) * rays * (0.22 + 0.5 * bass);
        // Surface : réseau de reflets qui ondule, vu de dessous.
        vec2 sv = d.xz / max(d.y, 0.08) * 1.6;
        float caustic = sin(sv.x * 3.0 + t * 0.9 + sin(sv.y * 2.3 + t * 0.6) * 1.7) *
                        sin(sv.y * 3.4 - t * 0.7 + sin(sv.x * 1.9 - t * 0.5) * 1.5);
        caustic = pow(max(caustic, 0.0), 3.0) * smoothstep(0.35, 0.95, d.y);
        c += mix(MAGENTA, CYAN, 0.5 + 0.5 * sin(sv.x + sv.y)) * caustic * (0.25 + 0.6 * bass);
        // Plancton : des points qui montent lentement.
        vec3 drift = normalize(d + vec3(0.0, -fract(t * 0.004), 0.0) * 0.6);
        c += CYAN * skyStars(drift, 0.02) * smoothstep(-0.05, 0.4, d.y) * 0.8;
    } else if (style > 2.5) {
        // Fournaise : une bouche de four rougeoie sur l'horizon derrière des
        // barres de métal, la fumée monte en colonnes, des braises volent.
        float t = g.camPos.w;
        float bass = g.bandsLow.x;
        // La bouche : un large rectangle derrière une grille de barreaux verticaux.
        vec3 mouthDir = normalize(vec3(0.10, 0.0, -1.0));
        vec3 side = normalize(cross(mouthDir, vec3(0.0, 1.0, 0.0)));
        float behind = step(0.0, dot(d, mouthDir));
        float lx = dot(d, side) / 0.42, ly = (d.y - 0.11) / 0.13;
        float edge = max(abs(lx), abs(ly));
        float grate = step(0.28, fract(lx * 5.5 + 0.14));
        float mouth = smoothstep(1.0, 0.97, edge) * grate * behind;
        vec3 hot = mix(haze * 1.6, vec3(1.0, 0.75, 0.35), smoothstep(1.0, -0.8, ly));
        c = mix(c, hot * (0.9 + 1.3 * bass), mouth);
        c += haze * (0.10 + 0.22 * bass) * exp(-max(edge - 1.0, 0.0) * 2.2) * smoothstep(-0.05, 0.1, d.y) * behind;
        // Colonnes de fumée : elles mangent la lueur de l'horizon par endroits.
        float azimuth = atan(d.x, -d.z);
        float smoke = skyNoise(azimuth * 6.0 + d.y * 3.0 - t * 0.03) * skyNoise(azimuth * 14.0 - d.y * 9.0 + t * 0.05);
        c *= 1.0 - 0.75 * smoothstep(0.18, 0.55, smoke) * smoothstep(0.0, 0.12, d.y) * smoothstep(0.75, 0.2, d.y);
        // Braises : des points chauds qui montent.
        vec3 rise = normalize(d - vec3(0.0, fract(t * 0.02), 0.0) * 0.5);
        c += vec3(1.0, 0.55, 0.15) * skyStars(rise, 0.012) * smoothstep(0.0, 0.1, d.y) * smoothstep(0.7, 0.15, d.y) * 1.4;
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
