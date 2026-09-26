#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

alignas(64) std::array<std::byte, 256> storage{};
std::size_t lastSize = 0;
std::size_t lastAlignment = 0;
unsigned initializes = 0;
unsigned frees = 0;
bool fail = false;
bool recurse = false;

void require(bool condition) {
    if (!condition) throw std::runtime_error("application heap test failed");
}

template<typename TAction>
void reject(TAction action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected);
}

void APS5_VABI initialize() { ++initializes; }
void APS5_VABI finalize() { require(initializes == 1); }

void* APS5_VABI allocate(std::size_t bytes) {
    lastSize = bytes;
    if (recurse) return ApplicationHeapAllocate_nid_no_patch(bytes);
    return fail ? nullptr : storage.data();
}

void APS5_VABI release(void* pointer) {
    require(pointer == storage.data());
    ++frees;
}

void* APS5_VABI reallocate(void* pointer, std::size_t bytes) {
    require(pointer == storage.data());
    return allocate(bytes);
}

void* APS5_VABI allocateZeroed(std::size_t count, std::size_t bytes) {
    return allocate(count * bytes);
}

void* APS5_VABI align(std::size_t alignment, std::size_t bytes) {
    lastAlignment = alignment;
    return allocate(bytes);
}

int APS5_VABI posixAlign(void** pointer, std::size_t alignment, std::size_t bytes) {
    if (fail) return 12;
    *pointer = align(alignment, bytes);
    return 0;
}

template<typename TValue, std::size_t TSize>
void write(std::array<std::byte, TSize>& data, std::size_t offset, TValue value) {
    require(offset <= data.size() && sizeof(value) <= data.size() - offset);
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

}

int main(int argc, char**) {
    reject([] { ApplicationHeapAllocate_nid_no_patch(64); });
    reject([] { ApplicationHeapRegister_nid_no_patch(nullptr); });
    std::array<std::byte, 0x40> process{};
    std::array<std::byte, 0x38> libc{};
    std::array<std::byte, 0x78> replacement{};
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x10, &initialize);
    write(replacement, 0x18, &finalize);
    write(replacement, 0x20, &allocate);
    write(replacement, 0x28, &release);
    write(replacement, 0x30, &allocateZeroed);
    write(replacement, 0x38, &reallocate);
    write(replacement, 0x40, &align);
    write(replacement, 0x48, &reallocate);
    write(replacement, 0x50, &posixAlign);
    if (argc > 1) {
        write(replacement, 8, std::uint64_t{99});
        reject([&] { ApplicationHeapInitialize_nid_no_patch(process.data()); });
        write(replacement, 8, std::uint64_t{2});
        reject([&] { ApplicationHeapInitialize_nid_no_patch(process.data()); });
        reject([] { ApplicationHeapAlign_nid_no_patch(4, 64); });
        require(initializes == 0);
        return 0;
    }
    ApplicationHeapInitialize_nid_no_patch(process.data());
    ApplicationHeapInitialize_nid_no_patch(process.data());
    require(initializes == 1);
    void* pointer = ApplicationHeapAlign_nid_no_patch(4, 64);
    require(pointer == storage.data() && lastAlignment == 4 && lastSize == 64);
    release(pointer);
    require(frees == 1);
    pointer = ApplicationHeapAllocate_nid_no_patch(32);
    require(pointer == storage.data() && lastSize == 32);
    require(ApplicationHeapReallocate_nid_no_patch(pointer, 96) == storage.data() && lastSize == 96);
    ApplicationHeapFree_nid_no_patch(pointer);
    require(frees == 2);
    require(ApplicationHeapCalloc_nid_no_patch(3, 16) == storage.data() && lastSize == 48);
    require(ApplicationHeapPosixAlign_nid_no_patch(&pointer, 64, 128) == 0 && pointer == storage.data() && lastAlignment == 64);
    reject([] { ApplicationHeapAlign_nid_no_patch(3, 64); });
    reject([] { ApplicationHeapCalloc_nid_no_patch(2, std::numeric_limits<std::size_t>::max()); });
    fail = true;
    reject([] { ApplicationHeapAlign_nid_no_patch(4, 64); });
    reject([] { ApplicationHeapAllocate_nid_no_patch(64); });
    void* unchanged = storage.data();
    reject([&] { ApplicationHeapPosixAlign_nid_no_patch(&unchanged, 64, 64); });
    require(unchanged == storage.data());
    fail = false;
    recurse = true;
    reject([] { ApplicationHeapAllocate_nid_no_patch(64); });
    recurse = false;
    require(ApplicationHeapAllocate_nid_no_patch(64) == storage.data());
}
