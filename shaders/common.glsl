// Données partagées par tous les shaders de scène.

layout(set = 0, binding = 0) uniform Globals {
    mat4 viewProj;
    vec4 camPos;     // xyz position, w temps en secondes
    vec4 camRight;   // xyz axe droit multiplié par tan(fov horizontal / 2), w signe du miroir
    vec4 camUp;      // xyz axe haut multiplié par tan(fov vertical / 2)
    vec4 camForward; // xyz axe de visée
    vec4 fog;        // rgb couleur de la brume, a densité
    vec4 screen;     // xy inverse de la taille de la cible de scène, z vitesse en m/s, w pulsation de la musique de 0 à 1
    vec4 neonA;      // couleurs du thème du circuit
    vec4 neonB;
    vec4 skyZenith;
    vec4 skyMid;
    vec4 skyHaze;    // a style de ciel : 0 anneau, 1 soleil, 2 planète, 3 fournaise, 4 fonds marins
    vec4 bandsLow;   // niveaux de la musique par bande, du grave à l'aigu, de 0 à 1
    vec4 bandsHigh;
} g;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 colorA;
    vec4 colorB;
    vec4 colorC; // vaisseau : rgb lumière de rythme sur le dos, a intensité
} pc;

// Les deux couleurs néon du circuit. Les noms datent du premier thème.
#define CYAN g.neonA.rgb
#define MAGENTA g.neonB.rgb

float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Garde-fou des sorties de la scène : une valeur non finie ou démesurée
// donnerait des pixels noirs après le tone mapping, que le bloom étale.
vec3 safe(vec3 c) {
    if (any(isnan(c)) || any(isinf(c))) return vec3(0.0);
    return clamp(c, vec3(0.0), vec3(256.0));
}

vec3 applyFog(vec3 color, vec3 worldPos) {
    float dist = length(worldPos - g.camPos.xyz);
    float visible = exp(-dist * g.fog.a);
    return mix(g.fog.rgb, color, visible);
}
