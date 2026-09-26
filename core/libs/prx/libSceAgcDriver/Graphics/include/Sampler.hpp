#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SAMPLER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SAMPLER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestSamplerResource.hpp"
#include <array>
#include <map>
#include <mutex>

namespace AgcDriver::Graphics {

class Sampler {
public:
    Sampler(const Context& context, const GuestSamplerResource& descriptor);
    ~Sampler();
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    VkSampler Handle() const;

private:
    void release() noexcept;

    VkDevice device;
    PFN_vkDestroySampler destroySampler;
    VkSampler sampler = VK_NULL_HANDLE;
};

class SamplerCache {
public:
    std::shared_ptr<Sampler> Get(const Context& context, std::span<const std::uint32_t> words, const GuestSamplerResource& descriptor);

private:
    std::mutex mutex;
    std::map<std::array<std::uint32_t, 5>, std::shared_ptr<Sampler>> entries;
};

}

#endif
