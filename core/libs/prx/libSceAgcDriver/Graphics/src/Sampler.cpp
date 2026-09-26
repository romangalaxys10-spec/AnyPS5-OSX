#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"

namespace AgcDriver::Graphics {

    Sampler::Sampler(const Context& context, const GuestSamplerResource& descriptor) : device(context.device), destroySampler(context.Function<PFN_vkDestroySampler>("vkDestroySampler")) {
        Require(!descriptor.anisotropyEnable || context.samplerAnisotropy, "guest sampler descriptor requests anisotropic filtering which the device does not support");
        Require(descriptor.maxAnisotropy <= context.limits.maxSamplerAnisotropy, "guest sampler descriptor requests an anisotropy ratio beyond the device limit");
        Require(descriptor.lodBias >= -context.limits.maxSamplerLodBias && descriptor.lodBias <= context.limits.maxSamplerLodBias, "guest sampler descriptor requests a LOD bias beyond the device limit");

        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        info.magFilter = descriptor.magFilter;
        info.minFilter = descriptor.minFilter;
        info.mipmapMode = descriptor.mipmapMode;
        info.addressModeU = descriptor.addressModeU;
        info.addressModeV = descriptor.addressModeV;
        info.addressModeW = descriptor.addressModeW;
        info.mipLodBias = descriptor.lodBias;
        info.anisotropyEnable = descriptor.anisotropyEnable ? VK_TRUE : VK_FALSE;
        info.maxAnisotropy = descriptor.maxAnisotropy;
        info.compareEnable = descriptor.compareEnable ? VK_TRUE : VK_FALSE;
        info.compareOp = descriptor.compareOp;
        info.minLod = descriptor.minLod;
        info.maxLod = descriptor.maxLod;
        info.borderColor = descriptor.borderColor;
        info.unnormalizedCoordinates = VK_FALSE;
        Check(context.Function<PFN_vkCreateSampler>("vkCreateSampler")(context.device, &info, nullptr, &sampler), "vkCreateSampler");
    }

    Sampler::~Sampler() {
        release();
    }

    void Sampler::release() noexcept {
        if (sampler) destroySampler(device, sampler, nullptr);
    }

    VkSampler Sampler::Handle() const {
        return sampler;
    }

    std::shared_ptr<Sampler> SamplerCache::Get(const Context& context, std::span<const std::uint32_t> words, const GuestSamplerResource& descriptor) {
        Require(words.size() == 4, "sampler cache descriptor must contain four DWORDs");
        const std::array<std::uint32_t, 5> key{words[0], words[1], words[2], words[3], descriptor.compareEnable ? 1u : 0u};
        std::lock_guard lock(mutex);
        const auto found = entries.find(key);
        if (found != entries.end()) return found->second;
        auto sampler = std::make_shared<Sampler>(context, descriptor);
        if (entries.size() >= 256) entries.erase(entries.begin());
        entries.emplace(key, sampler);
        return sampler;
    }

}
