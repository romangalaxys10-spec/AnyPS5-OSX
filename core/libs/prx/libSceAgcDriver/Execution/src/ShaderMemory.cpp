#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace AgcDriver {

ShaderMemory::ShaderMemory(std::span<const ShaderRecompiler::MemoryRegion> initial) {
    const ShaderRecompiler::RequestMemoryView validated(initial);
    for (const auto& region : initial) {
        regions.emplace(region.guestAddress, std::vector<std::byte>(region.bytes.begin(), region.bytes.end()));
    }
}

bool ShaderMemory::read(void* context, std::uint64_t address, std::uint32_t* value) {
    auto& self = *static_cast<ShaderMemory*>(context);
    if (address % sizeof(*value) != 0 || address > std::numeric_limits<std::uint64_t>::max() - sizeof(*value)) {
        throw std::runtime_error("AGC driver: invalid shader memory read address");
    }
    const auto next = self.regions.upper_bound(address);
    if (next != self.regions.begin()) {
        const auto previous = std::prev(next);
        const auto offset = address - previous->first;
        if (offset < previous->second.size()) {
            if (previous->second.size() - offset < sizeof(*value)) {
                throw std::runtime_error("AGC driver: shader memory read crosses a snapshot boundary");
            }
            std::memcpy(value, previous->second.data() + offset, sizeof(*value));
            return true;
        }
    }
    if (next != self.regions.end() && next->first - address < sizeof(*value)) {
        throw std::runtime_error("AGC driver: shader memory read overlaps a snapshot boundary");
    }
    std::vector<std::byte> bytes(sizeof(*value));
    GuestMemory::Read(address, bytes, alignof(std::uint32_t));
    std::memcpy(value, bytes.data(), sizeof(*value));
    self.regions.emplace(address, std::move(bytes));
    return true;
}

void ShaderMemory::Capture(const ShaderRecompiler::RecompileRequest& request) {
    const auto plan = ShaderRecompiler::GetResourcePlan(request);
    constexpr ShaderRecompiler::ResourceMaterializer materializer;
    ShaderRecompiler::SrtRuntime runtime;
    runtime.userData = request.context.userData;
    runtime.shaderBase = request.shader.codeAddress;
    runtime.userContext = this;
    runtime.readMemory = &read;
    runtime.readSpecializationMemory = &read;
    ShaderRecompiler::ResourceSnapshot snapshot;
    ShaderRecompiler::ResourceSpecialization specialization;
    materializer.Materialize(*plan, runtime, snapshot, specialization);
}

std::vector<ShaderRecompiler::MemoryRegion> ShaderMemory::Regions() const {
    std::vector<ShaderRecompiler::MemoryRegion> result;
    result.reserve(regions.size());
    for (const auto& [address, bytes] : regions) {
        result.push_back({address, bytes});
    }
    return result;
}

}
