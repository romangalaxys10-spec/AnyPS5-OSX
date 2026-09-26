#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <system_error>

#if defined(__linux__)
#include <sys/mman.h>
#else
#include <windows.h>

static constexpr int PROT_NONE = 0;
static constexpr int PROT_READ = 1;
static constexpr int PROT_WRITE = 2;
static constexpr int PROT_EXEC = 4;

static DWORD WinProtFromPosix(int prot) {
    if (prot == PROT_NONE) return PAGE_NOACCESS;
    if ((prot & PROT_EXEC) && (prot & PROT_WRITE)) return PAGE_EXECUTE_READWRITE;
    if ((prot & PROT_EXEC) && (prot & PROT_READ)) return PAGE_EXECUTE_READ;
    if (prot & PROT_EXEC) return PAGE_EXECUTE;
    if (prot & PROT_WRITE) return PAGE_READWRITE;
    return PAGE_READONLY;
}

static int mprotect(void* addr, size_t len, int prot) {
    DWORD old;
    if (!VirtualProtect(addr, len, WinProtFromPosix(prot), &old))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualProtect failed");
    return 0;
}
#endif

namespace {

void ValidateLength(size_t len) {
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Memory length must be a positive multiple of the guest page size");
    }
}

size_t ValidateAlignment(size_t alignment) {
    if (alignment == 0) return PS5_PAGE_SIZE;
    if (alignment < PS5_PAGE_SIZE || (alignment & (alignment - 1)) != 0) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Memory alignment must be a power of two no smaller than the guest page size");
    }
    return alignment;
}

void ValidateRange(const void* addr, size_t len, size_t alignment) {
    ValidateLength(len);
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    if (!addr || (start & (alignment - 1)) != 0 || len > std::numeric_limits<std::uintptr_t>::max() - start) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Invalid memory address, alignment or range");
    }
}

int LinuxProtFromSce(int prot) {
    if ((prot & ~0xF7) != 0) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Unsupported memory protection bits");
    }
    int result = PROT_NONE;
    if (prot & 0x13) result |= PROT_READ;
    if (prot & 0x22) result |= PROT_READ | PROT_WRITE;
    if (prot & 4) result |= PROT_READ | PROT_EXEC;
    return result;
}

void Unmap(void* addr, size_t len) {
    GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(addr, len);
}

void* MapAligned(void* addr, size_t len, int prot, int flags, size_t alignment) {
    ValidateLength(len);
    alignment = ValidateAlignment(alignment);
    constexpr int guestMapFixed = 0x10;
    constexpr int guestMapNoCoalesce = 0x400000;
    if ((flags & ~(guestMapFixed | guestMapNoCoalesce)) != 0) throw std::invalid_argument("Unsupported memory mapping flags");
    if ((flags & guestMapFixed) != 0) ValidateRange(addr, len, alignment);
    else if (addr != nullptr) throw std::invalid_argument("Non-fixed mapping address hints are not implemented");
    return GuestMemoryBacking::GuestMemoryBackingMap_nid_postfix(addr, len, alignment, prot);
}

void ValidateOutput(void** addr) {
    if (!addr) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Null memory mapping output");
    }
}

}

int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    if (physStart < 0 || (static_cast<std::uint64_t>(physStart) & (PS5_PAGE_SIZE - 1)) != 0 || static_cast<std::uint64_t>(physStart) >= DIRECT_MEMORY_SIZE || len > DIRECT_MEMORY_SIZE - static_cast<std::uint64_t>(physStart)) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    GuestAllocations::Mutation mutation;
    if (*addr != nullptr) mutation.RequireAvailable(*addr, len);
    void* mapped = MapAligned(*addr, len, LinuxProtFromSce(prot), flags, alignment);
    try {
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    return 0;
}

int DoMapAnon(void** addr, size_t len, int prot, int flags) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    if (*addr != nullptr) mutation.RequireAvailable(*addr, len);
    void* mapped = MapAligned(*addr, len, LinuxProtFromSce(prot), flags, PS5_PAGE_SIZE);
    try {
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    return 0;
}

int DoMprotect(const void* addr, size_t len, int prot) {
    const auto address = reinterpret_cast<std::uintptr_t>(addr);
    constexpr auto pageMask = static_cast<std::uintptr_t>(PS5_PAGE_SIZE - 1);
    const auto limit = std::numeric_limits<std::uintptr_t>::max();
    if (address == 0 || len == 0 || len > limit - address || address + len > limit - pageMask) throw std::invalid_argument("Invalid guest memory protection range");
    const auto first = address & ~pageMask;
    const auto end = (address + len + pageMask) & ~pageMask;
    const auto bytes = static_cast<std::size_t>(end - first);
    const auto* pointer = reinterpret_cast<const void*>(first);
    const auto nativeProtection = LinuxProtFromSce(prot);
    GuestAllocations::Mutation mutation;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(pointer, &memory, sizeof(memory)) != sizeof(memory)) throw std::runtime_error("Cannot query guest memory protection range");
    if (memory.Type == MEM_IMAGE) {
        if (memory.AllocationBase != GetModuleHandleW(nullptr)) throw std::invalid_argument("Memory protection of a foreign image is not supported");
        mutation.RegisterMainImage();
    }
#else
    mutation.RegisterMainImage();
#endif
    mutation.Protect(pointer, bytes, (prot & 3) != 0, (prot & 2) != 0, [&] {
        if (mprotect(const_cast<void*>(pointer), bytes, nativeProtection) != 0) throw std::system_error(errno, std::generic_category(), "mprotect failed");
    });
    return 0;
}

int DoMunmap(void* addr, size_t len) {
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0 || !addr) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    mutation.Unmap(addr, len, [&](const void*, bool) {
        Unmap(addr, len);
    });
    return 0;
}

int DoReserveVirtual(void** addr, size_t len, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    void* mapped = MapAligned(nullptr, len, PROT_NONE, 0, alignment);
    try {
        mutation.Add(mapped, len, false, false);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    return 0;
}
