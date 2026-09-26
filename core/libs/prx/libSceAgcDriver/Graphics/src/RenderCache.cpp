#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <limits>

namespace AgcDriver::Graphics {

void ResidentColor::Transition(VkCommandBuffer commands, VkImageLayout next) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0u : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.oldLayout = layout;
    barrier.newLayout = next;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = Target().Image();
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    layout = next;
}

void ResidentColor::Begin(VkCommandBuffer commands) {
    PerformanceTimer timing("Graphics.ResidentColor.Begin");
    Require(generation != std::numeric_limits<std::uint64_t>::max(), "resident color generation overflow");
    ++generation;
    Require(memoryWatch != nullptr, "render target memory ownership was released");
    if (!valid) {
        memoryWatch->Protect(GuestMemoryTracking::Protection::Read);
        const GuestMemory::MemoryAccessScope suspended(nullptr, nullptr);
        transfer.Upload(color.address, color.extent.width, color.extent.height, color.tileMode);
        transfer.Detile(commands);
        Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {color.extent.width, color.extent.height, 1};
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, transfer.LinearBuffer(), Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        timing.Mark("upload", color.bytes);
    } else {
        timing.Mark("reuse");
    }
    Transition(commands, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    valid = true;
    dirty = true;
    memoryWatch->Protect(GuestMemoryTracking::Protection::None);
}

void ResidentColor::Download(VkCommandBuffer commands) {
    if (!dirty) return;
    Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkMemoryBarrier reuse{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    reuse.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    reuse.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &reuse, 0, nullptr, 0, nullptr);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {color.extent.width, color.extent.height, 1};
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transfer.LinearBuffer(), 1, &copy);
    transfer.Tile(commands);
}

void ResidentColor::Commit() {
    if (!dirty) return;
    const GuestMemory::MemoryAccessScope suspended(nullptr, nullptr);
    transfer.WriteBackTracked(color.address);
    dirty = false;
    memoryWatch->Protect(GuestMemoryTracking::Protection::Read);
}

std::shared_ptr<ResidentColor> RenderCache::Get(const ColorTarget& color, bool blending) {
    Require(color.address != 0 && color.bytes != 0 && color.bytes <= std::numeric_limits<std::uint64_t>::max() - color.address, "invalid resident color range");
    if (context.drawQueue) context.drawQueue->Resolve(color.address, color.bytes);
    if (blending) {
        VkFormatProperties properties{};
        context.formatProperties(context.physical, color.format, &properties);
        Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) != 0, "render target format does not support blending");
    }
    for (auto it = entries.begin(); it != entries.end();) {
        const auto& previous = it->second->Description();
        if (!it->second->SharesPages(color)) {
            ++it;
            continue;
        }
        if (previous.address == color.address && previous.bytes == color.bytes && previous.extent.width == color.extent.width && previous.extent.height == color.extent.height && previous.format == color.format && previous.tileMode == color.tileMode) return it->second;
        Resolve(previous.address, previous.bytes, true);
        it->second->ReleaseMemory();
        it = entries.erase(it);
    }
    if (entries.size() >= 64) {
        Flush();
        for (const auto& [address, resident] : entries) resident->ReleaseMemory();
        entries.clear();
    }
    auto entry = std::make_shared<ResidentColor>(context, color);
    entries.emplace(color.address, entry);
    return entry;
}

std::shared_ptr<ResidentColor> RenderCache::Find(std::uint64_t address) const {
    const auto it = entries.find(address);
    if (it != entries.end() && context.drawQueue) context.drawQueue->Resolve(address, it->second->Description().bytes);
    return it != entries.end() && it->second->Valid() ? it->second : nullptr;
}

void RenderCache::Resolve(std::uint64_t address, std::size_t bytes, bool writable) {
    std::vector<std::shared_ptr<ResidentColor>> affected;
    for (const auto& [base, entry] : entries) {
        const auto& color = entry->Description();
        if (address >= base + color.bytes || base >= address + bytes) continue;
        if (entry->Dirty()) affected.push_back(entry);
        else if (writable) entry->Invalidate();
    }
    if (affected.empty()) return;
    PerformanceTimer timing("Graphics.RenderCache.Resolve");
    if (context.drawQueue) context.drawQueue->Wait();
    CommandBatch batch(context);
    for (const auto& entry : affected) entry->Download(batch.Handle());
    batch.SubmitAndWait();
    timing.Mark("download_wait");
    for (const auto& entry : affected) {
        entry->Commit();
        if (writable) entry->Invalidate();
    }
    timing.Mark("guest_writeback");
}

void RenderCache::Flush() {
    Resolve(0, std::numeric_limits<std::size_t>::max(), true);
}

}
