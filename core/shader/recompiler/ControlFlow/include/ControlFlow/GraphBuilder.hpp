#ifndef CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_GRAPHBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_GRAPHBUILDER_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "RdnaDecoder/RdnaProgram.hpp"
#include <vector>

namespace ShaderRecompiler {

class GraphBuilder {
public:
    [[nodiscard]] ControlFlowGraph Build(const RdnaProgram& program) const;

private:
    [[nodiscard]] std::vector<BasicBlock> splitIntoBlocks(const RdnaProgram& program) const;
    void linkBlocks(std::vector<BasicBlock>& blocks, const RdnaProgram& program) const;
};

}

#endif
