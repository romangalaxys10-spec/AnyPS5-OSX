#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTUREDETILER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTUREDETILER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

    class TextureDetiler {
    public:
        explicit TextureDetiler(const Context& context);
        ~TextureDetiler();
        TextureDetiler(const TextureDetiler&) = delete;
        TextureDetiler& operator=(const TextureDetiler&) = delete;

        void Dispatch(VkCommandBuffer commands, TextureTileMode tileMode, std::uint32_t elementBytes, VkBuffer source, std::uint64_t sourceOffset, VkBuffer destination, std::uint64_t destinationOffset, const TileMipLayout& layout, std::uint32_t arrayLayer);
        void BeginBatch();

    private:
        VkPipeline pipeline(TextureTileMode tileMode, std::uint32_t elementBytes);
        void release() noexcept;
        VkDescriptorSet allocateSet();

        const Context context;
        VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkShaderModule module = VK_NULL_HANDLE;
        std::vector<std::pair<std::uint32_t, VkPipeline>> pipelines;
        std::vector<VkDescriptorPool> descriptorPools;
        std::size_t allocatedSets = 0;
    };

}

#endif
