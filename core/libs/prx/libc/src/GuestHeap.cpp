#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeapStorage.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace GuestHeap {
namespace {

void* allocate(GuestAllocations::Mutation& mutation, std::size_t alignment, std::size_t bytes) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) throw std::invalid_argument("invalid guest heap alignment");
    alignment = std::max(alignment, alignof(void*));
    void* pointer = GuestHeapStorage::Allocate(alignment, bytes);
    try {
        mutation.Add(pointer, bytes, true, true);
    } catch (...) {
        GuestHeapStorage::Free(pointer);
        throw;
    }
    return pointer;
}

void free(GuestAllocations::Mutation& mutation, void* pointer) {
    mutation.Remove(pointer);
    GuestHeapStorage::Free(pointer);
}

}

void* GuestHeapAllocate_nid_postfix(std::size_t bytes) {
    GuestAllocations::Mutation mutation;
    return allocate(mutation, alignof(std::max_align_t), bytes);
}

void GuestHeapFree_nid_postfix(void* pointer) {
    if (pointer == nullptr) return;
    GuestAllocations::Mutation mutation;
    const auto range = mutation.Find(pointer);
    mutation.RequireUnpinned(pointer, range.bytes);
    free(mutation, pointer);
}

void* GuestHeapReallocate_nid_postfix(void* pointer, std::size_t bytes) {
    if (pointer == nullptr) return GuestHeapAllocate_nid_postfix(bytes);
    GuestAllocations::Mutation mutation;
    const auto range = mutation.Find(pointer);
    mutation.RequireUnpinned(pointer, range.bytes);
    if (bytes == 0) {
        free(mutation, pointer);
        return nullptr;
    }
    void* result = allocate(mutation, alignof(std::max_align_t), bytes);
    std::memcpy(result, pointer, std::min(bytes, range.bytes));
    free(mutation, pointer);
    return result;
}

void* GuestHeapAlign_nid_postfix(std::size_t alignment, std::size_t bytes) {
    GuestAllocations::Mutation mutation;
    return allocate(mutation, alignment, bytes);
}

}
