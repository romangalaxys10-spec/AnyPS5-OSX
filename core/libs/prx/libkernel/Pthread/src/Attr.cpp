#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;

static constexpr std::size_t DEFAULT_STACK_SIZE = 1u << 20;
static constexpr int DETACH_JOINABLE = 0;
static constexpr int DETACH_DETACHED = 1;
static constexpr int SCHED_FIFO_PS5 = 1;

#ifdef _WIN32
#include <windows.h>
#include <limits>
#endif

extern "C" {

int APS5_VABI scePthreadAttrInit(PthreadAttr* attr) {
    if (!attr) throw std::runtime_error("scePthreadAttrInit: null attr");
    auto* p = new (std::nothrow) PthreadAttrPrivate{};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    p->_stacksize = DEFAULT_STACK_SIZE;
    p->_detachstate = DETACH_JOINABLE;
    p->_schedpriority = 700;
    p->_schedpolicy = SCHED_FIFO_PS5;
    p->_inheritsched = 4;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadAttrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetdetachstate(PthreadAttr* attr, int detachstate) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadAttrSetdetachstate: null attr");
    if (detachstate != DETACH_JOINABLE && detachstate != DETACH_DETACHED)
        throw std::runtime_error("scePthreadAttrSetdetachstate: invalid state");
    (*attr)->_detachstate = detachstate;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param) {
    if (!attr || !*attr || !param)
        throw std::runtime_error("scePthreadAttrSetschedparam: null arg");
    (*attr)->_schedpriority = param->sched_priority;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stacksize) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadAttrSetstacksize: null attr");
    if (stacksize < 16384) throw std::runtime_error("scePthreadAttrSetstacksize: too small");
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    if (stacksize % system.dwPageSize != 0 || stacksize > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("scePthreadAttrSetstacksize: invalid Windows stack size");
#endif
    (*attr)->_stacksize = stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stackaddr, std::size_t* stacksize) {
    if (!attr || !*attr || !stackaddr || !stacksize)
        throw std::runtime_error("scePthreadAttrGetstack: null arg");
    *stackaddr = (*attr)->stackAddress;
    *stacksize = (*attr)->_stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr) {
    if (!thread || !attr || !*attr)
        throw std::runtime_error("scePthreadAttrGet: null arg");
    (*attr)->_stacksize = thread->stackSize;
    (*attr)->stackAddress = thread->stackAddress;
    (*attr)->_detachstate = thread->_detached ? DETACH_DETACHED : DETACH_JOINABLE;
    (*attr)->_schedpriority = 700;
    (*attr)->_schedpolicy = SCHED_FIFO_PS5;
    (*attr)->_inheritsched = 4;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask) {
 (void)attr;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state) {
 (void)attr;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, size_t* guard_size) {
 (void)attr;
 (void)guard_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param) {
 (void)attr;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetsolosched(const PthreadAttr* attr, int* solosched) {
 (void)attr;
 (void)solosched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetstackaddr(const PthreadAttr* attr, void** stack_addr) {
 (void)attr;
 (void)stack_addr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, size_t* stack_size) {
 (void)attr;
 (void)stack_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask) {
 (void)attr;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, size_t guard_size) {
 (void)attr;
 (void)guard_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inherit_sched) {
 (void)attr;
 (void)inherit_sched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy) {
 (void)attr;
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetsolosched(PthreadAttr* attr, int solosched) {
 (void)attr;
 (void)solosched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* addr, size_t size) {
 (void)attr;
 (void)addr;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetstackaddr(PthreadAttr* attr, void* addr) {
 (void)attr;
 (void)addr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
