#include "prx/libSceAgc/DcbState/include/Init.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_EXPORT("23LRUSvYu1M", sceAgcInit);
int APS5_VABI sceAgcInit(std::uint32_t* state, std::uint32_t version) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(state), alignof(std::uint32_t), __func__);
    Agc::Command::Require(version < 14, __func__, "unsupported AGC initialization version");
    return 0;
}

}
