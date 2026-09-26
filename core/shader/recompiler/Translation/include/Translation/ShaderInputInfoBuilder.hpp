#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_SHADERINPUTINFOBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_SHADERINPUTINFOBUILDER_HPP

#include "Optimization/ShaderStageInputInfo.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "Recompiler.hpp"

namespace ShaderRecompiler {

ShaderStageInputInfo BuildShaderStageInputInfo(ShaderStageKind stage, const GuestContext& context);

}

#endif
