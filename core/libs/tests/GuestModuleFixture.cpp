#include "prx/libc/include/general/VabiMacros.hpp"
#ifdef _WIN32
#define MODULE_EXPORT __declspec(dllexport)
#else
#define MODULE_EXPORT __attribute__((visibility("default")))
#endif
extern "C" MODULE_EXPORT int APS5_VABI GuestModuleAdd_nid_postfix(int a, int b) {
    return a + b;
}
