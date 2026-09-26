#include "prx/libc/include/MemoryTrackingPlatform.hpp"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

namespace GuestMemoryTracking::Platform {
namespace {

FaultHandler faultHandler = nullptr;
struct sigaction previousAction{};

void handleFault(int signal, siginfo_t* info, void* context) {
    const auto* native = static_cast<const ucontext_t*>(context);
    const auto error = native->uc_mcontext.gregs[REG_ERR];
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
    std::ifstream maps("/proc/self/maps");
    if (!maps) throw std::runtime_error("cannot query tracked guest memory mappings");
    std::vector<Region> regions;
    const auto end = address + bytes;
    std::string line;
    while (address < end && std::getline(maps, line)) {
        std::istringstream input(line);
        std::uint64_t first = 0;
        std::uint64_t last = 0;
        char separator = 0;
        std::string permissions;
        if (!(input >> std::hex >> first >> separator >> last >> permissions) || separator != '-' || permissions.size() < 3 || first >= last) throw std::runtime_error("invalid tracked guest memory mapping");
        if (last <= address) continue;
        if (first > address || permissions[0] != 'r' || permissions[1] != 'w' || permissions[2] != '-') throw std::runtime_error("tracked render memory must be mapped, writable and non-executable");
        const auto next = std::min(end, last);
        regions.push_back({address, static_cast<std::size_t>(next - address), static_cast<std::uint64_t>(PROT_READ | PROT_WRITE | (permissions[2] == 'x' ? PROT_EXEC : 0))});
        address = next;
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
