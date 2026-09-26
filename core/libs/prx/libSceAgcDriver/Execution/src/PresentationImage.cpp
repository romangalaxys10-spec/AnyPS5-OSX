#include "prx/libSceAgcDriver/Execution/include/PresentationScaler.hpp"

namespace AgcDriver {

void PresentationScaler::RecordImage(VkCommandBuffer commands, VkImage image) {
    Graphics::Require(sourceImage != VK_NULL_HANDLE && image != VK_NULL_HANDLE, "presentation image is unavailable");
    const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = sourceImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkImageCopy copy{};
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.dstSubresource = copy.srcSubresource;
    copy.extent = {sourceWidth, sourceHeight, 1};
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

}
