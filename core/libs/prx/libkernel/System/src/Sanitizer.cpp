#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceKernelIsAddressSanitizerEnabled(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

MallocReplace* APS5_VABI sceKernelGetSanitizerMallocReplaceExternal(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

NewReplace* APS5_VABI sceKernelGetSanitizerNewReplaceExternal(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

}
