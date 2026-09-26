#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include <cstring>
#include <string>
#include <string_view>

namespace {

using namespace AgcDriver::Graphics;

struct Push {
    std::uint32_t srcBase;
    std::uint32_t dstBase;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pitchBytes;
    std::uint32_t blocksPerRow;
    std::uint32_t tail;
    std::uint32_t tailX;
    std::uint32_t tailY;
    std::uint32_t elementBytes;
    std::uint32_t arrayLayer;
};

Push decodePush(const std::vector<std::byte>& bytes) {
    Push push{};
    Require(bytes.size() == sizeof(Push), "unexpected texture detiling push constant size");
    std::memcpy(&push, bytes.data(), sizeof(Push));
    return push;
}

TileMipLayout makeLayout(std::uint32_t width, std::uint32_t height, std::uint32_t pitchBytes, std::uint32_t blocksPerRow, std::uint64_t tiledSize, std::uint64_t linearSize) {
    TileMipLayout layout{};
    layout.width = width;
    layout.height = height;
    layout.pitchBytes = pitchBytes;
    layout.blocksPerRow = blocksPerRow;
    layout.tiledSize = tiledSize;
    layout.linearSize = linearSize;
    layout.tail = false;
    layout.tailX = 0;
    layout.tailY = 0;
    return layout;
}

template<typename TAction>
void reject(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected texture detiler test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected texture detiler rejection: ") + std::string(reason));
}

}

void RunTextureDetilerTests(const Context& context, const TextureDetilerTestAccess& access) {
    TextureDetiler detiler(context);
    const auto commands = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1));
    const auto source = access.makeBuffer(4096);
    const auto destination = access.makeBuffer(4096);

    const auto layout = makeLayout(20, 12, 96, 5, 64, 48);
    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 20, destination, 40, layout, 0);
    auto capture = access.lastDispatch();
    Require(capture.groupsX == 3 && capture.groupsY == 2 && capture.groupsZ == 1, "dispatch group counts were computed incorrectly");
    const auto push = decodePush(capture.pushConstants);
    Require(push.srcBase == 4 && push.dstBase == 8, "push constant buffer bases must account for storage buffer offset alignment");
    Require(push.width == 20 && push.height == 12, "push constant dimensions changed");
    Require(push.pitchBytes == 96 && push.blocksPerRow == 5, "push constant row layout changed");
    Require(push.tail == 0 && push.tailX == 0 && push.tailY == 0, "push constant tail fields must reflect a non-tail mip");
    Require(push.elementBytes == 4, "push constant element size changed");
    Require(capture.sourceBuffer == source && capture.destinationBuffer == destination, "dispatch bound the wrong source or destination buffer");
    Require(capture.sourceOffset == 16 && capture.sourceRange == 68, "source descriptor offset or range computed incorrectly");
    Require(capture.destinationOffset == 32 && capture.destinationRange == 56, "destination descriptor offset or range computed incorrectly");

    const auto pipelinesAfterFirst = access.pipelineCount();
    Require(pipelinesAfterFirst == 1, "the first dispatch must create exactly one compute pipeline");

    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterFirst, "dispatching with the same tile mode and element size must reuse the cached pipeline");

    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 8, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterFirst + 1, "a different element size must create a new compute pipeline");

    detiler.Dispatch(commands, TextureTileMode::kLinear, 4, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterFirst + 2, "a different tile mode must create a new compute pipeline");

    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterFirst + 2, "reusing an earlier tile mode and element size must not create another pipeline");

    detiler.Dispatch(commands, TextureTileMode::RenderTarget64KB, 4, source, 0, destination, 0, layout, 13);
    capture = access.lastDispatch();
    Require(decodePush(capture.pushConstants).arrayLayer == 13, "render target detiling must preserve the absolute array layer for XOR addressing");
    Require(access.pipelineCount() == pipelinesAfterFirst + 3, "render target detiling must use a separate pipeline");
    const auto specialization = access.lastSpecialization();
    Require(specialization[0] == 4 && specialization[1] == 65536 && specialization[2] == 2, "render target detiling must select its own swizzle family");

    reject([&] { detiler.Dispatch(VK_NULL_HANDLE, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, layout, 0); }, "active command buffer");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, VK_NULL_HANDLE, 0, destination, 0, layout, 0); }, "source and destination buffers");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, VK_NULL_HANDLE, 0, layout, 0); }, "source and destination buffers");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(0, 12, 96, 5, 64, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 0, 96, 5, 64, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 12, 96, 5, 0, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 12, 96, 5, 64, 0), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 3, source, 0, destination, 0, layout, 0); }, "unsupported element size");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 20, destination, 40, makeLayout(20, 12, 96, 5, 1024, 48), 0); }, "buffer range exceeds device limits");
}
