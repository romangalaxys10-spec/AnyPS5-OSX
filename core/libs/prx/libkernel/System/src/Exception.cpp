#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler) {
 (void)signum;
 (void)handler;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelRemoveExceptionHandler(int signum) {
 (void)signum;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelRaiseException(Pthread thread, int signum) {
 (void)thread;
 (void)signum;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI sceKernelDebugRaiseException(int c1, int c2) {
 (void)c1;
 (void)c2;
 NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI sceKernelDebugRaiseExceptionOnReleaseMode(int c1, int c2) {
 (void)c1;
 (void)c2;
 NotImplemented_nid_no_patch(__func__);
}

}
