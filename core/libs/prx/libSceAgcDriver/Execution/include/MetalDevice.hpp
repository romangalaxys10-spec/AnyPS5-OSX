#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_METALDEVICE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_METALDEVICE_HPP

#include "prx/libSceAgcDriver/Execution/include/GpuDevice.hpp"
#include <memory>

#if defined(__APPLE__)

namespace AgcDriver {

class MetalDevice : public GpuDevice {
public:
    explicit MetalDevice(const PresentationWindow* window = nullptr);
    ~MetalDevice() override;
    MetalDevice(const MetalDevice&) = delete;
    MetalDevice& operator=(const MetalDevice&) = delete;
    ShaderRecompiler::SpirvTarget Target() const override;
    void WaitIdle() override;
    void WaitDraws() override;
    void AcquireGpuMemory() override;
    void ResolveMemory(std::uint64_t address, std::size_t bytes, bool writable) override;
    void* Window() const override;
    void Resize(std::uint32_t width, std::uint32_t height) override;
    bool Presentable() const override;
    void PresentClear(std::uint32_t width, std::uint32_t height, bool opaque) override;
    void PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) override;
    void PresentDisplayBuffer(const DisplayBuffer& buffer) override;
    void Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}) override;
    void Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}) override;
    void EnqueueDraw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}) override;

    struct State;
private:
    std::unique_ptr<State> state;
    void present(std::uint32_t width, std::uint32_t height, bool opaque, std::span<const std::byte> pixels, const DisplayBuffer* display);
};

bool MetalDeviceAvailable();

}

#else

#include <stdexcept>

namespace AgcDriver {

class MetalDevice : public GpuDevice {
public:
    explicit MetalDevice(const PresentationWindow* window = nullptr) {
        static_cast<void>(window);
        throw std::runtime_error("MetalDevice is only available on Apple platforms");
    }
    ~MetalDevice() override = default;
    MetalDevice(const MetalDevice&) = delete;
    MetalDevice& operator=(const MetalDevice&) = delete;
    ShaderRecompiler::SpirvTarget Target() const override { throw std::runtime_error("MetalDevice is only available on Apple platforms"); }
    void WaitIdle() override { throw std::runtime_error("MetalDevice is only available on Apple platforms"); }
    void WaitDraws() override { throw std::runtime_error("MetalDevice is only available on Apple platforms"); }
    void AcquireGpuMemory() override { throw std::runtime_error("MetalDevice is only available on Apple platforms"); }
    void ResolveMemory(std::uint64_t address, std::size_t bytes, bool writable) override {
        static_cast<void>(address);
        static_cast<void>(bytes);
        static_cast<void>(writable);
        throw std::runtime_error("MetalDevice is only available on Apple platforms");
    }
    void* Window() const override { throw std::runtime_error("MetalDevice is only available on Apple platforms"); }
    void Resize(std::uint32_t width, std::uint32_t height) override {
        static_cast<void>(width);
        static_cast<void>(height);
        throw std::runtime_error("MetalDevice is only available on Apple platforms");
    }
    bool Presentable() const override { return false; }
    void PresentClear(std::uint32_t width, std::uint32_t height, bool opaque) override {
        static_cast<void>(width);
        static_cast<void>(height);
        static_cast<void>(opaque);
        throw std::runtime_error("MetalDevice is only available on Apple platforms");
    }
    void PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) override {
        static_cast<void>(width);
        static_cast<void>(height);
        static_cast<void>(pixels);
        throw std::runtime_error("MetalDevice is only available on Apple platforms");
    }
    void PresentDisplayBuffer(const DisplayBuffer& buffer) override {
        static_cast<void>(buffer);
        throw std::runtime_error("MetalDevice is only available on Apple platforms");
    }
    void Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}) override {
        static_cast<void>(shader);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(snapshots);
        throw std::runtime_error("MetalDevice is only available on Apple platforms");
    }
    void Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}) override {
        static_cast<void>(graphics);
        static_cast<void>(draw);
        static_cast<void>(shaders);
        static_cast<void>(snapshots);
        throw std::runtime_error("MetalDevice is only available on Apple platforms");
    }
    void EnqueueDraw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}) override {
        static_cast<void>(graphics);
        static_cast<void>(draw);
        static_cast<void>(shaders);
        static_cast<void>(snapshots);
        throw std::runtime_error("MetalDevice is only available on Apple platforms");
    }
};

}

#endif

#endif
