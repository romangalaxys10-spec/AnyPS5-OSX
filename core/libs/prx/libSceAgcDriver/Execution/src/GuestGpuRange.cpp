#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <limits>
#include <stdexcept>

namespace AgcDriver::GuestMemory {

void CheckGpuRange(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    if (alignment == 0 || address == 0 || address % alignment != 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - address) throw std::invalid_argument("invalid guest GPU memory range");
    const MemoryAccessScope suspended(nullptr, nullptr);
    GuestMemoryTracking::GuestMemoryTrackingValidate_nid_postfix(address, bytes, [writable](std::uint64_t first, std::size_t count) {
        CheckRange(reinterpret_cast<const void*>(first), count, 1, writable);
    });
}

}
