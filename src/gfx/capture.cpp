#include "gfx/capture.h"

#include <stb_image_write.h>

namespace gfx {

void Capture::init(Gpu& gpu, VkExtent2D size) {
    extent = size;
    staging = gpu.createBuffer(static_cast<VkDeviceSize>(size.width) * size.height * 4,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, "capture");
}

void Capture::shutdown(Gpu& gpu) { gpu.destroyBuffer(staging); }

void Capture::recordCopy(VkCommandBuffer cmd, const Image& image) {
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           staging.buffer, 1, &region);
}

bool Capture::writePng(const char* path) const {
    return stbi_write_png(path, static_cast<int>(extent.width), static_cast<int>(extent.height),
                          4, staging.mapped, static_cast<int>(extent.width) * 4) != 0;
}

} // namespace gfx
