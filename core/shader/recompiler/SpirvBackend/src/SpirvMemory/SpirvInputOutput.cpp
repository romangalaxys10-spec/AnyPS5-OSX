#include "SpirvBackend/SpirvMemory/SpirvInputOutput.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvSubgroup.hpp"
#include "SpirvBackend/SpirvMemory/SpirvBufferAccess.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <SpirvBackend/SpirvEmitterInstructions.hpp>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <array>
#include <span>
#include <algorithm>

namespace ShaderRecompiler
{
    namespace {

        constexpr std::uint32_t PsInputOffsetMask = 0x0000001fu;
        constexpr std::uint32_t PsInputFlatShade = 0x00000400u;
        constexpr std::uint32_t PixelParameterLimit = 32u;

        [[noreturn]] void FailEmit(const std::string& reason) {
            throw std::runtime_error("SPIR-V module emission failed: " + reason);
        }

        IrShaderStage StageOf(const SpirvEmitterState& state) {
            return state.program.Resources().stage;
        }

        const ShaderVertexInputInfo& VertexInfo(const SpirvEmitterState& state) {
            if (state.inputInfo.vertex == nullptr) {
                FailEmit("vertex input info is missing");
            }
            return *state.inputInfo.vertex;
        }

        const ShaderPixelInputInfo& PixelInfo(const SpirvEmitterState& state) {
            if (state.inputInfo.pixel == nullptr) {
                FailEmit("pixel input info is missing");
            }
            return *state.inputInfo.pixel;
        }

        bool IsVertexLikeStage(const SpirvEmitterState& state) {
            return StageOf(state) == IrShaderStage::Vertex || StageOf(state) == IrShaderStage::Local;
        }

        std::uint32_t PixelMappedLocation(const ShaderPixelInputInfo& info, std::uint32_t input) {
            if (input < info.inputNum) {
                if (input >= PixelParameterLimit) {
                    FailEmit("pixel interpolator index is out of range");
                }
                return info.interpolatorSettings[input] & PsInputOffsetMask;
            }
            return input;
        }

        bool PixelInputIsCustom(const ShaderPixelInputInfo& info, std::uint32_t input) {
            return input < PixelParameterLimit && (info.customInterpolationMask & (1u << input)) != 0u;
        }

        bool PixelInputIsFlat(const ShaderPixelInputInfo& info, std::uint32_t input) {
            return input < info.inputNum && input < PixelParameterLimit && (info.interpolatorSettings[input] & PsInputFlatShade) != 0u && !PixelInputIsCustom(info, input);
        }

        std::uint32_t PixelInputLocation(const ShaderPixelInputInfo& info, std::span<const std::uint32_t> activeInputs, std::uint32_t input) {
            std::array<bool, PixelParameterLimit> usedLocations{};
            for (const auto activeInput : activeInputs) {
                const auto mapped = PixelMappedLocation(info, activeInput);
                if (mapped >= usedLocations.size()) {
                    FailEmit("pixel parameter location is out of range");
                }
                usedLocations[mapped] = true;
            }
            std::array<std::uint32_t, PixelParameterLimit * 2u> groupLocations;
            groupLocations.fill(0xffffffffu);
            for (const auto activeInput : activeInputs) {
                const auto mapped = PixelMappedLocation(info, activeInput);
                const auto group = mapped * 2u + (PixelInputIsFlat(info, activeInput) ? 1u : 0u);
                auto& location = groupLocations[group];
                if (location == 0xffffffffu) {
                    location = mapped;
                    if (groupLocations[group ^ 1u] != 0xffffffffu) {
                        location = 0;
                        while (location < usedLocations.size() && usedLocations[location]) {
                            location++;
                        }
                        if (location >= usedLocations.size()) {
                            FailEmit("no free pixel parameter location");
                        }
                    }
                    usedLocations[location] = true;
                }
                if (activeInput == input) {
                    return location;
                }
            }
            return PixelMappedLocation(info, input);
        }

    }

