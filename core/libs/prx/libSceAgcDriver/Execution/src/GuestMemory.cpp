#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fstream>
#include <sstream>
#endif

namespace AgcDriver::GuestMemory {
namespace {
void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("AGC driver: ") + reason);
}
}

void CheckRange(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    require(alignment != 0, "zero guest memory alignment");
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && address % alignment == 0, "null or misaligned address");
    require(bytes <= std::numeric_limits<std::uintptr_t>::max() - address, "address range overflow");
    MemoryAccessScope::Resolve(address, bytes, writable);
    GuestMemoryTracking::GuestMemoryTrackingResolve_nid_postfix(address, bytes, writable);
    auto cursor = address;
    const auto end = address + bytes;
#ifdef _WIN32
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION memory{};
        require(VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == sizeof(memory), "cannot query guest memory");
        require(memory.State == MEM_COMMIT && (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0, "guest memory is not readable");
        const auto protection = memory.Protect & 0xffu;
        require(protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY, "guest memory has no read permission");
        const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        require(memory.RegionSize <= std::numeric_limits<std::uintptr_t>::max() - base && base + memory.RegionSize > cursor, "invalid guest memory mapping");
        require(!writable || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY, "guest memory has no write permission");
        cursor = std::min(end, base + memory.RegionSize);
    }
#else
    std::ifstream maps("/proc/self/maps");
    require(maps.is_open(), "cannot query guest memory maps");
    std::string line;
    while (cursor < end && std::getline(maps, line)) {
        std::istringstream fields(line);
        std::uintptr_t first = 0;
        std::uintptr_t last = 0;
        char separator = 0;
        std::string permissions;
        require(static_cast<bool>(fields >> std::hex >> first >> separator >> last >> permissions) && separator == '-' && first < last && !permissions.empty(), "invalid guest memory map entry");
        if (last <= cursor) continue;
        require(first <= cursor && permissions[0] == 'r', "guest memory is not readable");
        require(!writable || (permissions.size() > 1 && permissions[1] == 'w'), "guest memory has no write permission");
        cursor = std::min(end, last);
    }
    require(cursor == end, "guest address range is not mapped");
#endif
}

void Read(std::uint64_t address, std::span<std::byte> destination, std::size_t alignment) {
    PerformanceTimer timing("GuestMemory.Read");
    if (destination.empty()) return;
    const auto* source = reinterpret_cast<const void*>(address);
    CheckRange(source, destination.size(), alignment);
    timing.Mark("range_check");
    std::memcpy(destination.data(), source, destination.size());
    timing.Mark("copy", destination.size());
}

void Write(std::uint64_t address, std::span<const std::byte> source, std::size_t alignment) {
    PerformanceTimer timing("GuestMemory.Write");
    if (source.empty()) return;
    auto* destination = reinterpret_cast<void*>(address);
    CheckRange(destination, source.size(), alignment, true);
    timing.Mark("range_check");
    std::memcpy(destination, source.data(), source.size());
    timing.Mark("copy", source.size());
}

}

extern "C" void AgcDriverCheckGuestMemory_nid_postfix(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    AgcDriver::GuestMemory::CheckRange(pointer, bytes, alignment, writable);
}
