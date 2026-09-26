#include "prx/libSceAgc/Misc/include/PacketInfo.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t APS5_VABI sceAgcGetPacketSize(uint32_t* packet) {
 (void)packet;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("V++UgBtQhn0", sceAgcGetDataPacketPayloadAddressUnk);
int APS5_VABI sceAgcGetDataPacketPayloadAddressUnk(uint32_t** addr, uint32_t* cmd, int type) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(addr), alignof(uint32_t*), __func__);
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), alignof(uint32_t), __func__);
    if (type != 0) {
        *addr = cmd + 2;
    } else {
        *addr = (~cmd[0] & 0x3fff0000u) != 0 ? cmd + 1 : nullptr;
    }
    return 0;
}

}
