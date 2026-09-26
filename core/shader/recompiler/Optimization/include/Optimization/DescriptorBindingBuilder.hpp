#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_DESCRIPTORBINDINGBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_DESCRIPTORBINDINGBUILDER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/BindingAllocator.hpp"
#include "Recompiler.hpp"

namespace ShaderRecompiler {

class DescriptorBindingBuilder {
public:
    void Populate(BindingAllocationResult& allocation, const IrProgram& program, const ResourceSnapshot& snapshot) const;
    void Populate(BindingAllocationResult& allocation, const ShaderInfo& info, IrShaderStage stage, std::uint32_t userDataBase, const ResourceSnapshot& snapshot) const;
};

}

#endif
