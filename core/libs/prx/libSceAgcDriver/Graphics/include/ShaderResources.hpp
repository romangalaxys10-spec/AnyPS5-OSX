#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERRESOURCES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERRESOURCES_HPP

#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DescriptorCache.hpp"
#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <array>
#include <memory>
#include <vector>

namespace AgcDriver::Graphics {

class ShaderResources {
public:
    ShaderResources(const Context& context, const ShaderRecompiler::RecompileResult& vertex, const ShaderRecompiler::RecompileResult& fragment, const ColorTarget& target, std::uint64_t indexAddress, std::size_t indexBytes);
    ShaderResources(const Context& context, std::span<const CompiledShader> shaders, const ColorTarget& target, std::uint64_t indexAddress, std::size_t indexBytes, std::span<const GuestMemorySnapshot> snapshots = {});
    ShaderResources(const Context& context, const CompiledShader& compute, std::span<const GuestMemorySnapshot> snapshots = {});
    ~ShaderResources();
    ShaderResources(const ShaderResources&) = delete;
    ShaderResources& operator=(const ShaderResources&) = delete;
    VkDescriptorSetLayout Layout() const;
    void Bind(VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout) const;
    void WriteBack();
    bool WritesOverlap(std::uint64_t address, std::size_t bytes) const { return guestMemory.WritesOverlap(address, bytes); }
    const std::vector<std::uint32_t>& LayoutKey() const { return layoutKey; }

private:
    struct Allocation {
        std::uint64_t address;
        std::size_t size;
        bool guest;
        std::unique_ptr<Buffer> buffer;
        ShaderRecompiler::DescriptorRole role = ShaderRecompiler::DescriptorRole::ShaderData;
    };

    struct Binding {
        VkDescriptorSetLayoutBinding layout;
        std::vector<std::size_t> allocations;
        std::vector<std::size_t> imageAllocations;
    };

    void build(std::span<const CompiledShader> shaders, const ColorTarget* target, std::uint64_t indexAddress, std::size_t indexBytes);
    std::size_t addGuestBuffer(std::span<const std::uint32_t> words, const ColorTarget* target, std::uint64_t indexAddress, std::size_t indexBytes);
    std::size_t addDataBuffer(std::span<const std::uint32_t> words);
    void addImageBinding(const ShaderRecompiler::DescriptorBinding& binding, VkShaderStageFlags flags, std::vector<Binding>& bindings);
    void release() noexcept;
    void prepareAddressBindings(std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots);
    VkDescriptorBufferInfo descriptor(const Allocation& allocation) const;
    Context context;
    std::vector<std::uint32_t> layoutKey;
    GuestBufferMemory guestMemory;
    std::unique_ptr<BdaResources> bda;
    bool usesBda = false;
    bool usesFaultBuffer = false;
    VkDescriptorSetLayout _layout = VK_NULL_HANDLE;
    VkDescriptorSet _set = VK_NULL_HANDLE;
    std::unique_ptr<DescriptorAllocation> descriptors;
    std::vector<Allocation> allocations;
    std::vector<std::shared_ptr<Texture>> textures;
    std::vector<std::shared_ptr<Sampler>> samplers;
};

}

#endif
