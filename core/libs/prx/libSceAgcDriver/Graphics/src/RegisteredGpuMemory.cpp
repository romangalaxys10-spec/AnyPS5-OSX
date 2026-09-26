#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"

namespace AgcDriver::Graphics {

void GuestBufferMemory::AcquireRegistered() {
    Require(!uploaded && regions.empty() && lease.empty(), "guest allocation lease must precede resource registration");
    lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    regions.reserve(lease.size());
    for (const auto& range : lease) {
        validate(range->address, range->bytes);
        std::vector<std::byte> snapshot;
        if (!range->writable) {
            snapshot.resize(range->bytes);
            GuestMemory::Read(range->address, snapshot);
        }
        regions.push_back({range->address, range->address + range->bytes, range->writable, std::move(snapshot), nullptr});
    }
}

}
