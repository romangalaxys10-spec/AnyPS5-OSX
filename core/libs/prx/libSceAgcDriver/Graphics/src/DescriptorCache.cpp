#include "prx/libSceAgcDriver/Graphics/include/DescriptorCache.hpp"
#include <algorithm>
#include <utility>

namespace AgcDriver::Graphics {

DescriptorAllocation::DescriptorAllocation(const Context& context, std::span<const std::uint32_t> key, std::span<const VkDescriptorSetLayoutBinding> bindings, std::span<const VkDescriptorPoolSize> sizes) : device(context.device), destroyPool(context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")), destroyLayout(context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")), key(key.begin(), key.end()) {
    try {
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = static_cast<std::uint32_t>(bindings.size());
        info.pBindings = bindings.data();
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(device, &info, nullptr, &layout), "vkCreateDescriptorSetLayout");
        if (bindings.empty()) return;
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        poolInfo.pPoolSizes = sizes.data();
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &layout;
        Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(device, &allocation, &set), "vkAllocateDescriptorSets");
    } catch (...) {
        release();
        throw;
    }
}

DescriptorAllocation::~DescriptorAllocation() {
    release();
}

void DescriptorAllocation::release() noexcept {
    if (pool) destroyPool(device, pool, nullptr);
    if (layout) destroyLayout(device, layout, nullptr);
}

bool DescriptorAllocation::Matches(std::span<const std::uint32_t> candidate) const {
    return std::equal(key.begin(), key.end(), candidate.begin(), candidate.end());
}

std::unique_ptr<DescriptorAllocation> DescriptorCache::Take(std::span<const std::uint32_t> key) {
    std::lock_guard lock(mutex);
    for (auto& allocation : available) {
        if (allocation && allocation->Matches(key)) return std::move(allocation);
    }
    return nullptr;
}

void DescriptorCache::Put(std::unique_ptr<DescriptorAllocation> allocation) {
    Require(allocation != nullptr, "cannot cache an empty descriptor allocation");
    std::lock_guard lock(mutex);
    for (std::size_t offset = 0; offset < available.size(); ++offset) {
        const auto index = (cursor + offset) % available.size();
        if (available[index]) continue;
        available[index] = std::move(allocation);
        cursor = (index + 1) % available.size();
        return;
    }
    available[cursor] = std::move(allocation);
    cursor = (cursor + 1) % available.size();
}

}
