#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI pthread_rwlock_destroy_nid_postfix(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_rwlock_init_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) {
 (void)rwlock;
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_rwlock_wrlock_nid_postfix(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
