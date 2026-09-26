#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERMEMORY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "BdaAbi.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <memory>
#include <vector>

namespace AgcDriver::Graphics {

struct GuestMemorySnapshot {
    std::uint64_t address;
    std::span<const std::byte> bytes;
};

class GuestBufferMemory {
public:
    explicit GuestBufferMemory(const Context& context);
    void AcquireRegistered();
    void AddWritable(std::uint64_t address, std::size_t bytes);
    void AddSnapshot(const GuestMemorySnapshot& snapshot);
    void Upload(bool addressable);
    VkDescriptorBufferInfo Descriptor(std::uint64_t address, std::size_t bytes) const;
    std::vector<ShaderRecompiler::BdaAbi::Range> AddressRanges() const;
    void WriteBack();
    bool WritesOverlap(std::uint64_t address, std::size_t bytes) const;

private:
    struct Region {
        std::uint64_t begin;
        std::uint64_t end;
        bool writable;
        std::vector<std::byte> snapshot;
        std::unique_ptr<Buffer> buffer;
    };

    void validate(std::uint64_t address, std::size_t bytes) const;
    Context context;
    GuestAllocations::Lease lease;
    std::vector<Region> regions;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
    bool uploaded = false;
    bool committed = false;
};

}

#endif
