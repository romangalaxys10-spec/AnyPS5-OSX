#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SHAREDMEMORYBARRIERINSERTER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SHAREDMEMORYBARRIERINSERTER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>

namespace ShaderRecompiler {

struct SharedMemoryBarrierStats {
    std::uint32_t insertedBarriers = 0;
};

class SharedMemoryBarrierInserter {
public:
    [[nodiscard]] SharedMemoryBarrierStats Insert(IrProgram& program, std::uint32_t waveSize) const;
};

}

#endif
