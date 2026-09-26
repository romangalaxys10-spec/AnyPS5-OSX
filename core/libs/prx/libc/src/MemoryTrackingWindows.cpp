#include "prx/libc/include/MemoryTrackingPlatform.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <system_error>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace GuestMemoryTracking::Platform {
namespace {

FaultHandler faultHandler = nullptr;

LONG CALLBACK handleException(EXCEPTION_POINTERS* exception) {
    const auto* record = exception->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2 || record->ExceptionInformation[0] > 1) return EXCEPTION_CONTINUE_SEARCH;
    try {
        if (faultHandler(record->ExceptionInformation[1], record->ExceptionInformation[0] == 1)) return EXCEPTION_CONTINUE_EXECUTION;
    } catch (...) {
        std::terminate();
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void protect(std::uint64_t address, std::size_t bytes, DWORD protection) {
    DWORD previous = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), bytes, protection, &previous)) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "guest memory tracking VirtualProtect failed");
}

}

std::size_t PageSize() {
    static const auto size = [] {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        if (info.dwPageSize == 0) throw std::runtime_error("invalid native page size");
        return static_cast<std::size_t>(info.dwPageSize);
    }();
    return size;
}

void Install(FaultHandler handler) {
    if (handler == nullptr || faultHandler != nullptr) throw std::runtime_error("invalid guest memory fault handler installation");
    faultHandler = handler;
    if (AddVectoredExceptionHandler(1, handleException) == nullptr) {
        const auto error = GetLastError();
        faultHandler = nullptr;
        throw std::system_error(static_cast<int>(error), std::system_category(), "guest memory fault handler installation failed");
    }
}

std::vector<Region> Query(std::uint64_t address, std::size_t bytes) {
    std::vector<Region> regions;
    const auto end = address + bytes;
    while (address < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info)) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "tracked guest memory query failed");
        if (info.State != MEM_COMMIT || info.Protect != PAGE_READWRITE) throw std::runtime_error("tracked render memory must be committed, writable and non-executable");
        const auto next = std::min(end, reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize);
        if (next <= address) throw std::runtime_error("invalid tracked guest memory mapping");
        regions.push_back({address, static_cast<std::size_t>(next - address), info.Protect});
        address = next;
    }
    return regions;
}

void Protect(std::uint64_t address, std::size_t bytes, Protection protection) {
    if (protection != Protection::None && protection != Protection::Read) throw std::invalid_argument("invalid tracked page protection");
    protect(address, bytes, protection == Protection::None ? PAGE_NOACCESS : PAGE_READONLY);
}

void Restore(const std::vector<Region>& regions) {
    for (const auto& region : regions) protect(region.address, region.bytes, static_cast<DWORD>(region.protection));
}

}
