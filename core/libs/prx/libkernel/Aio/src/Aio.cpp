#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceKernelAioDeleteRequest(int32_t id, int32_t* ret) {
 (void)id;
 (void)ret;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelAioInitializeImpl(void* param, int32_t size) {
 (void)param;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI sceKernelAioInitializeParam(void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelAioSubmitReadCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
 (void)req;
 (void)size;
 (void)prio;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelAioSubmitWriteCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
 (void)req;
 (void)size;
 (void)prio;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelAioWaitRequest(int32_t id, int32_t* state, uint32_t* usec) {
 (void)id;
 (void)state;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
