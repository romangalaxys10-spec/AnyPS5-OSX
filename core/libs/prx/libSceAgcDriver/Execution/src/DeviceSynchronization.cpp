#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"

namespace AgcDriver {

void VulkanDevice::AcquireGpuMemory() {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    PerformanceTimer timing("Vulkan.AcquireGpuMemory");
    const auto context = graphicsContext();
    Graphics::Require(context.drawQueue != nullptr, "GPU memory barrier requires a draw queue");
    context.drawQueue->RecordMemoryBarrier(context);
    timing.Mark("barrier_record");
}

}
