#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"

namespace AgcDriver::Graphics {

BufferPool::BufferPool(const Context& context) : device(context.device), unmap(context.Function<PFN_vkUnmapMemory>("vkUnmapMemory")), destroyBuffer(context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")), freeMemory(context.Function<PFN_vkFreeMemory>("vkFreeMemory")) {
    buckets.fill(none);
    for (std::size_t index = 0; index < capacity; ++index) freeSlots[index] = index;
}

BufferPool::~BufferPool() {
    for (const auto& slot : slots) {
        if (slot.allocation) destroy(*slot.allocation);
    }
}

std::size_t BufferPool::bucket(std::size_t bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) noexcept {
    auto hash = bytes ^ (bytes >> 16u);
    hash = hash * 16777619u ^ usage;
    hash = hash * 16777619u ^ properties;
    return (hash ^ (hash >> 8u) ^ (hash >> 16u)) % bucketCount;
}

BufferAllocation BufferPool::remove(std::size_t index) noexcept {
    auto& slot = slots[index];
    const auto allocation = *slot.allocation;
    auto* link = &buckets[bucket(allocation.bytes, allocation.usage, allocation.properties)];
    while (*link != index) link = &slots[*link].next;
    *link = slot.next;
    if (slot.older != none) slots[slot.older].newer = slot.newer;
    else oldest = slot.newer;
    if (slot.newer != none) slots[slot.newer].older = slot.older;
    else newest = slot.older;
    retainedBytes -= allocation.allocationBytes;
    slot = Slot{};
    freeSlots[freeCount++] = index;
    return allocation;
}

void BufferPool::destroy(const BufferAllocation& allocation) noexcept {
    if (allocation.mapping != nullptr) unmap(device, allocation.memory);
    destroyBuffer(device, allocation.buffer, nullptr);
    freeMemory(device, allocation.memory, nullptr);
}

std::optional<BufferAllocation> BufferPool::Take(std::size_t bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    std::lock_guard lock(mutex);
    for (auto index = buckets[bucket(bytes, usage, properties)]; index != none; index = slots[index].next) {
        const auto& allocation = *slots[index].allocation;
        if (allocation.bytes == bytes && allocation.usage == usage && allocation.properties == properties) return remove(index);
    }
    return std::nullopt;
}

void BufferPool::Put(const BufferAllocation& allocation) noexcept {
    std::lock_guard lock(mutex);
    if (allocation.allocationBytes > budget) {
        destroy(allocation);
        return;
    }
    while (retainedBytes > budget - allocation.allocationBytes || freeCount == 0) destroy(remove(oldest));
    const auto index = freeSlots[--freeCount];
    auto& slot = slots[index];
    auto& head = buckets[bucket(allocation.bytes, allocation.usage, allocation.properties)];
    slot.allocation = allocation;
    slot.next = head;
    slot.older = newest;
    if (newest != none) slots[newest].newer = index;
    else oldest = index;
    newest = index;
    head = index;
    retainedBytes += allocation.allocationBytes;
}

std::shared_ptr<BufferPool> GetBufferPool(const Context& context) {
    if (!context.bufferPool) context.bufferPool = std::make_shared<BufferPool>(context);
    return context.bufferPool;
}

}
