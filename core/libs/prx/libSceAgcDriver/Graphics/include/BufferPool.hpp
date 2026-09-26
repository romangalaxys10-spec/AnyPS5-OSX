#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BUFFERPOOL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BUFFERPOOL_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <memory>
#include <mutex>
#include <optional>

namespace AgcDriver::Graphics {

struct BufferAllocation {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapping;
    VkDeviceAddress address;
    VkDeviceSize allocationBytes;
    std::size_t bytes;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
};

class BufferPool {
public:
    explicit BufferPool(const Context& context);
    ~BufferPool();
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    std::optional<BufferAllocation> Take(std::size_t bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void Put(const BufferAllocation& allocation) noexcept;

private:
    static constexpr std::size_t capacity = 1024;
    static constexpr std::size_t bucketCount = 256;
    static constexpr std::size_t none = capacity;
    struct Slot {
        std::optional<BufferAllocation> allocation;
        std::size_t next = none;
        std::size_t older = none;
        std::size_t newer = none;
    };
    static std::size_t bucket(std::size_t bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) noexcept;
    BufferAllocation remove(std::size_t index) noexcept;
    void destroy(const BufferAllocation& allocation) noexcept;
    VkDevice device;
    PFN_vkUnmapMemory unmap;
    PFN_vkDestroyBuffer destroyBuffer;
    PFN_vkFreeMemory freeMemory;
    std::mutex mutex;
    std::array<Slot, capacity> slots;
    std::array<std::size_t, bucketCount> buckets;
    std::array<std::size_t, capacity> freeSlots;
    std::size_t freeCount = capacity;
    std::size_t oldest = none;
    std::size_t newest = none;
    VkDeviceSize retainedBytes = 0;
    static constexpr VkDeviceSize budget = 512ull * 1024 * 1024;
};

std::shared_ptr<BufferPool> GetBufferPool(const Context& context);

}

#endif
