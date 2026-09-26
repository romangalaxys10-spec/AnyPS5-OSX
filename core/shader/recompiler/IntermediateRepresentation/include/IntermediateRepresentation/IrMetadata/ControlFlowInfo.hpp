#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_CONTROLFLOWINFO_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_CONTROLFLOWINFO_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "IntermediateRepresentation/IrValue.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace ShaderRecompiler {

struct BlockInfo {
    std::uint32_t id = 0;
    std::uint32_t startPc = 0;
    std::uint32_t endPc = 0;
    Terminator terminator;
    IrValue* condition = nullptr;
    IrValue* indirectTarget = nullptr;
};

struct DescriptorSource {
    struct IndirectImage {
        std::uint32_t materialSource = 0;
        std::uint32_t heapSource = 0;
        std::uint32_t selectorStride = 0;
        std::uint32_t selectorOffset = 0;
        std::uint32_t keyArg = 0;

        bool operator==(const IndirectImage& other) const = default;
    };

    std::array<IrValue*, 8> dwords {};
    std::uint32_t dwordCount = 0;
    std::optional<IndirectImage> indirectImage;

    bool operator==(const DescriptorSource& other) const = default;
};

struct SrtRead {
    IrValue* value = nullptr;
    std::uint32_t flatOffset = 0;

    bool operator==(const SrtRead& other) const = default;
};

struct ResourceBlock {
    IrValue* condition = nullptr;
    std::vector<std::uint32_t> successors;
    std::vector<std::uint32_t> sources;
};

}

#endif
