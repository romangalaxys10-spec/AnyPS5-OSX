#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

intptr_t APS5_VABI sceKernelGetEventData(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetEventError(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

intptr_t APS5_VABI sceKernelGetEventFflags(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetEventFilter(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* APS5_VABI sceKernelGetEventUserData(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

}
