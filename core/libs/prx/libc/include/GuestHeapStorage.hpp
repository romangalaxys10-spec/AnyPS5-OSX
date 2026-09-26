#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTHEAPSTORAGE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTHEAPSTORAGE_HPP

#include <cstddef>

namespace GuestHeapStorage {

void* Allocate(std::size_t alignment, std::size_t bytes);
void Free(void* pointer);

}

#endif
