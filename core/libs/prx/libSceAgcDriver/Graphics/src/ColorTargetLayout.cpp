#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>

namespace AgcDriver::Graphics {
namespace {

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint32_t blockOffset(std::uint32_t x, std::uint32_t y) {
    return ((y << 4u) & 0x0070u) ^ ((y << 5u) & 0x0f00u) ^ ((y << 9u) & 0x1000u) ^ ((y << 8u) & 0x4000u)
        ^ ((x << 2u) & 0x000cu) ^ ((x << 5u) & 0x0380u) ^ ((x << 4u) & 0x0400u) ^ ((x << 6u) & 0x0800u) ^ ((x << 9u) & 0xa000u);
}

}

ColorTileMode DecodeColorTileMode(std::uint32_t attrib3) {
    require((attrib3 & 0x80002000u) == 0 && (attrib3 & 0x1fffu) == 0 && ((attrib3 >> 24u) & 3u) == 1 && ((attrib3 >> 27u) & 7u) == 1, "AGC graphics: unsupported color depth, dimension, resource level or metadata mode");
    const auto mode = (attrib3 >> 14u) & 0x1fu;
    const auto fmaskMode = (attrib3 >> 19u) & 0x1fu;
    require(fmaskMode == 0 || fmaskMode == 0x18, "AGC graphics: unsupported color FMASK swizzle mode");
    require(mode == 0 || mode == 0x1b, "AGC graphics: unsupported color tile mode");
    return static_cast<ColorTileMode>(mode);
}

ColorTargetLayout::ColorTargetLayout(std::uint32_t width, std::uint32_t height, ColorTileMode mode) : width(width), height(height), pitch(width), mode(mode), bytes(0) {
    require(width != 0 && height != 0 && width <= 16384 && height <= 16384, "AGC graphics: invalid color surface extent");
    std::uint32_t paddedHeight = height;
    switch (mode) {
        case ColorTileMode::Linear:
            require(width % 64u == 0, "AGC graphics: linear surface pitch requires a width aligned to 256 bytes");
            break;
        case ColorTileMode::RenderTarget:
            pitch = (width + 127u) & ~127u;
            paddedHeight = (height + 127u) & ~127u;
            break;
        default: throw std::runtime_error("AGC graphics: unsupported color tile mode");
    }
    const auto size = static_cast<std::uint64_t>(pitch) * paddedHeight * 4u;
    require(size <= std::numeric_limits<std::size_t>::max(), "AGC graphics: color surface size overflow");
    bytes = static_cast<std::size_t>(size);
}

std::size_t ColorTargetLayout::offset(std::uint32_t x, std::uint32_t y) const {
    if (mode == ColorTileMode::Linear) return (static_cast<std::size_t>(y) * pitch + x) * 4u;
    const auto block = static_cast<std::size_t>(y / 128u) * (pitch / 128u) + x / 128u;
    return block * 65536u + blockOffset(x, y);
}

std::size_t ColorTargetLayout::Offset(std::uint32_t x, std::uint32_t y) const {
    require(x < width && y < height, "AGC graphics: color surface coordinate out of range");
    return offset(x, y);
}

void ColorTargetLayout::Detile(std::span<const std::byte> source, std::span<std::byte> destination) const {
    require(source.size() == Bytes() && destination.size() == LinearBytes(), "AGC graphics: color detile buffer size mismatch");
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::memcpy(destination.data() + (static_cast<std::size_t>(y) * width + x) * 4u, source.data() + offset(x, y), 4);
        }
    }
}

void ColorTargetLayout::Tile(std::span<const std::byte> source, std::span<std::byte> destination) const {
    require(source.size() == LinearBytes() && destination.size() == Bytes(), "AGC graphics: color tile buffer size mismatch");
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::memcpy(destination.data() + offset(x, y), source.data() + (static_cast<std::size_t>(y) * width + x) * 4u, 4);
        }
    }
}

}
