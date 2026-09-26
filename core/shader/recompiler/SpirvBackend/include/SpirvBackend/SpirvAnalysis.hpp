#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVANALYSIS_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVANALYSIS_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace ShaderRecompiler {

struct SpirvRequirements {
    bool subgroupBallot = false;
    bool subgroupShuffle = false;
    bool subgroupLocalInvocationId = false;
    bool computeDerivatives = false;
    bool imageGatherExtended = false;
    bool functionLds = false;
    bool functionScratch = false;
    bool pixelValidMask = false;
    bool bufferInt64Atomics = false;
    std::vector<std::uint32_t> capabilities;
    std::vector<std::string> extensions;
};

[[nodiscard]] SpirvRequirements AnalyzeProgramRequirements(const IrProgram& program);

}

#endif
