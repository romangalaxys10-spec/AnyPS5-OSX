#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"

namespace AgcDriver::Graphics {

void GpuColorTransfer::WriteBack(std::uint64_t address) {
    writeBack(address, false);
}

void GpuColorTransfer::WriteBackTracked(std::uint64_t address) {
    writeBack(address, true);
}

void GpuColorTransfer::writeBack(std::uint64_t address, bool tracked) {
    PerformanceTimer timing("ColorTransfer.WriteBack");
    Require(tiled != nullptr && readback != nullptr, "color transfer is not prepared for writeback");
    const ColorTargetLayout layout(width, height, mode);
    Require(address != 0 && address % layout.Alignment() == 0, "misaligned color writeback address");
    timing.Mark("validate");
    readback->Invalidate();
    if (tracked) GuestMemoryBacking::GuestMemoryBackingWrite_nid_postfix(address, readback->Bytes().data(), readback->Bytes().size());
    else GuestMemory::Write(address, readback->Bytes(), layout.Alignment());
    timing.Mark("guest_write", layout.Bytes());
}

}
