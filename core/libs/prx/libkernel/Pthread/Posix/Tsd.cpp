#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void* APS5_VABI pthread_getspecific_nid_postfix(PthreadKey key) {
 (void)key;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI pthread_setspecific_nid_postfix(PthreadKey key, void* value) {
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_key_create_nid_postfix(PthreadKey* key, pthread_key_destructor_func_t destructor) {
 (void)key;
 (void)destructor;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_key_delete_nid_postfix(PthreadKey key) {
 (void)key;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
