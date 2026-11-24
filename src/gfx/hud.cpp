#include "gfx/hud.h"

#include <algorithm>

#include "core/log.h"

#include <stb_truetype.h>

#include <cmath>

namespace gfx {

namespace {

const unsigned char kFontMedium[] =
#include "Orbitron-Medium.ttf.inc"
    ;
const unsigned char kFontBold[] =
#include "Orbitron-Bold.ttf.inc"
    ;

// Décode un caractère UTF-8. Les glyphes au-delà de Latin-1 ne sont pas dans
// l'atlas, donc deux octets suffisent.
uint32_t nextCodepoint(std::string_view text, size_t& i) {
    unsigned char c = static_cast<unsigned char>(text[i++]);
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && i < text.size())
        return ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(text[i++]) & 0x3Fu);
    while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) i++;
    return '?';
}

} // namespace

void FontAtlas::bake() {
    width = 1024;
    height = 1024;
    pixels.assign(static_cast<size_t>(width) * height, 0);

    const float emPixels = 40.0f;
    const int padding = 5;
    const unsigned char onEdge = 128;
    const float distScale = static_cast<float>(onEdge) / static_cast<float>(padding);

    const unsigned char* sources[] = {kFontMedium, kFontBold};
    int penX = 1, penY = 1, rowHeight = 0;

    for (int f = 0; f < static_cast<int>(FontId::Count); f++) {
        stbtt_fontinfo info;
        if (!stbtt_InitFont(&info, sources[f], stbtt_GetFontOffsetForIndex(sources[f], 0)))
            fatal("Police illisible");
        float scale = stbtt_ScaleForMappingEmToPixels(&info, emPixels);

        for (int cp = 32; cp < 256; cp++) {
            if (cp >= 127 && cp < 160) continue;
            int index = stbtt_FindGlyphIndex(&info, cp);
            if (index == 0 && cp != 32) continue;

            int advance = 0, bearing = 0;
            stbtt_GetGlyphHMetrics(&info, index, &advance, &bearing);
            Glyph& g = glyphs[f][cp];
            g.advance = static_cast<float>(advance) * scale / emPixels;
            g.valid = true;

            int w = 0, h = 0, xoff = 0, yoff = 0;
            unsigned char* sdf = stbtt_GetGlyphSDF(&info, scale, index, padding, onEdge,
                                                   distScale, &w, &h, &xoff, &yoff);
            if (!sdf) continue; // espace : une avance, pas d'image

            if (penX + w + 1 > width) {
                penX = 1;
                penY += rowHeight + 1;
                rowHeight = 0;
            }
            if (penY + h + 1 > height) fatal("Atlas de police trop petit");
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    pixels[static_cast<size_t>(penY + y) * width + penX + x] = sdf[y * w + x];
            stbtt_FreeSDF(sdf, nullptr);

            g.uv0 = {static_cast<float>(penX) / width, static_cast<float>(penY) / height};
            g.uv1 = {static_cast<float>(penX + w) / width, static_cast<float>(penY + h) / height};
            g.min = {static_cast<float>(xoff) / emPixels, static_cast<float>(yoff) / emPixels};
            g.max = {static_cast<float>(xoff + w) / emPixels,
                     static_cast<float>(yoff + h) / emPixels};
            penX += w + 1;
            rowHeight = std::max(rowHeight, h);
        }
    }
}

void HudBatch::begin(const FontAtlas& atlas, float screenWidth, float screenHeight) {
    fonts = &atlas;
    verts.clear();
    screen = {screenWidth, screenHeight};
    scale = fitScale(screen);
    anchor = Anchor::Left;
    vAnchor = VAnchor::Middle;
}

float HudBatch::fitScale(glm::vec2 screen) {
    return std::min(screen.y / kRefHeight, screen.x / kRefWidth);
}

glm::vec2 HudBatch::place(glm::vec2 ref, Anchor anchor, VAnchor vAnchor, glm::vec2 screen) {
    const float scale = fitScale(screen);
    float x;
    switch (anchor) {
    case Anchor::Center:
        x = screen.x * 0.5f + (ref.x - kRefWidth * 0.5f) * scale;
        break;
    case Anchor::Right:
        x = screen.x - (kRefWidth - ref.x) * scale;
        break;
    default:
        x = ref.x * scale;
        break;
    }
    float y;
    switch (vAnchor) {
    case VAnchor::Top:
        y = ref.y * scale;
        break;
    case VAnchor::Bottom:
        y = screen.y - (kRefHeight - ref.y) * scale;
        break;
    default:
        y = screen.y * 0.5f + (ref.y - kRefHeight * 0.5f) * scale;
        break;
    }
    return {x, y};
}

