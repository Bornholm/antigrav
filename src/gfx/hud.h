#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace gfx {

struct HudVertex {
    glm::vec2 pos;
    glm::vec2 uv;
    glm::vec4 color;
    glm::vec4 color2;
    glm::vec4 params;
    float mode;
};

struct Glyph {
    glm::vec2 uv0{0.0f}, uv1{0.0f};   // rectangle dans l'atlas
    glm::vec2 min{0.0f}, max{0.0f};   // rectangle par rapport au crayon, en cadratins, y vers le bas
    float advance = 0.0f;             // en cadratins
    bool valid = false;
};

enum class FontId { Medium = 0, Bold = 1, Count };

// Atlas de glyphes en champ de distance : un seul rendu de la police sert à
// toutes les tailles de texte.
struct FontAtlas {
    int width = 0, height = 0;
    std::vector<uint8_t> pixels;
    Glyph glyphs[static_cast<int>(FontId::Count)][256];

    void bake();
};

enum class Anchor { Left, Center, Right };
enum class Align { Left, Center, Right };

// Accumule les formes de l'interface d'une image.
//
// Les coordonnées sont données dans un repère de référence de 900 unités de
// haut. `anchor` choisit le bord de l'écran auquel les abscisses s'accrochent,
// ce qui garde la mise en page correcte sur tous les rapports d'écran.
struct HudBatch {
    static constexpr float kRefWidth = 1600.0f;
    static constexpr float kRefHeight = 900.0f;

    const FontAtlas* fonts = nullptr;
    std::vector<HudVertex> verts;
    glm::vec2 screen{kRefWidth, kRefHeight};
    float scale = 1.0f;
    Anchor anchor = Anchor::Left;

    void begin(const FontAtlas& atlas, float screenWidth, float screenHeight);

    // Repère de référence vers pixels.
    glm::vec2 px(glm::vec2 ref) const;

    void quad(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d, glm::vec4 color);
    void tri(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec4 color);
    void line(glm::vec2 a, glm::vec2 b, float thickness, glm::vec4 color, float glow = 0.0f);
    void roundRect(glm::vec2 center, glm::vec2 half, float radius, glm::vec4 fill,
                   glm::vec4 border, float borderWidth);
    void circle(glm::vec2 center, float radius, glm::vec4 fill, glm::vec4 border,
                float borderWidth) {
        roundRect(center, {radius, radius}, radius, fill, border, borderWidth);
    }

    float textWidth(FontId font, std::string_view text, float size) const;
    // pos est le point de la ligne de base. Renvoie la largeur du texte.
    float text(FontId font, std::string_view text, glm::vec2 pos, float size, glm::vec4 color,
               Align align = Align::Left, float shear = 0.2f, glm::vec4 halo = glm::vec4(0.0f));

  private:
    void push(glm::vec2 posPx, glm::vec2 uv, glm::vec4 color, glm::vec4 color2, glm::vec4 params,
              float mode);
};

} // namespace gfx
