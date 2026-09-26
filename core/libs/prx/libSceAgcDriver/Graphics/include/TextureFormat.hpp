#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTUREFORMAT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTUREFORMAT_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <cstdint>

namespace AgcDriver::Graphics {

VkFormat ResolveTextureFormat(std::uint32_t guestFormat);
std::uint32_t BytesPerElement(std::uint32_t guestFormat);
bool IsBlockCompressed(std::uint32_t guestFormat);
std::uint32_t BlockWidth(std::uint32_t guestFormat);
std::uint32_t BlockHeight(std::uint32_t guestFormat);

}

#endif
