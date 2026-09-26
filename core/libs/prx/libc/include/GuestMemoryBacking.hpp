#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTMEMORYBACKING_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTMEMORYBACKING_HPP

#include <cstddef>
#include <cstdint>

namespace GuestMemoryBacking {

extern "C" {
void* GuestMemoryBackingMap_nid_postfix(void* address, std::size_t bytes, std::size_t alignment, int protection);
void GuestMemoryBackingUnmap_nid_postfix(void* address, std::size_t bytes);
void GuestMemoryBackingRequire_nid_postfix(std::uint64_t address, std::size_t bytes);
void GuestMemoryBackingWrite_nid_postfix(std::uint64_t address, const void* source, std::size_t bytes);
}

}

#endif
