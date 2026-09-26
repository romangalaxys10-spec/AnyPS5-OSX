#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex) {
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr) {
 (void)mutex;
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex) {
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_mutex_timedlock_nid_postfix(PthreadMutex* mutex, const KernelTimespec* abstime) {
 (void)mutex;
 (void)abstime;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_mutex_trylock_nid_postfix(PthreadMutex* mutex) {
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex) {
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_mutexattr_setprotocol_nid_postfix(PthreadMutexattr* attr, int protocol) {
 (void)attr;
 (void)protocol;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_mutexattr_settype_nid_postfix(PthreadMutexattr* attr, int type) {
 (void)attr;
 (void)type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
