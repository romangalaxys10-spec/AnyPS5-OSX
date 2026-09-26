#include "prx/libSceAgc/Command/include/Draw.hpp"
#include "prx/libSceAgc/DcbDraw/include/DrawIndexed.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbDrawIndex(CommandBuffer* buf, uint32_t index_count, const volatile void* index_addr, uint64_t modifier) {
 (void)buf;
 (void)index_count;
 (void)index_addr;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndexGetSize() {
    return 24;
}

std::uint32_t* APS5_VABI sceAgcDcbDrawIndexAuto(CommandBuffer* buf, std::uint32_t indexCount, std::uint64_t modifier) {
    return Agc::Command::Emit(buf, 0x2du, {indexCount, Agc::Command::DrawInitiator(modifier, false, __func__)}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndexAutoGetSize() {
    return 12;
}

std::uint32_t* APS5_VABI sceAgcDcbDrawIndexOffset(CommandBuffer* buf, std::uint32_t indexOffset, std::uint32_t indexCount, std::uint64_t modifier) {
    return Agc::Command::Emit(buf, 0x35u, {indexCount == 0 ? 1u : indexCount, indexOffset, indexCount, Agc::Command::DrawInitiator(modifier, true, __func__)}, __func__);
}

uint32_t APS5_VABI sceAgcDcbDrawIndexOffsetGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndexIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint64_t modifier) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndexIndirectGetSize() {
    return 20;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndexIndirectMulti(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t count_indirect, uint32_t max_count_or_count, const volatile void* count_addr, uint32_t stride_in_bytes, uint64_t modifier) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)count_indirect;
 (void)max_count_or_count;
 (void)count_addr;
 (void)stride_in_bytes;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndexIndirectMultiGetSize() {
    return 40;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndexMultiInstanced(CommandBuffer* buf, uint32_t index_count, const volatile void* index_addr, const volatile void* object_ids, uint32_t instance_count, uint64_t modifier) {
 (void)buf;
 (void)index_count;
 (void)index_addr;
 (void)object_ids;
 (void)instance_count;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbDrawIndexMultiInstancedGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