glm::vec2 HudBatch::px(glm::vec2 ref) const { return place(ref, anchor, vAnchor, screen); }

void HudBatch::push(glm::vec2 posPx, glm::vec2 uv, glm::vec4 color, glm::vec4 color2,
                    glm::vec4 params, float mode) {
    verts.push_back({posPx, uv, color, color2, params, mode});
}

void HudBatch::tri(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec4 color) {
    const glm::vec4 zero{0.0f};
    push(px(a), {}, color, zero, zero, 0.0f);
    push(px(b), {}, color, zero, zero, 0.0f);
    push(px(c), {}, color, zero, zero, 0.0f);
}

void HudBatch::quad(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d, glm::vec4 color) {
    tri(a, b, c, color);
    tri(a, c, d, color);
}

void HudBatch::line(glm::vec2 a, glm::vec2 b, float thickness, glm::vec4 color, float glow) {
    glm::vec2 pa = px(a), pb = px(b);
    glm::vec2 delta = pb - pa;
    float len = glm::length(delta);
    if (len < 1e-4f) return;
    glm::vec2 dir = delta / len;
    glm::vec2 normal{-dir.y, dir.x};
    float radius = thickness * 0.5f * scale;
    float pad = radius + (glow > 0.0f ? 18.0f * scale : 1.5f);
    float halfLen = len * 0.5f;
    glm::vec2 mid = (pa + pb) * 0.5f;
    glm::vec4 params{halfLen, radius, glow, 0.0f};
    const glm::vec4 zero{0.0f};

    const glm::vec2 corners[4] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    HudVertex v[4];
    for (int i = 0; i < 4; i++) {
        glm::vec2 local{corners[i].x * (halfLen + pad), corners[i].y * pad};
        v[i] = {mid + dir * local.x + normal * local.y, local, color, zero, params, 3.0f};
    }
    for (int i : {0, 1, 2, 0, 2, 3}) verts.push_back(v[i]);
}

void HudBatch::roundRect(glm::vec2 center, glm::vec2 half, float radius, glm::vec4 fill,
                         glm::vec4 border, float borderWidth) {
    glm::vec2 c = px(center);
    glm::vec2 h = half * scale;
    float pad = 26.0f * scale;
    glm::vec4 params{h.x, h.y, radius * scale, borderWidth * scale};
    const glm::vec2 corners[4] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    HudVertex v[4];
    for (int i = 0; i < 4; i++) {
        glm::vec2 local = corners[i] * (h + pad);
        v[i] = {c + local, local, fill, border, params, 2.0f};
    }
    for (int i : {0, 1, 2, 0, 2, 3}) verts.push_back(v[i]);
}

float HudBatch::textWidth(FontId font, std::string_view str, float size) const {
    float w = 0.0f;
    for (size_t i = 0; i < str.size();) {
        uint32_t cp = nextCodepoint(str, i);
        if (cp < 256 && fonts->glyphs[static_cast<int>(font)][cp].valid)
            w += fonts->glyphs[static_cast<int>(font)][cp].advance * size;
    }
    return w;
}

float HudBatch::text(FontId font, std::string_view str, glm::vec2 pos, float size,
                     glm::vec4 color, Align align, float shear, glm::vec4 halo) {
    float width = textWidth(font, str, size);
    float pen = pos.x;
    if (align == Align::Center) pen -= width * 0.5f;
    if (align == Align::Right) pen -= width;
    const glm::vec4 zero{0.0f};

    for (size_t i = 0; i < str.size();) {
        uint32_t cp = nextCodepoint(str, i);
        if (cp >= 256) continue;
        const Glyph& g = fonts->glyphs[static_cast<int>(font)][cp];
        if (!g.valid) continue;
        if (g.uv1.x > g.uv0.x) {
            float x0 = pen + g.min.x * size, x1 = pen + g.max.x * size;
            float y0 = pos.y + g.min.y * size, y1 = pos.y + g.max.y * size;
            // Italique : les points au-dessus de la ligne de base partent à droite.
            float s0 = (pos.y - y0) * shear, s1 = (pos.y - y1) * shear;
            HudVertex v[4] = {
                {px({x0 + s0, y0}), {g.uv0.x, g.uv0.y}, color, halo, zero, 1.0f},
                {px({x1 + s0, y0}), {g.uv1.x, g.uv0.y}, color, halo, zero, 1.0f},
                {px({x1 + s1, y1}), {g.uv1.x, g.uv1.y}, color, halo, zero, 1.0f},
                {px({x0 + s1, y1}), {g.uv0.x, g.uv1.y}, color, halo, zero, 1.0f},
            };
            for (int k : {0, 1, 2, 0, 2, 3}) verts.push_back(v[k]);
        }
        pen += g.advance * size;
    }
    return width;
}

} // namespace gfx
