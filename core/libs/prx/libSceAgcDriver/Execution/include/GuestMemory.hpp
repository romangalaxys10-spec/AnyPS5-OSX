#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GUESTMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GUESTMEMORY_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace AgcDriver::GuestMemory {

void CheckRange(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable = false);
void CheckGpuRange(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable = false);
void Read(std::uint64_t address, std::span<std::byte> destination, std::size_t alignment = 1);
void Write(std::uint64_t address, std::span<const std::byte> source, std::size_t alignment = 1);

}

extern "C" void AgcDriverCheckGuestMemory_nid_postfix(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable = false);

#endif
