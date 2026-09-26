#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNADESCRIPTORFORMAT_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNADESCRIPTORFORMAT_HPP

#include "IntermediateRepresentation/IrMetadata.hpp"
#include <cstdint>

namespace ShaderRecompiler {

enum class ImageType : std::uint32_t {
    Color1D = 8,
    Color2D = 9,
    Color3D = 10,
    Cube = 11,
    Color1DArray = 12,
    Color2DArray = 13,
    Color2DMsaa = 14,
    Color2DMsaaArray = 15
};

[[nodiscard]] bool IsFmaskTextureFormat(IrBufferFormat format);
[[nodiscard]] IrTextureNumericClass SampledTextureNumericClass(IrBufferFormat format);
[[nodiscard]] IrBufferFormat RemapTextureFormat(IrBufferFormat format);

}

#endif
