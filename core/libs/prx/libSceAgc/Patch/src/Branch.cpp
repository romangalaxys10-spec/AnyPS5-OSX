#include "prx/libSceAgc/Patch/include/Branch.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcBranchPatchSetCompareAddress(std::uint32_t* cmd, const volatile std::uint64_t* compareAddress) {
    (void)cmd;
    (void)compareAddress;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcBranchPatchSetThenTarget(std::uint32_t* cmd, const volatile std::uint32_t* target, std::uint32_t sizeInDwords) {
    (void)cmd;
    (void)target;
    (void)sizeInDwords;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcBranchPatchSetElseTarget(std::uint32_t* cmd, const volatile std::uint32_t* target, std::uint32_t sizeInDwords) {
    (void)cmd;
    (void)target;
    (void)sizeInDwords;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
