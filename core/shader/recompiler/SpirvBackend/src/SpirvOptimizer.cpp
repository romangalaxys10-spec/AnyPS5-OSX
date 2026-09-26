#include "SpirvBackend/SpirvOptimizer.hpp"
#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/optimizer.hpp>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

std::vector<std::uint32_t> ValidateAndOptimizeSpirv(std::span<const std::uint32_t> spirv, std::uint32_t vulkanVersion, std::uint32_t spirvVersion) {
    spv_target_env environment;
    const auto apiVersion = vulkanVersion & ~0xfffu;
    std::uint32_t maxSpirvVersion = 0;
    switch (apiVersion) {
    case 0x00400000u: environment = SPV_ENV_VULKAN_1_0; maxSpirvVersion = 0x00010000u; break;
    case 0x00401000u: environment = spirvVersion == 0x00010400u ? SPV_ENV_VULKAN_1_1_SPIRV_1_4 : SPV_ENV_VULKAN_1_1; maxSpirvVersion = 0x00010400u; break;
    case 0x00402000u: environment = SPV_ENV_VULKAN_1_2; maxSpirvVersion = 0x00010500u; break;
    case 0x00403000u: environment = SPV_ENV_VULKAN_1_3; maxSpirvVersion = 0x00010600u; break;
    case 0x00404000u: environment = SPV_ENV_VULKAN_1_4; maxSpirvVersion = 0x00010600u; break;
    default: throw std::runtime_error("SPIRV-Tools: unsupported Vulkan target");
    }
    if (spirvVersion < 0x00010000u || spirvVersion > maxSpirvVersion || (spirvVersion & 0xffu) != 0) {
        throw std::runtime_error("SPIRV-Tools: unsupported Vulkan/SPIR-V target");
    }
    if (spirv.size() >= 5 && spirv[1] > spirvVersion) {
        throw std::runtime_error("SPIRV-Tools: module version exceeds requested SPIR-V target");
    }
    std::string diagnostics;
    const auto consumer = [&diagnostics](spv_message_level_t level, const char* source, const spv_position_t& position, const char* message) {
        if (!diagnostics.empty()) diagnostics += '\n';
        diagnostics += "level=" + std::to_string(static_cast<int>(level)) + " word=" + std::to_string(position.index);
        if (source != nullptr && *source != '\0') diagnostics += " source=" + std::string(source);
        if (message != nullptr) diagnostics += ": " + std::string(message);
    };
    spvtools::SpirvTools tools(environment);
    if (!tools.IsValid()) throw std::runtime_error("SPIRV-Tools: cannot create validator");
    tools.SetMessageConsumer(consumer);
    if (!tools.Validate(spirv.data(), spirv.size())) {
        throw std::runtime_error("SPIR-V validation before optimization failed:\n" + diagnostics);
    }
    spvtools::Optimizer optimizer(environment);
    optimizer.SetMessageConsumer(consumer);
    optimizer.RegisterPerformancePasses(true);
    optimizer.SetValidateAfterAll(true);
    spvtools::OptimizerOptions options;
    options.set_preserve_bindings(true);
    options.set_preserve_spec_constants(true);
    std::vector<std::uint32_t> optimized;
    diagnostics.clear();
    if (!optimizer.Run(spirv.data(), spirv.size(), &optimized, options)) {
        throw std::runtime_error("SPIR-V optimization failed:\n" + diagnostics);
    }
    diagnostics.clear();
    if (!tools.Validate(optimized.data(), optimized.size())) {
        throw std::runtime_error("SPIR-V validation after optimization failed:\n" + diagnostics);
    }
    return optimized;
}

}
