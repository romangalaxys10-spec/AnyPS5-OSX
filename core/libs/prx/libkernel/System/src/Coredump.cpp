#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceCoredumpRegisterCoredumpHandler(uint64_t handler, size_t stack_size, uint64_t context) {
 (void)handler;
 (void)stack_size;
 (void)context;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceCoredumpUnregisterCoredumpHandler(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
