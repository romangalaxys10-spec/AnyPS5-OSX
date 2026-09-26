#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cstring>
extern "C" int APS5_VABI sceSystemServiceLoadExec(const char*, const char* const*);
extern "C" void APS5_VABI _Exit_nid_postfix(int);
namespace {
bool cleaned = false;
void Cleanup() { cleaned = true; }
void VerifyExit() {
    if (!cleaned) std::_Exit(1);
    std::puts("Guest shutdown and atexit completed");
}
void Require(bool value) { if (!value) std::abort(); }
void UnexpectedCleanup() { std::_Exit(3); }
}
int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--immediate") == 0) {
        LibcRegisterShutdown_nid_postfix(UnexpectedCleanup);
        Require(std::atexit(UnexpectedCleanup) == 0);
        _Exit_nid_postfix(0);
        return 2;
    }
    if (argc > 1) {
        LibcRegisterShutdown_nid_postfix(Cleanup);
        Require(std::atexit(VerifyExit) == 0);
        sceSystemServiceLoadExec("exit", nullptr);
        return 2;
    }
    Require(sceSystemServiceLoadExec(nullptr, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceSystemServiceLoadExec("", nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    bool rejected = false;
    try { sceSystemServiceLoadExec("/app0/another.bin", nullptr); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
}
