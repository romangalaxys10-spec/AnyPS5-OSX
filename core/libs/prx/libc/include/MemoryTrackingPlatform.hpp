#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_MEMORYTRACKINGPLATFORM_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_MEMORYTRACKINGPLATFORM_HPP

#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <vector>

namespace GuestMemoryTracking::Platform {

struct Region {
    std::uint64_t address;
    std::size_t bytes;
    std::uint64_t protection;
};

using FaultHandler = bool (*)(std::uint64_t, bool);
std::size_t PageSize();
void Install(FaultHandler handler);
std::vector<Region> Query(std::uint64_t address, std::size_t bytes);
void Protect(std::uint64_t address, std::size_t bytes, Protection protection);
void Restore(const std::vector<Region>& regions);

}

#endif