    std::vector<FragmentParameter> DescribeFragmentParameters(const IrProgram& program, const ShaderStageInputInfo& inputInfo) {
        if (inputInfo.pixel == nullptr) FailEmit("pixel input info is missing");
        const auto& pixel = *inputInfo.pixel;
        std::vector<std::uint32_t> activeInputs;
        for (const auto& input : program.Info().inputs) {
            if (input.kind == StageInputKind::Parameter) activeInputs.push_back(input.location);
        }
        std::vector<FragmentParameter> result;
        for (const auto& input : program.Info().inputs) {
            if (input.kind != StageInputKind::Parameter) continue;
            const auto location = PixelInputLocation(pixel, activeInputs, input.location);
            if (std::any_of(result.begin(), result.end(), [&](const auto& parameter) { return parameter.location == location; })) continue;
            result.push_back({location, PixelMappedLocation(pixel, input.location), PixelInputIsFlat(pixel, input.location), input.perVertex});
        }
        return result;
    }

    std::uint32_t PixelParameterLocation(const SpirvEmitterState& state, std::uint32_t attr) {
        if (StageOf(state) != IrShaderStage::Pixel) {
            return attr;
        }
        std::array<std::uint32_t, PixelParameterLimit> activeInputs{};
        std::uint32_t activeCount = 0;
        for (const auto& input : state.inputs) {
            if (input.kind == StageInputKind::Parameter) {
                if (activeCount >= activeInputs.size()) {
                    FailEmit("too many pixel parameters");
                }
                activeInputs[activeCount++] = input.location;
            }
        }
        return PixelInputLocation(PixelInfo(state), std::span<const std::uint32_t>(activeInputs.data(), activeCount), attr);
    }

    bool PixelParameterIsFlat(const SpirvEmitterState& state, std::uint32_t attr) {
        return StageOf(state) == IrShaderStage::Pixel && PixelInputIsFlat(PixelInfo(state), attr);
    }

    bool PixelParameterIsCustom(const SpirvEmitterState& state, std::uint32_t attr) {
        return StageOf(state) == IrShaderStage::Pixel && PixelInputIsCustom(PixelInfo(state), attr);
    }

    VertexInputScalarKind VertexParameterScalarKind(const SpirvEmitterState& state, std::uint32_t location) {
        if (!IsVertexLikeStage(state)) {
            return VertexInputScalarKind::Float;
        }
        const auto& vertex = VertexInfo(state);
        if (location >= static_cast<std::uint32_t>(ShaderVertexInputInfo::MaxResources) || location >= static_cast<std::uint32_t>(vertex.resourcesNum)) {
            return VertexInputScalarKind::Float;
        }
        switch (vertex.resources[location].Format()) {
        case IrBufferFormat::Format8UInt:
        case IrBufferFormat::Format16UInt:
        case IrBufferFormat::Format8_8UInt:
        case IrBufferFormat::Format32UInt:
        case IrBufferFormat::Format16_16UInt:
        case IrBufferFormat::Format8_8_8_8UInt:
        case IrBufferFormat::Format32_32UInt:
        case IrBufferFormat::Format16_16_16_16UInt:
        case IrBufferFormat::Format32_32_32UInt:
        case IrBufferFormat::Format32_32_32_32UInt: return VertexInputScalarKind::Uint;
        case IrBufferFormat::Format8SInt:
        case IrBufferFormat::Format16SInt:
        case IrBufferFormat::Format8_8SInt:
        case IrBufferFormat::Format32SInt:
        case IrBufferFormat::Format16_16SInt:
        case IrBufferFormat::Format8_8_8_8SInt:
        case IrBufferFormat::Format32_32SInt:
        case IrBufferFormat::Format16_16_16_16SInt:
        case IrBufferFormat::Format32_32_32SInt:
        case IrBufferFormat::Format32_32_32_32SInt: return VertexInputScalarKind::Sint;
        default: return VertexInputScalarKind::Float;
        }
    }

