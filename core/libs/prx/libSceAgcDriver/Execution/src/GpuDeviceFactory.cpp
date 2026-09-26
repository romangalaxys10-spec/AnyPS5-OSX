#include "prx/libSceAgcDriver/Execution/include/GpuDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>

#if defined(__APPLE__)
#include "prx/libSceAgcDriver/Execution/include/MetalDevice.hpp"
#endif

namespace AgcDriver {
namespace {

enum class Backend { Auto, Metal, Vulkan };

Backend ReadBackend() {
    if (const char* value = std::getenv("ANYPS5_GPU_BACKEND")) {
        const std::string request(value);
        if (request == "metal" || request == "METAL") return Backend::Metal;
        if (request == "vulkan" || request == "VULKAN") return Backend::Vulkan;
        if (request != "auto" && request != "AUTO") {
            throw std::runtime_error("ANYPS5_GPU_BACKEND must be auto, metal or vulkan");
        }
    }
    return Backend::Auto;
}

}

std::shared_ptr<GpuDevice> MakeGpuDevice(const PresentationWindow* window) {
    const Backend backend = ReadBackend();
#if defined(__APPLE__)
    if (backend == Backend::Metal) {
        return std::make_shared<MetalDevice>(window);
    }
    if (backend == Backend::Auto) {
        try {
            return std::make_shared<MetalDevice>(window);
        } catch (const std::exception&) {
            // Fall back to Vulkan on top of MoltenVK when Metal or MetalFX is unavailable.
        }
    }
    return std::make_shared<VulkanDevice>(window);
#else
    static_cast<void>(backend);
    return std::make_shared<VulkanDevice>(window);
#endif
}

}
