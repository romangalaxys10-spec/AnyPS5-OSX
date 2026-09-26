#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GraphicsPipelineCache.hpp"
#include <cstring>
#include <limits>
#include <memory>

namespace AgcDriver::Graphics {
namespace {

struct DrawStorage {
    std::unique_ptr<Buffer> indices;
    std::vector<std::unique_ptr<Buffer>> vertices;
    std::shared_ptr<ResidentColor> color;
    std::shared_ptr<Pipeline> pipeline;
};

}

void Draw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots) {
    PerformanceTimer timing("Graphics.Draw");
    Require(draw.indexed ? draw.flags == 0 : (draw.flags & ~0x20u) == 0, "draw modifiers are unsupported");
    if (draw.indexed) {
        Require(draw.indexSize == 2 || draw.indexSize == 4, "only uint16 and uint32 index buffers are supported");
        Require(draw.firstVertex == 0 && draw.firstInstance == 0, "indexed draw offsets are unsupported");
    } else {
        Require(draw.indexAddress == 0 && draw.indexSize == 0, "auto draw must not reference an index buffer");
        if (draw.indexCount == 0 || draw.instanceCount == 0) return;
        Require(draw.firstVertex <= std::numeric_limits<std::uint32_t>::max() - (draw.indexCount - 1u), "auto draw vertex range overflow");
        Require(draw.firstInstance <= std::numeric_limits<std::uint32_t>::max() - (draw.instanceCount - 1u), "auto draw instance range overflow");
    }
    Require(draw.indexCount != 0 && draw.instanceCount != 0, "zero-count indexed draws are unsupported");
    const auto indexBytes = static_cast<std::uint64_t>(draw.indexCount) * draw.indexSize;
    Require(indexBytes <= std::numeric_limits<std::size_t>::max(), "index buffer size overflow");
    if (draw.indexed) GuestMemory::CheckRange(reinterpret_cast<const void*>(draw.indexAddress), static_cast<std::size_t>(indexBytes), draw.indexSize);
    Require(!draw.indexed || !state.hasColorTarget || draw.indexAddress + indexBytes <= state.color.address || state.color.address + state.color.bytes <= draw.indexAddress, "index buffer aliases the render target");
    if (state.rectList) Require(draw.indexCount % 3 == 0, "incomplete rect-list primitive");
    ValidateShaders(shaders, state, context.subgroup, context.fragmentShaderBarycentric);
    const auto shaderStages = PipelineStages(shaders);
    std::uint32_t meshGroups = 0;
    if (state.stages.mesh) {
        Require(draw.firstVertex == 0 && draw.firstInstance == 0, "mesh draw offsets are unsupported");
        Require(context.meshShader, "device does not support mesh shaders");
        const auto& mesh = *state.stages.mesh;
        const auto inputSize = mesh.inputPrimitive == 1 ? 1u : mesh.inputPrimitive == 2 ? 2u : 3u;
        Require(draw.indexCount >= inputSize && mesh.primitivesPerGroup != 0, "mesh draw contains no complete primitive");
        const auto step = mesh.inputPrimitive == 6 ? 1u : inputSize;
        const auto primitives = (draw.indexCount - inputSize) / step + 1u;
        meshGroups = (primitives - 1u) / mesh.primitivesPerGroup + 1u;
        Require(meshGroups <= context.meshLimits.maxMeshWorkGroupCount[0] && draw.instanceCount <= context.meshLimits.maxMeshWorkGroupCount[1] && static_cast<std::uint64_t>(meshGroups) * draw.instanceCount <= context.meshLimits.maxMeshWorkGroupTotalCount, "mesh draw exceeds workgroup count limits");
    }
    if (state.stages.tessellation) Require(draw.indexCount % state.stages.tessellation->inputControlPoints == 0, "incomplete tessellation patch");
    timing.Mark("validate");
    Require(context.renderCache != nullptr && context.drawQueue != nullptr && context.graphicsPipelines != nullptr, "device graphics execution caches are unavailable");
    auto storage = std::make_shared<DrawStorage>();
    auto& indices = storage->indices;
    std::uint32_t maxIndex = draw.indexed ? 0u : draw.firstVertex + draw.indexCount - 1u;
    if (draw.indexed) {
        indices = std::make_unique<Buffer>(context, static_cast<std::size_t>(indexBytes), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        GuestMemory::Read(draw.indexAddress, indices->Bytes(), draw.indexSize);
        for (std::size_t offset = 0; offset < indexBytes; offset += draw.indexSize) {
            std::uint32_t index = 0;
            if (draw.indexSize == 2) {
                std::uint16_t value = 0;
                std::memcpy(&value, indices->Bytes().data() + offset, sizeof(value));
                index = value;
            } else {
                std::memcpy(&index, indices->Bytes().data() + offset, sizeof(index));
            }
            Require(index <= context.limits.maxDrawIndexedIndexValue, "index exceeds the device's indexed draw limit");
            maxIndex = std::max(maxIndex, index);
        }
    }
    timing.Mark("index_upload");
    const auto& attributes = shaders.front().program->vertexAttributes;
    static_cast<void>(BuildVertexInputLayout(context, attributes));
    auto& vertexBuffers = storage->vertices;
    std::vector<VkBuffer> vertexHandles;
    std::vector<VkDeviceSize> vertexOffsets(attributes.size(), 0);
    for (const auto& attribute : attributes) {
        const auto bytes = VertexBufferReadSize(attribute, maxIndex, draw.instanceCount, draw.firstInstance);
        const auto& fields = attribute.resource.fields;
        const auto address = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
        Require(!state.hasColorTarget || address + bytes <= state.color.address || state.color.address + state.color.bytes <= address, "vertex buffer aliases the render target");
        GuestMemory::CheckRange(reinterpret_cast<const void*>(address), bytes, 1);
        auto buffer = std::make_unique<Buffer>(context, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        GuestMemory::Read(address, buffer->Bytes(), 1);
        vertexHandles.push_back(buffer->Handle());
        vertexBuffers.push_back(std::move(buffer));
    }
    timing.Mark("vertex_upload");
    auto resources = std::make_shared<ShaderResources>(context, shaders, state.color, draw.indexAddress, static_cast<std::size_t>(indexBytes), snapshots);
    timing.Mark("shader_resources");
    if (state.hasColorTarget) {
        const ColorTargetLayout colorLayout(state.color.extent.width, state.color.extent.height, state.color.tileMode);
        Require(state.color.bytes == colorLayout.Bytes(), "color target transfer size mismatch");
        storage->color = context.renderCache->Get(state.color, state.blend.blendEnable != 0);
    }
    timing.Mark("render_target_cache");
    storage->pipeline = context.graphicsPipelines->Get(state, storage->color, *resources, shaders);
    auto& pipeline = *storage->pipeline;
    timing.Mark("pipeline_cache");
    const auto commands = context.drawQueue->Begin(context);
    VkMemoryBarrier upload{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    upload.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    upload.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | shaderStages, 0, 1, &upload, 0, nullptr, 0, nullptr);
    if (storage->color) storage->color->Begin(commands);
    pipeline.Begin(commands, state.renderExtent);
    resources->Bind(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Layout());
    pipeline.PushConstants(commands, shaders);
    if (state.stages.mesh) {
        context.Function<PFN_vkCmdDrawMeshTasksEXT>("vkCmdDrawMeshTasksEXT")(commands, meshGroups, draw.instanceCount, 1);
    } else {
        if (!vertexHandles.empty()) context.Function<PFN_vkCmdBindVertexBuffers>("vkCmdBindVertexBuffers")(commands, 0, static_cast<std::uint32_t>(vertexHandles.size()), vertexHandles.data(), vertexOffsets.data());
        if (draw.indexed) {
            context.Function<PFN_vkCmdBindIndexBuffer>("vkCmdBindIndexBuffer")(commands, indices->Handle(), 0, draw.indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
            context.Function<PFN_vkCmdDrawIndexed>("vkCmdDrawIndexed")(commands, draw.indexCount, draw.instanceCount, 0, 0, 0);
        } else {
            context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, draw.indexCount, draw.instanceCount, draw.firstVertex, draw.firstInstance);
        }
    }
    context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
    VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    download.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    download.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT | shaderStages, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
    timing.Mark("command_record");
    context.drawQueue->Enqueue(std::move(resources), std::move(storage));
    timing.Mark("enqueue");
}

}
