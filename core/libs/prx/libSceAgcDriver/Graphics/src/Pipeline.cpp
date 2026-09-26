#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace AgcDriver::Graphics {

Pipeline::Pipeline(const Context& context, const State& state, const RenderTarget* target, const ShaderResources& resources, std::span<const CompiledShader> shaders) : context(context), _modules(shaders.size()) {
    Require((target != nullptr) == state.hasColorTarget, "render target does not match decoded color state");
    Require(state.renderExtent.width != 0 && state.renderExtent.height != 0 && state.renderExtent.width <= context.limits.maxFramebufferWidth && state.renderExtent.height <= context.limits.maxFramebufferHeight, "framebuffer extent exceeds device limits");
    Require(state.hasColorTarget || (context.limits.framebufferNoAttachmentsSampleCounts & VK_SAMPLE_COUNT_1_BIT) != 0, "device does not support single-sample rendering without attachments");
    ValidateShaders(shaders, state, context.subgroup, context.fragmentShaderBarycentric);
    Require(!state.negativeOneToOne || context.depthClipControl, "negative-one-to-one depth clipping requires VK_EXT_depth_clip_control with depthClipControl enabled");
    if (state.rectList) Require(context.tessellationShader && context.limits.maxTessellationPatchSize >= 4, "rect-list requires tessellation with four output control points");
    if (state.stages.tessellation) {
        Require(context.tessellationShader, "device does not support tessellation shaders");
        Require(state.stages.tessellation->inputControlPoints <= context.limits.maxTessellationPatchSize && state.stages.tessellation->outputControlPoints <= context.limits.maxTessellationPatchSize, "tessellation patch exceeds device limits");
    }
    if (state.stages.mesh) {
        Require(context.meshShader, "device does not support VK_EXT_mesh_shader");
        const auto& mesh = *state.stages.mesh;
        Require(mesh.threadsPerGroup <= context.meshLimits.maxMeshWorkGroupInvocations && mesh.threadsPerGroup <= context.meshLimits.maxMeshWorkGroupSize[0], "mesh workgroup exceeds device limits");
        Require(mesh.maxVertices <= context.meshLimits.maxMeshOutputVertices && mesh.maxPrimitives <= context.meshLimits.maxMeshOutputPrimitives && static_cast<std::uint64_t>(mesh.ldsSizeDwords) * 4 <= context.meshLimits.maxMeshSharedMemorySize, "mesh output or LDS exceeds device limits");
    }
    const auto& viewport = state.viewport;
    Require(std::isfinite(viewport.minDepth) && std::isfinite(viewport.maxDepth), "non-finite viewport depth range");
    Require(context.depthRangeUnrestricted || (viewport.minDepth >= 0 && viewport.minDepth <= 1 && viewport.maxDepth >= 0 && viewport.maxDepth <= 1), "viewport depth outside [0, 1] requires VK_EXT_depth_range_unrestricted");
    Require(std::isfinite(viewport.x) && std::isfinite(viewport.y) && std::isfinite(viewport.width) && std::isfinite(viewport.height), "viewport arithmetic overflow");
    Require(viewport.width <= context.limits.maxViewportDimensions[0] && std::abs(viewport.height) <= context.limits.maxViewportDimensions[1], "viewport dimensions exceed device limits");
    Require(viewport.x >= context.limits.viewportBoundsRange[0] && viewport.x + viewport.width <= context.limits.viewportBoundsRange[1], "viewport X exceeds device bounds");
    Require(std::min(viewport.y, viewport.y + viewport.height) >= context.limits.viewportBoundsRange[0] && std::max(viewport.y, viewport.y + viewport.height) <= context.limits.viewportBoundsRange[1], "viewport Y exceeds device bounds");
    const auto pushStages = PushConstantStages(shaders);
    Require(pushStages == 0 || context.limits.maxPushConstantsSize >= PipelinePushConstantBytes, "graphics push constant range exceeds device limit");
    try {
        std::vector<VkPipelineShaderStageCreateInfo> stages(shaders.size());
        for (std::uint32_t i = 0; i < shaders.size(); ++i) {
            const auto& shader = *shaders[i].program;
            VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            module.codeSize = shader.spirv.size() * sizeof(std::uint32_t);
            module.pCode = shader.spirv.data();
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &module, nullptr, &_modules[i]), "vkCreateShaderModule graphics");
            const auto stage = VulkanStage(shaders[i].stage);
            stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].stage = stage;
            stages[i].module = _modules[i];
            stages[i].pName = "main";
        }
        const auto setLayout = resources.Layout();
        const VkPushConstantRange push{pushStages, 0, PipelinePushConstantBytes};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &setLayout;
        layoutInfo.pushConstantRangeCount = pushStages != 0 ? 1 : 0;
        layoutInfo.pPushConstantRanges = pushStages != 0 ? &push : nullptr;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout graphics");
        VkAttachmentDescription color{};
        color.format = state.color.format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = state.hasColorTarget ? 1 : 0;
        subpass.pColorAttachments = state.hasColorTarget ? &reference : nullptr;
        VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        passInfo.attachmentCount = state.hasColorTarget ? 1 : 0;
        passInfo.pAttachments = state.hasColorTarget ? &color : nullptr;
        passInfo.subpassCount = 1;
        passInfo.pSubpasses = &subpass;
        Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &renderPass), "vkCreateRenderPass");
        const auto view = target ? target->View() : VK_NULL_HANDLE;
        VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = state.hasColorTarget ? 1 : 0;
        framebufferInfo.pAttachments = state.hasColorTarget ? &view : nullptr;
        framebufferInfo.width = state.renderExtent.width;
        framebufferInfo.height = state.renderExtent.height;
        framebufferInfo.layers = 1;
        Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        const auto vertexInput = BuildVertexInputLayout(context, shaders.front().program->vertexAttributes);
        input.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vertexInput.bindings.size());
        input.pVertexBindingDescriptions = vertexInput.bindings.data();
        input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertexInput.attributes.size());
        input.pVertexAttributeDescriptions = vertexInput.attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = state.topology;
        VkPipelineViewportStateCreateInfo viewports{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        VkPipelineViewportDepthClipControlCreateInfoEXT depthClip{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT};
        depthClip.negativeOneToOne = state.negativeOneToOne;
        if (state.negativeOneToOne) viewports.pNext = &depthClip;
        viewports.viewportCount = 1;
        viewports.pViewports = &state.viewport;
        viewports.scissorCount = 1;
        viewports.pScissors = &state.scissor;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = state.cullMode;
        raster.frontFace = state.frontFace;
        raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = state.hasColorTarget ? 1 : 0;
        blend.pAttachments = state.hasColorTarget ? &state.blend : nullptr;
        std::copy(state.blendConstants.begin(), state.blendConstants.end(), blend.blendConstants);
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        VkPipelineTessellationStateCreateInfo tessellation{VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};
        if (state.rectList || state.stages.tessellation) {
            tessellation.patchControlPoints = state.rectList ? 3u : state.stages.tessellation->inputControlPoints;
            pipelineInfo.pTessellationState = &tessellation;
        }
        pipelineInfo.pVertexInputState = state.stages.mesh ? nullptr : &input;
        pipelineInfo.pInputAssemblyState = state.stages.mesh ? nullptr : &assembly;
        pipelineInfo.pViewportState = &viewports;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &samples;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.layout = layout;
        pipelineInfo.renderPass = renderPass;
        Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateGraphicsPipelines");
    } catch (...) {
        release();
        throw;
    }
}

Pipeline::~Pipeline() {
    release();
}

void Pipeline::release() noexcept {
    if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
    if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
    if (renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, renderPass, nullptr);
    if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
    for (auto module : _modules) {
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
    }
}

VkPipelineLayout Pipeline::Layout() const {
    return layout;
}

void Pipeline::Begin(VkCommandBuffer commands, VkExtent2D extent) const {
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = renderPass;
    begin.framebuffer = framebuffer;
    begin.renderArea = {{0, 0}, extent};
    context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void Pipeline::PushConstants(VkCommandBuffer commands, std::span<const CompiledShader> shaders) const {
    const auto stages = PushConstantStages(shaders);
    if (stages == 0) return;
    const auto bytes = AssemblePushConstants(shaders);
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, layout, stages, 0, PipelinePushConstantBytes, bytes.data());
}

}
