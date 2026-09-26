#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <limits>

namespace AgcDriver::Graphics {

ResidentColor::ResidentColor(const Context& context, const ColorTarget& color) : context(context), color(color), transfer(context) {
    memoryWatch = std::make_unique<GuestMemoryTracking::Watch>(color.address, color.bytes, this, [](void* owner, GuestMemoryTracking::Access access) {
        static_cast<ResidentColor*>(owner)->resolveCpuAccess(access);
    });
}

ResidentColor::~ResidentColor() = default;

void ResidentColor::Invalidate() {
    Require(!dirty, "cannot discard GPU-owned render target contents");
    if (memoryWatch) memoryWatch->Protect(GuestMemoryTracking::Protection::ReadWrite);
    valid = false;
}

void ResidentColor::ReleaseMemory() {
    Require(!dirty, "cannot release GPU-owned render target memory");
    Invalidate();
    memoryWatch.reset();
}

bool ResidentColor::SharesPages(const ColorTarget& other) const {
    const auto pageSize = GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix();
    Require(color.bytes != 0 && other.bytes != 0, "empty render target page range");
    Require(color.bytes <= std::numeric_limits<std::uint64_t>::max() - color.address && other.bytes <= std::numeric_limits<std::uint64_t>::max() - other.address, "render target page range overflow");
    const auto first = color.address / pageSize;
    const auto last = (color.address + color.bytes - 1) / pageSize;
    const auto otherFirst = other.address / pageSize;
    const auto otherLast = (other.address + other.bytes - 1) / pageSize;
    return first <= otherLast && otherFirst <= last;
}

void ResidentColor::resolveCpuAccess(GuestMemoryTracking::Access access) {
    PerformanceTimer timing("Graphics.RenderMemory.CpuAccess");
    Require(memoryWatch != nullptr && context.drawQueue != nullptr, "render target memory resolver is unavailable");
    if (dirty || access == GuestMemoryTracking::Access::Invalidate) {
        context.drawQueue->WaitGpu();
        timing.Mark("draw_wait");
    }
    if (dirty) {
        CommandBatch batch(context);
        Download(batch.Handle());
        batch.SubmitAndWait();
        timing.Mark("download_wait");
        Commit();
        timing.Mark("guest_writeback");
    }
    if (access != GuestMemoryTracking::Access::Read) Invalidate();
    if (access == GuestMemoryTracking::Access::Invalidate) context.drawQueue->Wait();
}

RenderCache::~RenderCache() {
    for (const auto& [address, entry] : entries) entry->ReleaseMemory();
}

}
