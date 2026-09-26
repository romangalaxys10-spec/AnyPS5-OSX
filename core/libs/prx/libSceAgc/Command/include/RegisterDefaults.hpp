#ifndef CORE_LIBS_PRX_LIBSCEAGC_COMMAND_INCLUDE_REGISTERDEFAULTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_COMMAND_INCLUDE_REGISTERDEFAULTS_HPP

#include <cstdint>

namespace Agc::Command {

void* GetRegisterDefaults(std::uint32_t version, bool internal, const char* function);

}

#endif
