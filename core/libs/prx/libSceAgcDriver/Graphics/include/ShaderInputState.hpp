#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERINPUTSTATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERINPUTSTATE_HPP

#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "Recompiler.hpp"
#include <cstdint>
#include <span>

namespace AgcDriver::Graphics {

ShaderRecompiler::ShaderPixelStageInfo DecodePixelStageInfo(const Registers& context, bool hasColorTarget, std::uint8_t colorComponentMapping);
ShaderRecompiler::ShaderComputeStageInfo DecodeComputeStageInfo(const Registers& shader);
ShaderRecompiler::ShaderVertexStageInfo DecodeVertexStageInfo(std::span<const std::byte> header, std::uint64_t headerAddress, std::span<const std::uint32_t> userData);

}

#endif
