#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VULKANDEVICE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VULKANDEVICE_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Execution/include/GpuDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include <memory>

namespace AgcDriver {

class VulkanDevice : public GpuDevice {
public:
    explicit VulkanDevice(const PresentationWindow* window = nullptr);
    ~VulkanDevice();
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    ShaderRecompiler::SpirvTarget Target() const;
    void WaitIdle();
    void WaitDraws();
    void AcquireGpuMemory();
    void ResolveMemory(std::uint64_t address, std::size_t bytes, bool writable);
    void* Window() const;
    void Resize(std::uint32_t width, std::uint32_t height);
    bool Presentable() const;
    void PresentClear(std::uint32_t width, std::uint32_t height, bool opaque);
    void PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels);
    void PresentDisplayBuffer(const DisplayBuffer& buffer);
    void Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});
    void Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});
    void EnqueueDraw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});

private:
    Graphics::Context graphicsContext() const;
    void present(std::uint32_t width, std::uint32_t height, bool opaque, std::span<const std::byte> pixels, const DisplayBuffer* display = nullptr);
    struct State;
    std::unique_ptr<State> state;
};

}

#endif
