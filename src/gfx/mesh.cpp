#include "gfx/mesh.h"

#include <cstring>

namespace gfx {

// Les deux GPU visés partagent leur mémoire avec le processeur. Un tampon
// visible par l'hôte ne coûte donc rien de plus qu'un tampon réservé au GPU,
// et il évite une copie intermédiaire.
Mesh uploadMesh(Gpu& gpu, const MeshData& data, const char* name) {
    Mesh mesh;
    mesh.indexCount = static_cast<uint32_t>(data.indices.size());
    VkDeviceSize vbytes = data.vertices.size() * sizeof(Vertex);
    VkDeviceSize ibytes = data.indices.size() * sizeof(uint32_t);
    mesh.vertices = gpu.createBuffer(vbytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, name);
    mesh.indices = gpu.createBuffer(ibytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, true, name);
    std::memcpy(mesh.vertices.mapped, data.vertices.data(), vbytes);
    std::memcpy(mesh.indices.mapped, data.indices.data(), ibytes);
    vmaFlushAllocation(gpu.allocator, mesh.vertices.alloc, 0, VK_WHOLE_SIZE);
    vmaFlushAllocation(gpu.allocator, mesh.indices.alloc, 0, VK_WHOLE_SIZE);
    return mesh;
}

void destroyMesh(Gpu& gpu, Mesh& mesh) {
    gpu.destroyBuffer(mesh.vertices);
    gpu.destroyBuffer(mesh.indices);
    mesh = Mesh{};
}

} // namespace gfx
