#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"

namespace AgcDriver::Graphics {

Texture::Texture(const Context& context, const std::shared_ptr<ResidentColor>& source, const GuestTextureResource& descriptor, VkComponentMapping components) : context(context), source(source) {
    try {
        Require(source != nullptr && descriptor.dimension == TextureDimension::k2D && descriptor.mipCount == 1 && descriptor.baseLevel == 0 && descriptor.baseArray == 0, "invalid resident texture view");
        const auto format = ResolveTextureFormat(descriptor.format);
        Require(!IsBlockCompressed(descriptor.format) && BytesPerElement(descriptor.format) == 4, "resident texture copy requires a 32-bit texel format");
        VkFormatProperties properties{};
        context.formatProperties(context.physical, format, &properties);
        Require((properties.optimalTilingFeatures & (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) == (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT), "resident texture format does not support sampling and copies");
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {descriptor.width, descriptor.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage resident texture");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocationBytes = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory resident texture");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory resident texture");
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView resident texture");
        if (context.drawQueue) context.drawQueue->Flush();
        upload = std::make_unique<CommandBatch>(context);
        const auto commands = upload->Handle();
        source->Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = viewInfo.subresourceRange;
        const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkImageCopy copy{};
        copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.dstSubresource = copy.srcSubresource;
        copy.extent = info.extent;
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source->Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        upload->Submit();
    } catch (...) {
        release();
        throw;
    }
}

}
