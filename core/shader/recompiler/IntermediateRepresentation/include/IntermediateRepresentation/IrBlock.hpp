#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRBLOCK_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRBLOCK_HPP

#include "IntermediateRepresentation/IrValue.hpp"
#include <cstdint>
#include <array>
#include <list>
#include <vector>

namespace ShaderRecompiler {

class IrBlock {
public:
    explicit IrBlock(std::uint32_t id);
    IrBlock(const IrBlock& other) = delete;
    IrBlock& operator=(const IrBlock& other) = delete;
    IrBlock(IrBlock&& other) = delete;
    IrBlock& operator=(IrBlock&& other) = delete;

    [[nodiscard]] std::uint32_t Id() const;
    [[nodiscard]] std::list<IrValue*>& Instructions();
    [[nodiscard]] const std::list<IrValue*>& Instructions() const;
    [[nodiscard]] std::vector<IrBlock*>& Predecessors();
    [[nodiscard]] std::vector<IrBlock*>& Successors();

    void AppendInstruction(IrValue* value);
    void InsertInstructionBefore(IrValue* position, IrValue* value);
    void RemoveInstruction(IrValue* value);
    void AddPredecessor(IrBlock* block);
    void AddSuccessor(IrBlock* block);

    std::array<IrValue*, NumScalarRegs> ssaScalarValues{};
    std::array<IrValue*, NumScalarRegs> ssaThreadBitScalarValues{};
    std::array<IrValue*, NumScalarRegs> ssaScalarMaskTags{};
    std::array<IrValue*, NumVectorRegs> ssaVectorValues{};

    [[nodiscard]] const std::vector<IrBlock*>& Predecessors() const;
    [[nodiscard]] const std::vector<IrBlock*>& Successors() const;
    void SsaSeal();
    [[nodiscard]] bool IsSsaSealed() const;
    void AddBranch(IrBlock* block);
    [[nodiscard]] bool Empty() const;

private:
    bool ssaSealed = false;
    std::uint32_t id;
    std::list<IrValue*> instructions;
    std::vector<IrBlock*> predecessors;
    std::vector<IrBlock*> successors;
};

}

#endif
