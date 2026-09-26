#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAW_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAW_HPP

#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"

namespace AgcDriver::Graphics {

void Draw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots = {});

}

#endif
