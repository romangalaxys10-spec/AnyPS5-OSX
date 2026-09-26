#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_MEMORYBACKINGPLATFORM_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_MEMORYBACKINGPLATFORM_HPP

#include <cstddef>
#include <cstdint>

namespace GuestMemoryBacking::Platform {

struct Mapping {
    std::uint64_t address;
    std::size_t bytes;
    void* alias;
    std::uintptr_t handle;
};

Mapping Map(void* address, std::size_t bytes, std::size_t alignment, int protection);
void Unmap(const Mapping& mapping);
void Deactivate(std::uint64_t address, std::size_t bytes);

}

#endif
