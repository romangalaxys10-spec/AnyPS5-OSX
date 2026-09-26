#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sem_init_nid_postfix(void* sem, int pshared, unsigned int value) {
 (void)sem;
 (void)pshared;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sem_destroy_nid_postfix(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sem_wait_nid_postfix(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sem_trywait_nid_postfix(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sem_timedwait_nid_postfix(void* sem, const KernelTimespec* abstime) {
 (void)sem;
 (void)abstime;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sem_reltimedwait_np_nid_postfix(void* sem, uint32_t usec) {
 (void)sem;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sem_post_nid_postfix(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sem_getvalue_nid_postfix(void* sem, int* value) {
 (void)sem;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
