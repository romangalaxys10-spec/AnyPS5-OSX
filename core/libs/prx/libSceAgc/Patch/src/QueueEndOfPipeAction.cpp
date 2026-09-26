#include "prx/libSceAgc/Patch/include/QueueEndOfPipeAction.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcQueueEndOfPipeActionPatchAddress(std::uint32_t* cmd, const volatile Label* address) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), 4, __func__);
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    const auto opcode = (cmd[0] >> 8u) & 0xffu;
    if (opcode == 0x49u) {
        Agc::Command::ValidatePacket(cmd, opcode, 8, __func__);
        const auto dataSelect = cmd[2] >> 29u;
        Agc::Command::CheckAddress(guestAddress, dataSelect == 2 || dataSelect == 3 ? 8 : 4, __func__);
        cmd[3] = static_cast<std::uint32_t>(guestAddress);
        cmd[4] = static_cast<std::uint32_t>(guestAddress >> 32u);
    } else {
        Agc::Command::ValidatePacket(cmd, 0x47u, 6, __func__);
        const auto dataSelect = cmd[3] >> 29u;
        Agc::Command::CheckAddress(guestAddress, dataSelect == 2 || dataSelect == 3 ? 8 : 4, __func__);
        Agc::Command::CheckBits(guestAddress, 0xffffffffffffull, __func__);
        cmd[2] = static_cast<std::uint32_t>(guestAddress);
        cmd[3] = (cmd[3] & 0xffff0000u) | static_cast<std::uint32_t>(guestAddress >> 32u);
    }
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchData(uint32_t* cmd, uint32_t context_id, uint32_t data_sel, uint64_t data) {
 (void)cmd;
 (void)context_id;
 (void)data_sel;
 (void)data;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchGcrCntl(std::uint32_t* cmd, std::uint16_t gcrControl) {
    (void)cmd;
    (void)gcrControl;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchType(std::uint32_t* cmd, std::uint8_t action) {
    (void)cmd;
    (void)action;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
