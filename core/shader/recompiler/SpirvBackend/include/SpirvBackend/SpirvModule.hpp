#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMODULE_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMODULE_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <set>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace ShaderRecompiler {

struct SpirvTypeAnnotation {
    std::uint32_t opcode = 0;
    std::vector<std::uint32_t> operands;
};

struct SpirvDeferredPhi {
    std::size_t wordOffset = 0;
};

class SpirvModule {
public:
    explicit SpirvModule(std::uint32_t version = 0x00010300u);
    [[nodiscard]] std::uint32_t AllocateId();
    void EmitCapability(std::uint32_t capability);
    void EmitExtension(const std::string& extensionName);
    void EmitEntryPoint(std::uint32_t executionModel, std::uint32_t entryPointId, const std::string& entryPointName, const std::vector<std::uint32_t>& interfaceIds);
    void EmitTypeDeclaration(std::vector<std::uint32_t> words);
    void EmitGlobalVariable(std::vector<std::uint32_t> words);
    void EmitFunctionInstruction(std::vector<std::uint32_t> words);
    [[nodiscard]] std::vector<std::uint32_t> Finalize() const;

    void RequireVersion(std::uint32_t version);
    [[nodiscard]] std::uint32_t Import(const std::string& name);
    [[nodiscard]] std::uint32_t DefineGlobalVariable(std::uint32_t pointerType, std::uint32_t storageClass);
    void DefineGlobalVariable(std::uint32_t id, std::uint32_t pointerType, std::uint32_t storageClass);
    void AddMemoryModel(std::uint32_t addressingModel, std::uint32_t memoryModel);
    void AddName(std::uint32_t target, const std::string& name);
    void AddFunction(std::span<const std::uint32_t> words);
    [[nodiscard]] SpirvDeferredPhi AddDeferredPhi(std::uint32_t type, std::uint32_t result, std::size_t incomingCount);
    void PatchDeferredPhi(SpirvDeferredPhi phi, std::size_t incoming, std::uint32_t value, std::uint32_t parent);

    template<typename... TOperands>
    std::uint32_t Type(std::uint32_t opcode, const TOperands&... operands) {
        return declareType(opcode, makeTypeKey(opcode, operands...));
    }

    template<typename... TOperands>
    std::uint32_t DecoratedType(std::uint32_t opcode, std::initializer_list<SpirvTypeAnnotation> annotations, const TOperands&... operands) {
        return declareDecoratedType(opcode, makeTypeKey(opcode, operands...), annotations);
    }

    template<typename... TOperands>
    std::uint32_t Constant(std::uint32_t opcode, std::uint32_t type, const TOperands&... operands) {
        std::vector<std::uint32_t> key;
        key.reserve(2u + (0u + ... + operandWordCount(operands)));
        appendOperands(key, opcode, type, operands...);
        return declareConstant(opcode, std::move(key));
    }

    template<typename... TOperands>
    void AddExecutionMode(std::uint32_t entryPoint, std::uint32_t mode, const TOperands&... operands) {
        appendInstruction(executionModes, spv::OpExecutionMode, entryPoint, mode, operands...);
    }

    template<typename... TOperands>
    void AddAnnotation(std::uint32_t opcode, const TOperands&... operands) {
        appendInstruction(annotations, opcode, operands...);
    }

    template<typename... TOperands>
    void AddFunction(std::uint32_t opcode, const TOperands&... operands) {
        appendInstruction(functionInstructions, opcode, operands...);
    }

private:
    static void appendOperand(std::vector<std::uint32_t>& words, std::uint32_t value) {
        words.push_back(value);
    }

    static void appendOperand(std::vector<std::uint32_t>& words, std::int32_t value) {
        words.push_back(static_cast<std::uint32_t>(value));
    }

    template<typename TEnum>
    requires std::is_enum_v<TEnum>
    static void appendOperand(std::vector<std::uint32_t>& words, TEnum value) {
        static_assert(sizeof(TEnum) == sizeof(std::uint32_t));
        words.push_back(static_cast<std::uint32_t>(value));
    }

    static void appendOperand(std::vector<std::uint32_t>& words, std::span<const std::uint32_t> values) {
        words.insert(words.end(), values.begin(), values.end());
    }

    template<typename... TOperands>
    static void appendOperands(std::vector<std::uint32_t>& words, const TOperands&... operands) {
        (appendOperand(words, operands), ...);
    }

    template<typename T>
    static std::size_t operandWordCount(const T& operand) {
        if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
            return 1;
        } else {
            return std::size(operand);
        }
    }

    template<typename... TOperands>
    static std::vector<std::uint32_t> makeTypeKey(std::uint32_t opcode, const TOperands&... operands) {
        const auto operandCount = static_cast<std::uint32_t>((0u + ... + operandWordCount(operands)));
        std::vector<std::uint32_t> key;
        key.reserve(2u + operandCount);
        appendOperands(key, opcode, operandCount, operands...);
        return key;
    }

    template<typename... TOperands>
    static void appendInstruction(std::vector<std::uint32_t>& section, std::uint32_t opcode, const TOperands&... operands) {
        const auto offset = section.size();
        appendOperands(section, opcode, operands...);
        const auto wordCount = static_cast<std::uint32_t>(section.size() - offset);
        section[offset] |= wordCount << spv::WordCountShift;
    }

    std::uint32_t declareType(std::uint32_t opcode, std::vector<std::uint32_t> key);
    std::uint32_t declareDecoratedType(std::uint32_t opcode, std::vector<std::uint32_t> key, std::initializer_list<SpirvTypeAnnotation> annotationList);
    std::uint32_t declareConstant(std::uint32_t opcode, std::vector<std::uint32_t> key);
    static void appendString(std::vector<std::uint32_t>& words, const std::string& text);

    std::uint32_t nextId = 1;
    std::uint32_t version = 0x00010300u;
    std::vector<std::uint32_t> extInstImports;
    std::vector<std::uint32_t> memoryModel;
    std::vector<std::uint32_t> executionModes;
    std::vector<std::uint32_t> debug;
    std::vector<std::uint32_t> annotations;
    std::vector<std::uint32_t> declarations;
    std::set<std::uint32_t> requiredCapabilities;
    std::set<std::string> requiredExtensions;
    std::map<std::string, std::uint32_t> importIds;
    std::map<std::vector<std::uint32_t>, std::uint32_t> declarationIds;
    std::size_t unpatchedPhiIncomings = 0;
    std::vector<std::uint32_t> capabilities;
    std::vector<std::uint32_t> extensions;
    std::vector<std::uint32_t> entryPoints;
    std::vector<std::uint32_t> typeDeclarations;
    std::vector<std::uint32_t> globalVariables;
    std::vector<std::uint32_t> functionInstructions;
};

}

#endif
