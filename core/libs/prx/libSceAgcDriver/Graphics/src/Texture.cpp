#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace AgcDriver::Graphics {

namespace {


std::uint32_t FullArrayLayers(const GuestTextureResource& descriptor) {
    switch (descriptor.dimension) {
        case TextureDimension::k1D:
        case TextureDimension::k2D: return 1u;
        case TextureDimension::k2DArray:
        case TextureDimension::kCube: return descriptor.depthOrLastArray + 1u;
    }
    throw std::runtime_error("AGC graphics: Texture encountered an unknown guest texture dimension");
}

VkImageType ImageTypeFor(TextureDimension dimension) {
    return dimension == TextureDimension::k1D ? VK_IMAGE_TYPE_1D : VK_IMAGE_TYPE_2D;
}

VkImageViewType ViewTypeFor(TextureDimension dimension, std::uint32_t viewLayerCount) {
    switch (dimension) {
        case TextureDimension::k1D: return VK_IMAGE_VIEW_TYPE_1D;
        case TextureDimension::k2D: return VK_IMAGE_VIEW_TYPE_2D;
        case TextureDimension::k2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureDimension::kCube: return viewLayerCount == 6u ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }
    throw std::runtime_error("AGC graphics: Texture encountered an unknown guest texture dimension");
}

std::uint64_t SliceLinearBytes(const std::vector<TileMipLayout>& mips) {
    std::uint64_t bytes = 0;
    for (const auto& mip : mips) bytes = std::max(bytes, mip.linearOffset + mip.linearSize);
    return bytes;
}

}

Texture::Texture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, VkComponentMapping components, std::span<const std::byte> snapshot) : context(context) {
    try {
        const auto vkFormat = ResolveTextureFormat(descriptor.format);
        if (IsBlockCompressed(descriptor.format)) {
            Require(context.textureCompressionBC, "device does not support BC compressed textures");
        }

        const auto mips = ComputeMipLayout(descriptor.tileMode, descriptor.format, descriptor.width, descriptor.height, descriptor.mipCount);
        const auto arrayLayers = FullArrayLayers(descriptor);
        const auto elementBytes = BytesPerElement(descriptor.format);

        const auto guestBytes = ComputeSurfaceSize(mips, arrayLayers);
        const auto guestSliceBytes = guestBytes / arrayLayers;
        Require(snapshot.size() == guestBytes, "texture snapshot size mismatch");

        const auto sliceLinearBytes = SliceLinearBytes(mips);
        Require(arrayLayers == 0 || sliceLinearBytes <= UINT64_MAX / arrayLayers, "detiled texture buffer size overflows");
        const auto linearBytes = sliceLinearBytes * arrayLayers;

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.flags = descriptor.dimension == TextureDimension::kCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u;
        imageInfo.imageType = ImageTypeFor(descriptor.dimension);
        imageInfo.format = vkFormat;
        imageInfo.extent = {descriptor.width, descriptor.height, 1u};
        imageInfo.mipLevels = descriptor.mipCount;
        imageInfo.arrayLayers = arrayLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage");

        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocationBytes = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory texture");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory");

        {
            Buffer staging(context, static_cast<std::size_t>(guestBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            std::memcpy(staging.Bytes().data(), snapshot.data(), snapshot.size());
            Buffer linear(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

            detiler.BeginBatch();
            if (context.drawQueue) context.drawQueue->Flush();
            CommandBatch batch(context);
            const auto commands = batch.Handle();

            VkBufferMemoryBarrier stagingReadBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            stagingReadBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            stagingReadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            stagingReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            stagingReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            stagingReadBarrier.buffer = staging.Handle();
            stagingReadBarrier.offset = 0;
            stagingReadBarrier.size = VK_WHOLE_SIZE;

            VkBufferMemoryBarrier linearWriteBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            linearWriteBarrier.srcAccessMask = 0;
            linearWriteBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            linearWriteBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearWriteBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearWriteBarrier.buffer = linear.Handle();
            linearWriteBarrier.offset = 0;
            linearWriteBarrier.size = VK_WHOLE_SIZE;

            const VkBufferMemoryBarrier preBarriers[] = {stagingReadBarrier, linearWriteBarrier};
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 2, preBarriers, 0, nullptr);

            for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                const auto guestLayerOffset = static_cast<std::uint64_t>(layer) * guestSliceBytes;
                const auto linearLayerOffset = static_cast<std::uint64_t>(layer) * sliceLinearBytes;
                for (const auto& mip : mips) {
                    detiler.Dispatch(commands, descriptor.tileMode, elementBytes, staging.Handle(), guestLayerOffset + mip.tiledOffset, linear.Handle(), linearLayerOffset + mip.linearOffset, mip, layer);
                }
            }

            VkBufferMemoryBarrier linearReadBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            linearReadBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            linearReadBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            linearReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearReadBarrier.buffer = linear.Handle();
            linearReadBarrier.offset = 0;
            linearReadBarrier.size = VK_WHOLE_SIZE;

            VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransferDst.srcAccessMask = 0;
            toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.image = image;
            toTransferDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, arrayLayers};
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearReadBarrier, 1, &toTransferDst);

            std::vector<VkBufferImageCopy> regions;
            regions.reserve(static_cast<std::size_t>(arrayLayers) * mips.size());
            for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                const auto linearLayerOffset = static_cast<std::uint64_t>(layer) * sliceLinearBytes;
                for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
                    const auto& mip = mips[level];
                    VkBufferImageCopy region{};
                    region.bufferOffset = linearLayerOffset + mip.linearOffset;
                    region.bufferRowLength = mip.pitchBytes / elementBytes * BlockWidth(descriptor.format);
                    region.bufferImageHeight = 0;
                    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1};
                    region.imageOffset = {0, 0, 0};
                    region.imageExtent = {std::max(descriptor.width >> level, 1u), std::max(descriptor.height >> level, 1u), 1u};
                    regions.push_back(region);
                }
            }
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear.Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());

            VkImageMemoryBarrier toShaderRead{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toShaderRead.image = image;
            toShaderRead.subresourceRange = toTransferDst.subresourceRange;
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShaderRead);

            batch.SubmitAndWait();
        }

        const auto viewLevelCount = descriptor.mipCount - descriptor.baseLevel;
        const auto viewLayerCount = arrayLayers - descriptor.baseArray;
        if (descriptor.dimension == TextureDimension::kCube) {
            Require(viewLayerCount % 6u == 0, "guest cube texture view does not contain a multiple of 6 array slices");
        }

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = ViewTypeFor(descriptor.dimension, viewLayerCount);
        viewInfo.format = vkFormat;
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView");
    } catch (...) {
        release();
        throw;
    }
}

Texture::~Texture() {
    release();
}

void Texture::release() noexcept {
    upload.reset();
    if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
    if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
}

VkImageView Texture::View() const {
    return view;
}

}
