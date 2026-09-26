#include "prx/libc/include/MemoryBackingPlatform.hpp"
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <sys/mman.h>
#include <unistd.h>

namespace GuestMemoryBacking::Platform {
namespace {

void check(bool success, const char* operation) {
    if (!success) throw std::system_error(errno, std::generic_category(), operation);
}

}

Mapping Map(void* address, std::size_t bytes, std::size_t alignment, int protection) {
    if (bytes > static_cast<std::size_t>(std::numeric_limits<off_t>::max()) || bytes > std::numeric_limits<std::size_t>::max() - alignment) throw std::overflow_error("guest backing size overflow");
    int descriptor = memfd_create("AnyPS5 guest memory", MFD_CLOEXEC);
    check(descriptor >= 0, "memfd_create guest backing");
    void* alias = MAP_FAILED;
    void* guest = MAP_FAILED;
    void* reservation = MAP_FAILED;
    std::size_t reservationBytes = 0;
    try {
        check(ftruncate(descriptor, static_cast<off_t>(bytes)) == 0, "ftruncate guest backing");
        alias = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, descriptor, 0);
        check(alias != MAP_FAILED, "mmap guest backing alias");
        if (address == nullptr) {
            reservationBytes = bytes + alignment;
            reservation = mmap(nullptr, reservationBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        } else {
            reservationBytes = bytes;
            reservation = mmap(address, reservationBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        }
        check(reservation != MAP_FAILED, "mmap guest backing reservation");
        if (address != nullptr && reservation != address) throw std::runtime_error("fixed guest backing reservation address mismatch");
        const auto first = reinterpret_cast<std::uintptr_t>(reservation);
        const auto aligned = (first + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
        guest = mmap(reinterpret_cast<void*>(aligned), bytes, protection, MAP_SHARED | MAP_FIXED, descriptor, 0);
        check(guest != MAP_FAILED, "mmap guest backing view");
        const auto prefix = aligned - first;
        const auto suffix = reservationBytes - prefix - bytes;
        if (prefix != 0) check(munmap(reservation, prefix) == 0, "munmap guest backing prefix");
        if (suffix != 0) check(munmap(reinterpret_cast<void*>(aligned + bytes), suffix) == 0, "munmap guest backing suffix");
        reservation = MAP_FAILED;
        const auto closeResult = close(descriptor);
        descriptor = -1;
        check(closeResult == 0, "close guest backing descriptor");
        return {aligned, bytes, alias, 0};
    } catch (...) {
        if (reservation != MAP_FAILED) check(munmap(reservation, reservationBytes) == 0, "munmap failed guest reservation");
        else if (guest != MAP_FAILED) check(munmap(guest, bytes) == 0, "munmap failed guest view");
        if (alias != MAP_FAILED) check(munmap(alias, bytes) == 0, "munmap failed guest alias");
        if (descriptor >= 0) check(close(descriptor) == 0, "close failed guest backing");
        throw;
    }
}

void Unmap(const Mapping& mapping) {
    check(munmap(reinterpret_cast<void*>(mapping.address), mapping.bytes) == 0, "munmap guest view");
    check(munmap(mapping.alias, mapping.bytes) == 0, "munmap guest alias");
}

void Deactivate(std::uint64_t address, std::size_t bytes) {
    check(mprotect(reinterpret_cast<void*>(address), bytes, PROT_NONE) == 0, "mprotect guest backing unmap");
}

}
