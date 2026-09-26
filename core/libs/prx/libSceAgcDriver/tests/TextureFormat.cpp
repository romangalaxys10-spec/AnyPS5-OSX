#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <string>
#include <string_view>

namespace {

using namespace AgcDriver::Graphics;

template<typename TAction>
void reject(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected format test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected texture format rejection: ") + std::string(reason));
}

}

void RunTextureFormatTests() {
    Require(ResolveTextureFormat(1) == VK_FORMAT_R8_UNORM, "format 1 must resolve to R8_UNORM");
    Require(BytesPerElement(1) == 1u, "format 1 must be one byte wide");
    Require(!IsBlockCompressed(1), "format 1 must not be block compressed");
    Require(BlockWidth(1) == 1u && BlockHeight(1) == 1u, "format 1 must have a one-texel block");

    Require(ResolveTextureFormat(56) == VK_FORMAT_R8G8B8A8_UNORM, "format 56 must resolve to R8G8B8A8_UNORM");
    Require(BytesPerElement(56) == 4u, "format 56 must be four bytes wide");

    Require(ResolveTextureFormat(22) == VK_FORMAT_R32_SFLOAT, "format 22 must resolve to R32_SFLOAT");
    Require(BytesPerElement(22) == 4u, "format 22 must be four bytes wide");

    Require(ResolveTextureFormat(77) == VK_FORMAT_R32G32B32A32_SFLOAT, "format 77 must resolve to R32G32B32A32_SFLOAT");
    Require(BytesPerElement(77) == 16u, "format 77 must be sixteen bytes wide");

    Require(ResolveTextureFormat(169) == VK_FORMAT_BC1_RGBA_UNORM_BLOCK, "format 169 must resolve to BC1_RGBA_UNORM_BLOCK");
    Require(IsBlockCompressed(169), "format 169 must be block compressed");
    Require(BytesPerElement(169) == 8u, "BC1 blocks must be eight bytes");
    Require(BlockWidth(169) == 4u && BlockHeight(169) == 4u, "BC1 blocks must be four by four texels");

    Require(ResolveTextureFormat(181) == VK_FORMAT_BC7_UNORM_BLOCK, "format 181 must resolve to BC7_UNORM_BLOCK");
    Require(BytesPerElement(181) == 16u, "BC7 blocks must be sixteen bytes");

    Require(ResolveTextureFormat(34) == ResolveTextureFormat(20), "format 34 must remap to format 20");
    Require(BytesPerElement(34) == BytesPerElement(20), "remapped format 34 must share the width of format 20");

    reject([] { ResolveTextureFormat(0); }, "unsupported guest texture format");
    reject([] { ResolveTextureFormat(183); }, "unsupported guest texture format");
    reject([] { ResolveTextureFormat(9999); }, "unsupported guest texture format");
    reject([] { BytesPerElement(2); }, "unsupported guest texture format");
    reject([] { IsBlockCompressed(200); }, "unsupported guest texture format");
}
