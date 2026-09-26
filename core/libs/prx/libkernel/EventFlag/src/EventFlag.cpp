#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceKernelCreateEventFlag(KernelEventFlag* ef, const char* name, uint32_t attr, uint64_t init_pattern, const void* param) {
 (void)ef;
 (void)name;
 (void)attr;
 (void)init_pattern;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelDeleteEventFlag(KernelEventFlag ef) {
 (void)ef;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelSetEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
 (void)ef;
 (void)bit_pattern;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelClearEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
 (void)ef;
 (void)bit_pattern;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelCancelEventFlag(KernelEventFlag ef, uint64_t set_pattern, int* num_wait_threads) {
 (void)ef;
 (void)set_pattern;
 (void)num_wait_threads;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelPollEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat) {
 (void)ef;
 (void)bit_pattern;
 (void)wait_mode;
 (void)result_pat;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelWaitEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat, KernelUseconds* timeout) {
 (void)ef;
 (void)bit_pattern;
 (void)wait_mode;
 (void)result_pat;
 (void)timeout;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
