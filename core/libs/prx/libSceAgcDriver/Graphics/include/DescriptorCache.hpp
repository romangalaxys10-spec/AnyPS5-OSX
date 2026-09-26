#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DESCRIPTORCACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DESCRIPTORCACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <mutex>
#include <span>
#include <vector>

namespace AgcDriver::Graphics {

class DescriptorAllocation {
public:
    DescriptorAllocation(const Context& context, std::span<const std::uint32_t> key, std::span<const VkDescriptorSetLayoutBinding> bindings, std::span<const VkDescriptorPoolSize> sizes);
    ~DescriptorAllocation();
    DescriptorAllocation(const DescriptorAllocation&) = delete;
    DescriptorAllocation& operator=(const DescriptorAllocation&) = delete;
    VkDescriptorSetLayout Layout() const { return layout; }
    VkDescriptorSet Set() const { return set; }
    bool Matches(std::span<const std::uint32_t> candidate) const;

private:
    void release() noexcept;
    VkDevice device;
    PFN_vkDestroyDescriptorPool destroyPool;
    PFN_vkDestroyDescriptorSetLayout destroyLayout;
    std::vector<std::uint32_t> key;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};

class DescriptorCache {
public:
    std::unique_ptr<DescriptorAllocation> Take(std::span<const std::uint32_t> key);
    void Put(std::unique_ptr<DescriptorAllocation> allocation);

private:
    std::mutex mutex;
    std::array<std::unique_ptr<DescriptorAllocation>, 256> available;
    std::size_t cursor = 0;
};

}

#endif