    std::uint32_t VertexParameterComponentCount(const SpirvInputBinding& input) {
        return std::clamp(input.componentCount, 1u, 4u);
    }

    std::uint32_t VertexParameterScalarType(SpirvEmitterState& state, VertexInputScalarKind kind) {
        switch (kind) {
        case VertexInputScalarKind::Sint: return TypeI32(state);
        case VertexInputScalarKind::Uint: return TypeU32(state);
        case VertexInputScalarKind::Float: return TypeF32(state);
        }
        FailEmit("vertex input scalar kind is invalid");
    }

    std::uint32_t OutputVariableForExport(const SpirvEmitterState& state, const ExportInfo& exp) {
        if (exp.kind == ExportTargetKind::Position && exp.index == 0) {
            return state.perVertexVariable;
        }
        if (exp.kind == ExportTargetKind::MrtZ) {
            return state.depthVariable;
        }
        const auto expectedKind = exp.kind == ExportTargetKind::Mrt ? StageOutputKind::Mrt : StageOutputKind::Parameter;
        for (const auto& binding : state.outputs) {
            if (binding.kind == expectedKind && binding.index == exp.index) {
                return binding.variableId;
            }
        }
        return 0;
    }

    std::uint32_t InputVariableForKind(const SpirvEmitterState& state, StageInputKind kind) {
        for (const auto& input : state.inputs) {
            if (input.kind == kind) {
                return input.variableId;
            }
        }
        return 0;
    }

    const SpirvInputBinding* SpirvInputBindingForParameter(const SpirvEmitterState& state, std::uint32_t location) {
        for (const auto& input : state.inputs) {
            if (input.kind == StageInputKind::Parameter && input.location == location) {
                return &input;
            }
        }
        return nullptr;
    }

    std::uint32_t EmitVertexParameterComponentU32(SpirvEmitterState& state, const SpirvInputBinding& input, std::uint32_t component) {
        const auto count = VertexParameterComponentCount(input);
        const auto kind = VertexParameterScalarKind(state, input.location);
        const auto scalarType = VertexParameterScalarType(state, kind);
        const auto raw = state.module.AllocateId();
        if (count == 1u) {
            state.module.AddFunction(spv::OpLoad, scalarType, raw, input.variableId);
        } else {
            const auto pointerType = TypePointer(state, spv::StorageClassInput, scalarType);
            const auto pointer = state.module.AllocateId();
            state.module.AddFunction(spv::OpAccessChain, pointerType, pointer, input.variableId, ConstantU32(state, component));
            state.module.AddFunction(spv::OpLoad, scalarType, raw, pointer);
        }
        if (kind == VertexInputScalarKind::Uint) {
            return raw;
        }
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, raw);
        return bits;
    }

    std::uint32_t EmitInputComponentU32(SpirvEmitterState& state, StageInputKind kind, std::uint32_t component) {
        const auto variable = InputVariableForKind(state, kind);
        if (variable == 0) {
            FailEmit("stage input " + std::to_string(static_cast<std::uint32_t>(kind)) + " was not declared");
        }
        const auto pointer = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeU32(state)), pointer, variable, ConstantU32(state, component));
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return value;
    }

    std::uint32_t EmitLocalInvocationIndex(SpirvEmitterState& state) {
        const auto variable = InputVariableForKind(state, StageInputKind::LocalInvocationIndex);
        if (variable == 0) {
            FailEmit("LocalInvocationIndex was not declared");
        }
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, variable);
        if (state.laneCount == 2) {
            const auto waveBase = EmitBinaryU32(state, spv::OpBitwiseAnd, value, ConstantU32(state, ~31u));
            return EmitAddU32(state, EmitAddU32(state, value, waveBase), ConstantU32(state, state.laneHalf * 32));
        }
        return value;
    }
}
