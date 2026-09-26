#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURETILING_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURETILING_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include <cstdint>
#include <vector>

namespace AgcDriver::Graphics {

struct TileMipLayout {
    std::uint64_t tiledOffset;
    std::uint64_t tiledSize;
    std::uint64_t linearOffset;
    std::uint64_t linearSize;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t blocksPerRow;
    std::uint32_t pitchBytes;
    bool tail;
    std::uint32_t tailX;
    std::uint32_t tailY;
};

std::vector<TileMipLayout> ComputeMipLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount);
std::uint64_t ComputeSurfaceSize(const std::vector<TileMipLayout>& mips, std::uint32_t arrayLayers);

}

#endif
