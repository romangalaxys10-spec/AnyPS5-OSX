#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/MemoryBackingPlatform.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>

namespace GuestMemoryBacking {
namespace {

struct Allocation {
    Platform::Mapping mapping;
    std::map<std::uint64_t, std::uint64_t> ranges;
};

std::map<std::uint64_t, Allocation>& allocations() {
    static auto* value = new std::map<std::uint64_t, Allocation>;
    return *value;
}

Allocation& find(std::uint64_t address, std::size_t bytes) {
    if (address == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) throw std::invalid_argument("invalid guest backing range");
    auto found = allocations().upper_bound(address);
    if (found == allocations().begin()) throw std::runtime_error("guest memory has no shared backing");
    auto& allocation = std::prev(found)->second;
    auto range = allocation.ranges.upper_bound(address);
    if (range == allocation.ranges.begin() || address + bytes > std::prev(range)->second) throw std::runtime_error("guest memory backing range is unmapped");
    return allocation;
}

}

void* GuestMemoryBackingMap_nid_postfix(void* address, std::size_t bytes, std::size_t alignment, int protection) {
    const auto pageSize = GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix();
    if (bytes == 0 || bytes % pageSize != 0 || alignment < pageSize || (alignment & (alignment - 1)) != 0 || (protection & ~7) != 0) throw std::invalid_argument("invalid shared guest memory mapping");
    if (address != nullptr && reinterpret_cast<std::uintptr_t>(address) % alignment != 0) throw std::invalid_argument("misaligned fixed guest memory mapping");
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    const auto mapping = Platform::Map(address, bytes, alignment, protection);
    try {
        if (mapping.bytes > std::numeric_limits<std::uint64_t>::max() - mapping.address) throw std::overflow_error("guest backing mapping overflow");
        const auto next = allocations().lower_bound(mapping.address);
        if (next != allocations().end() && next->first < mapping.address + bytes) throw std::runtime_error("overlapping guest backing mappings");
        if (next != allocations().begin()) {
            const auto& previous = std::prev(next)->second.mapping;
            if (previous.address + previous.bytes > mapping.address) throw std::runtime_error("guest mapping overlaps a retained backing reservation");
        }
        Allocation allocation{mapping, {{mapping.address, mapping.address + bytes}}};
        if (!allocations().emplace(mapping.address, std::move(allocation)).second) throw std::runtime_error("duplicate guest backing mapping");
    } catch (...) {
        Platform::Unmap(mapping);
        throw;
    }
    return reinterpret_cast<void*>(mapping.address);
}

void GuestMemoryBackingUnmap_nid_postfix(void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto pageSize = GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix();
    if (address % pageSize != 0 || bytes % pageSize != 0) throw std::invalid_argument("misaligned guest backing unmap");
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    auto& allocation = find(address, bytes);
    auto replacement = allocation.ranges;
    const auto found = std::prev(replacement.upper_bound(address));
    const auto first = found->first;
    const auto last = found->second;
    replacement.erase(found);
    if (first < address) replacement.emplace(first, address);
    if (address + bytes < last) replacement.emplace(address + bytes, last);
    GuestMemoryTracking::GuestMemoryTrackingInvalidate_nid_postfix(address, bytes);
    if (replacement.empty()) {
        const auto base = allocation.mapping.address;
        Platform::Unmap(allocation.mapping);
        allocations().erase(base);
    } else {
        Platform::Deactivate(address, bytes);
        allocation.ranges.swap(replacement);
    }
}

void GuestMemoryBackingRequire_nid_postfix(std::uint64_t address, std::size_t bytes) {
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    static_cast<void>(find(address, bytes));
}

void GuestMemoryBackingWrite_nid_postfix(std::uint64_t address, const void* source, std::size_t bytes) {
    if (source == nullptr) throw std::invalid_argument("missing guest backing write source");
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    auto& allocation = find(address, bytes);
    auto* destination = static_cast<std::byte*>(allocation.mapping.alias) + (address - allocation.mapping.address);
    std::memcpy(destination, source, bytes);
}

}
