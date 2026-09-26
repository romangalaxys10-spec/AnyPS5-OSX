#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

namespace AgcDriver {

void Submit(const Packet* packet, std::uint32_t queue);

}

extern "C" void AgcDriverWaitIdle_nid_postfix();
extern "C" void AgcDriverSuspendPoint_nid_postfix();
extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader);

#endif
