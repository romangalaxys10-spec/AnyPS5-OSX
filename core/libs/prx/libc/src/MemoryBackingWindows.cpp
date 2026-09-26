#include "prx/libc/include/MemoryBackingPlatform.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <system_error>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace GuestMemoryBacking::Platform {
namespace {

DWORD nativeProtection(int protection) {
    if ((protection & 4) != 0) return (protection & 2) != 0 ? PAGE_EXECUTE_READWRITE : (protection & 1) != 0 ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    if ((protection & 2) != 0) return PAGE_READWRITE;
    return (protection & 1) != 0 ? PAGE_READONLY : PAGE_NOACCESS;
}

void check(bool success, const char* operation) {
    if (!success) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
}

}

Mapping Map(void* address, std::size_t bytes, std::size_t alignment, int protection) {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    alignment = std::max(alignment, static_cast<std::size_t>(info.dwAllocationGranularity));
    if (address != nullptr && reinterpret_cast<std::uintptr_t>(address) % info.dwAllocationGranularity != 0) throw std::invalid_argument("fixed guest view is not aligned to native allocation granularity");
    const auto size = static_cast<std::uint64_t>(bytes);
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, static_cast<DWORD>(size >> 32u), static_cast<DWORD>(size), nullptr);
    check(section != nullptr, "CreateFileMapping guest backing");
    void* alias = nullptr;
    void* guest = nullptr;
    try {
        alias = MapViewOfFile(section, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, bytes);
        check(alias != nullptr, "MapViewOfFile guest backing alias");
        if (address == nullptr && alignment > info.dwAllocationGranularity) {
            if (bytes > std::numeric_limits<std::size_t>::max() - alignment) throw std::overflow_error("aligned guest backing reservation overflow");
            void* reservation = VirtualAlloc(nullptr, bytes + alignment, MEM_RESERVE, PAGE_NOACCESS);
            check(reservation != nullptr, "VirtualAlloc guest backing reservation");
            const auto first = reinterpret_cast<std::uintptr_t>(reservation);
            address = reinterpret_cast<void*>((first + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1));
            check(VirtualFree(reservation, 0, MEM_RELEASE) != FALSE, "VirtualFree guest backing reservation");
        }
        guest = MapViewOfFileEx(section, FILE_MAP_READ | FILE_MAP_WRITE | FILE_MAP_EXECUTE, 0, 0, bytes, address);
        check(guest != nullptr, "MapViewOfFileEx guest memory");
        if (reinterpret_cast<std::uintptr_t>(guest) % alignment != 0 || (address != nullptr && guest != address)) throw std::runtime_error("guest backing view address mismatch");
        DWORD previous = 0;
        check(VirtualProtect(guest, bytes, nativeProtection(protection), &previous) != FALSE, "VirtualProtect guest backing view");
        return {reinterpret_cast<std::uintptr_t>(guest), bytes, alias, reinterpret_cast<std::uintptr_t>(section)};
    } catch (...) {
        if (guest != nullptr) check(UnmapViewOfFile(guest) != FALSE, "UnmapViewOfFile failed guest view");
        if (alias != nullptr) check(UnmapViewOfFile(alias) != FALSE, "UnmapViewOfFile failed guest alias");
        check(CloseHandle(section) != FALSE, "CloseHandle failed guest backing");
        throw;
    }
}

void Unmap(const Mapping& mapping) {
    check(UnmapViewOfFile(reinterpret_cast<void*>(mapping.address)) != FALSE, "UnmapViewOfFile guest view");
    check(UnmapViewOfFile(mapping.alias) != FALSE, "UnmapViewOfFile guest alias");
    check(CloseHandle(reinterpret_cast<HANDLE>(mapping.handle)) != FALSE, "CloseHandle guest backing");
}

void Deactivate(std::uint64_t address, std::size_t bytes) {
    DWORD previous = 0;
    check(VirtualProtect(reinterpret_cast<void*>(address), bytes, PAGE_NOACCESS, &previous) != FALSE, "VirtualProtect guest backing unmap");
}

}
