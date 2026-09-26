#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"

namespace AgcDriver::Graphics {

void DrawQueue::retire(Batch batch) {
    PerformanceTimer timing("Graphics.DrawQueue.Retire");
    const GuestMemory::MemoryAccessScope suspended(nullptr, nullptr);
    Require(drawCount >= batch.entries.size(), "draw queue completion count underflow");
    drawCount -= batch.entries.size();
    for (auto& entry : batch.entries) entry.resources->WriteBack();
    timing.Mark("resources_writeback");
    batch.entries.clear();
    available.push_back(std::move(batch.commands));
    timing.Mark("resources_release");
}

void DrawQueue::Collect() {
    while (!pending.empty() && pending.front().commands->IsComplete()) {
        auto batch = std::move(pending.front());
        pending.erase(pending.begin());
        retire(std::move(batch));
    }
}

void DrawQueue::WaitGpu() {
    Flush();
    for (auto& batch : pending) batch.commands->Wait();
}

void DrawQueue::Wait() {
    if (pending.empty() && !recording.commands) return;
    PerformanceTimer timing("Graphics.DrawQueue.Wait");
    Flush();
    timing.Mark("submit");
    while (!pending.empty()) {
        pending.front().commands->Wait();
        timing.Mark("fence_wait");
        auto batch = std::move(pending.front());
        pending.erase(pending.begin());
        retire(std::move(batch));
        timing.Mark("retire");
    }
}

void DrawQueue::RecordMemoryBarrier(const Context& context) {
    const auto commands = Begin(context);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    recording.hasBarrier = true;
}

}
