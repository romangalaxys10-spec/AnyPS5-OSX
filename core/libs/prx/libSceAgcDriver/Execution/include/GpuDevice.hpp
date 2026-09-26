#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GPUDEVICE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GPUDEVICE_HPP

#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace AgcDriver {

// Backend-neutral execution interface. On Linux and Windows the only implementation is
// VulkanDevice. On macOS there are two: VulkanDevice running on top of MoltenVK and
// MetalDevice running natively on Metal with MetalFX presentation scaling.
class GpuDevice {
public:
    virtual ~GpuDevice() = default;

    virtual ShaderRecompiler::SpirvTarget Target() const = 0;
    virtual void WaitIdle() = 0;
    virtual void WaitDraws() = 0;
    virtual void AcquireGpuMemory() = 0;
    virtual void ResolveMemory(std::uint64_t address, std::size_t bytes, bool writable) = 0;
    virtual void* Window() const = 0;
    virtual void Resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual bool Presentable() const = 0;
    virtual void PresentClear(std::uint32_t width, std::uint32_t height, bool opaque) = 0;
    virtual void PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) = 0;
    virtual void PresentDisplayBuffer(const DisplayBuffer& buffer) = 0;
    virtual void Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}) = 0;
    virtual void Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}) = 0;
    virtual void EnqueueDraw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}) = 0;
};

// Creates the execution device for the current host and backend configuration.
// Selection order (ANYPS5_GPU_BACKEND env overrides the compile-time default):
//   macOS  : METAL when MetalFX is available (default), VULKAN (MoltenVK) fallback.
//   others : VULKAN always.
std::shared_ptr<GpuDevice> MakeGpuDevice(const PresentationWindow* window);

}

#endif
