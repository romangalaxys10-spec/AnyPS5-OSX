#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI pthread_cond_broadcast_nid_postfix(PthreadCond* cond) {
 (void)cond;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) {
 (void)cond;
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_cond_signal_nid_postfix(PthreadCond* cond) {
 (void)cond;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
 (void)cond;
 (void)mutex;
 (void)abstime;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
 (void)cond;
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_condattr_destroy_nid_postfix(PthreadCondattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_condattr_init_nid_postfix(PthreadCondattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_condattr_setclock_nid_postfix(PthreadCondattr* attr, KernelClockid clock_id) {
 (void)attr;
 (void)clock_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
