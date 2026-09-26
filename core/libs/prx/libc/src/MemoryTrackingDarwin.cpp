#include "prx/libc/include/MemoryTrackingPlatform.hpp"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <exception>
#include <stdexcept>
#include <system_error>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

namespace GuestMemoryTracking::Platform {
namespace {

FaultHandler faultHandler = nullptr;
struct sigaction previousAction{};

void handleFault(int signal, siginfo_t* info, void* context) {
    const auto* native = static_cast<const ucontext_t*>(context);
    const auto error = native->uc_mcontext->__es.__err;
    if (info->si_code == SEGV_ACCERR && (error & 16) == 0) {
        try {
            if (faultHandler(reinterpret_cast<std::uintptr_t>(info->si_addr), (error & 2) != 0)) return;
        } catch (...) {
            std::terminate();
        }
    }
    if (previousAction.sa_handler == SIG_DFL || previousAction.sa_handler == SIG_IGN) {
        if (sigaction(signal, &previousAction, nullptr) != 0) std::terminate();
        if (raise(signal) != 0) std::terminate();
        return;
    }
    if ((previousAction.sa_flags & SA_SIGINFO) != 0) previousAction.sa_sigaction(signal, info, context);
    else previousAction.sa_handler(signal);
}

void protect(std::uint64_t address, std::size_t bytes, int protection) {
    if (mprotect(reinterpret_cast<void*>(address), bytes, protection) != 0) throw std::system_error(errno, std::generic_category(), "guest memory tracking mprotect failed");
}

}

std::size_t PageSize() {
    static const auto size = [] {
        const auto result = sysconf(_SC_PAGESIZE);
        if (result <= 0) throw std::runtime_error("invalid native page size");
        return static_cast<std::size_t>(result);
    }();
    return size;
}

void Install(FaultHandler handler) {
    if (handler == nullptr || faultHandler != nullptr) throw std::runtime_error("invalid guest memory fault handler installation");
    faultHandler = handler;
    struct sigaction action{};
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = handleFault;
    if (sigemptyset(&action.sa_mask) != 0 || sigaction(SIGSEGV, &action, &previousAction) != 0) {
        const auto error = errno;
        faultHandler = nullptr;
        throw std::system_error(error, std::generic_category(), "guest memory fault handler installation failed");
    }
}

std::vector<Region> Query(std::uint64_t address, std::size_t bytes) {
    std::vector<Region> regions;
    const auto end = address + bytes;
    mach_vm_address_t cursor = static_cast<mach_vm_address_t>(address);
    const mach_port_t task = mach_task_self();
    while (cursor < static_cast<mach_vm_address_t>(end)) {
        mach_vm_size_t regionSize = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t infoCount = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t objectName = MACH_PORT_NULL;
        const kern_return_t status = mach_vm_region(task, &cursor, &regionSize, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &infoCount, &objectName);
        if (status != KERN_SUCCESS) throw std::runtime_error("tracked guest memory range is not mapped");
        if (objectName != MACH_PORT_NULL) mach_port_deallocate(task, objectName);
        const std::uint64_t regionStart = static_cast<std::uint64_t>(cursor);
        const std::uint64_t regionEnd = regionStart + static_cast<std::uint64_t>(regionSize);
        if (regionEnd > address) {
            if (regionStart > address) throw std::runtime_error("tracked guest memory range is not mapped");
            const bool readable = (info.protection & VM_PROT_READ) != 0;
            const bool writable = (info.protection & VM_PROT_WRITE) != 0;
            const bool executable = (info.protection & VM_PROT_EXECUTE) != 0;
            if (!readable || !writable) throw std::runtime_error("tracked render memory must be mapped, writable and non-executable");
            const auto next = std::min(static_cast<mach_vm_address_t>(end), static_cast<mach_vm_address_t>(regionEnd));
            const int protection = PROT_READ | PROT_WRITE | (executable ? PROT_EXEC : 0);
            regions.push_back({address, static_cast<std::size_t>(next - address), static_cast<std::uint64_t>(protection)});
            address = static_cast<std::uint64_t>(next);
        }
        cursor = static_cast<mach_vm_address_t>(regionEnd);
    }
    if (address != end) throw std::runtime_error("tracked guest memory range is not mapped");
    return regions;
}

void Protect(std::uint64_t address, std::size_t bytes, Protection protection) {
    if (protection != Protection::None && protection != Protection::Read) throw std::invalid_argument("invalid tracked page protection");
    protect(address, bytes, protection == Protection::None ? PROT_NONE : PROT_READ);
}

void Restore(const std::vector<Region>& regions) {
    for (const auto& region : regions) protect(region.address, region.bytes, static_cast<int>(region.protection));
}

}
