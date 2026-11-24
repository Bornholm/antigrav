#pragma once

#include "gfx/gpu.h"

#include <glm/glm.hpp>

#include <vector>

namespace gfx {

// Format de sommet unique de la scène. Voir shaders/world.vert.
struct Vertex {
    glm::vec3 pos;
    float height = 0.0f;
    glm::vec3 normal;
    float kind = 0.0f;
    glm::vec2 uv{0.0f};
    glm::vec4 color{1.0f};
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    uint32_t add(const Vertex& v) {
        vertices.push_back(v);
        return static_cast<uint32_t>(vertices.size() - 1);
    }
    void tri(uint32_t a, uint32_t b, uint32_t c) {
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }
    void quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
        tri(a, b, c);
        tri(a, c, d);
    }
};

struct Mesh {
    Buffer vertices;
    Buffer indices;
    uint32_t indexCount = 0;
    // Maillage de piste : premier indice de chaque tronçon, puis le total.
    // Vide pour un maillage dessiné d'un bloc.
    std::vector<uint32_t> chunks;
};

Mesh uploadMesh(Gpu& gpu, const MeshData& data, const char* name);
void destroyMesh(Gpu& gpu, Mesh& mesh);

} // namespace gfx
