#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PM4_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PM4_HPP

#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4Opcodes.hpp"
#include <array>
#include <span>
#include <string>

namespace AgcDriver::Pm4 {

struct DrawParameters {
    std::uint64_t indexAddress;
    std::uint32_t indexCount;
    std::uint32_t indexSize;
    std::uint32_t instanceCount;
    std::uint32_t flags;
    bool indexed = true;
    std::uint32_t firstVertex = 0;
    std::uint32_t firstInstance = 0;
};

std::string Name(std::uint32_t header);
std::string_view UnsupportedReason(std::uint32_t header);
void Validate(std::span<const std::uint32_t> packet, std::uint32_t queue);
void Execute(std::span<const std::uint32_t> packet, QueueState& queue);
bool AccessesMemory(std::uint32_t header);
bool UsesGpuCacheBarrier(std::span<const std::uint32_t> packet);
std::array<std::uint32_t, 5> ResolveDispatch(std::span<const std::uint32_t> packet, const QueueState& queue);
DrawParameters ResolveDraw(std::span<const std::uint32_t> packet, const QueueState& queue);

}

#endif
