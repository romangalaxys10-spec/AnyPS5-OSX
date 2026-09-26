#include "prx/libc/include/GuestHeapStorage.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <algorithm>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace GuestHeapStorage {
namespace {

struct Chunk {
    void* address;
    std::size_t bytes;
    std::map<std::uintptr_t, std::size_t> free;
};

struct Allocation {
    Chunk* chunk;
    std::size_t bytes;
};

struct Arena {
    std::vector<std::unique_ptr<Chunk>> chunks;
    std::map<std::uintptr_t, Allocation> allocations;
};

Arena& arena() {
    static auto* value = new Arena;
    return *value;
}

}

void* Allocate(std::size_t alignment, std::size_t bytes) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) throw std::invalid_argument("invalid shared heap alignment");
    bytes = std::max(bytes, std::size_t{1});
    if (bytes > std::numeric_limits<std::size_t>::max() - alignment) throw std::length_error("shared heap allocation overflow");
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    auto& state = arena();
    for (std::size_t index = 0;; ++index) {
        if (index == state.chunks.size()) {
            constexpr std::size_t chunkSize = 64 * 1024 * 1024;
            const auto granularity = std::max(alignment, std::size_t{65536});
            const auto requested = std::max(bytes, chunkSize);
            if (requested > std::numeric_limits<std::size_t>::max() - (granularity - 1)) throw std::length_error("shared heap chunk overflow");
            const auto size = (requested + granularity - 1) & ~(granularity - 1);
            void* memory = GuestMemoryBacking::GuestMemoryBackingMap_nid_postfix(nullptr, size, granularity, 3);
            try {
                auto chunk = std::make_unique<Chunk>();
                chunk->address = memory;
                chunk->bytes = size;
                chunk->free.emplace(reinterpret_cast<std::uintptr_t>(memory), size);
                state.chunks.push_back(std::move(chunk));
            } catch (...) {
                GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(memory, size);
                throw;
            }
        }
        auto& chunk = *state.chunks[index];
        for (auto it = chunk.free.begin(); it != chunk.free.end(); ++it) {
            const auto first = it->first;
            const auto available = it->second;
            if (first > std::numeric_limits<std::uintptr_t>::max() - (alignment - 1)) throw std::overflow_error("shared heap address overflow");
            const auto address = (first + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
            const auto prefix = address - first;
            if (prefix > available || bytes > available - prefix) continue;
            if (!state.allocations.emplace(address, Allocation{&chunk, bytes}).second) throw std::runtime_error("duplicate shared heap allocation");
            const auto suffix = available - prefix - bytes;
            try {
                if (suffix != 0) chunk.free.emplace(address + bytes, suffix);
            } catch (...) {
                state.allocations.erase(address);
                throw;
            }
            if (prefix != 0) it->second = prefix;
            else chunk.free.erase(it);
            return reinterpret_cast<void*>(address);
        }
    }
}

void Free(void* pointer) {
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    auto& allocations = arena().allocations;
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto found = allocations.find(address);
    if (found == allocations.end()) throw std::invalid_argument("unknown shared heap allocation");
    auto* chunk = found->second.chunk;
    auto& free = chunk->free;
    auto [entry, inserted] = free.emplace(address, found->second.bytes);
    if (!inserted) throw std::runtime_error("shared heap allocation was already freed");
    allocations.erase(found);
    if (entry != free.begin()) {
        auto previous = std::prev(entry);
        if (previous->first + previous->second == entry->first) {
            previous->second += entry->second;
            free.erase(entry);
            entry = previous;
        }
    }
    const auto next = std::next(entry);
    if (next != free.end() && entry->first + entry->second == next->first) {
        entry->second += next->second;
        free.erase(next);
    }
    if (free.size() == 1 && entry->first == reinterpret_cast<std::uintptr_t>(chunk->address) && entry->second == chunk->bytes) {
        GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(chunk->address, chunk->bytes);
        std::erase_if(arena().chunks, [chunk](const auto& value) { return value.get() == chunk; });
    }
}

}
