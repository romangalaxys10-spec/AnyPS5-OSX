#include "Recompiler.hpp"
#include "CacheKey.hpp"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include "ControlFlow/include/ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/include/ControlFlow/Structurizer.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "IntermediateRepresentation/include/IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/include/Optimization/BindingAllocator.hpp"
#include "Optimization/include/Optimization/ConstantFolder.hpp"
#include "Optimization/include/Optimization/DeadCodeEliminator.hpp"
#include "Optimization/include/Optimization/DescriptorBindingBuilder.hpp"
#include "Optimization/include/Optimization/ReadLaneEliminator.hpp"
#include "Optimization/include/Optimization/RequestMemoryView.hpp"
#include "Optimization/include/Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/include/Optimization/ResourceTracker.hpp"
#include "Optimization/include/Optimization/ShaderInfoCollector.hpp"
#include "Optimization/include/Optimization/SrtWalker.hpp"
#include "Optimization/include/Optimization/SsaBuilder.hpp"
#include "SpirvBackend/include/SpirvBackend/SpirvEmitter.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "SpirvBackend/SpirvMemory/SpirvInputOutput.hpp"
#include "Translation/include/Translation/InstructionTranslator.hpp"
#include "Translation/include/Translation/ShaderInputInfoBuilder.hpp"
#include <exception>
#include <stdexcept>
#include <string>
#include <ControlFlow/RequestSerializer.hpp>

namespace ShaderRecompiler {

namespace {

ShaderStageKind toShaderStageKind(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Compute:
        return ShaderStageKind::Compute;
    case ShaderStage::Vertex:
        return ShaderStageKind::Vertex;
    case ShaderStage::TessellationControl:
        return ShaderStageKind::TessellationControl;
    case ShaderStage::TessellationEvaluation:
        return ShaderStageKind::TessellationEvaluation;
    case ShaderStage::Fragment:
        return ShaderStageKind::Pixel;
    case ShaderStage::Local:
        return ShaderStageKind::Local;
    case ShaderStage::Mesh:
        return ShaderStageKind::Mesh;
    case ShaderStage::Geometry:
        break;
    }
    throw std::runtime_error("ShaderRecompiler::Recompile: unsupported shader stage");
}

}

IrProgram PrepareResourceProgram(const RecompileRequest& request) {
    const auto stageKind = toShaderStageKind(request.shader.stage);
    const auto inputInfo = BuildShaderStageInputInfo(stageKind, request.context);

    constexpr RdnaInstructionDecoder decoder;
    const auto decoded = decoder.Decode(request.shader.code);

    constexpr GraphBuilder graphBuilder;
    auto cfg = graphBuilder.Build(decoded);

    constexpr Structurizer structurizer;
    structurizer.Structurize(cfg);

    TranslateOptions translateOptions {};
    translateOptions.stage = stageKind;
    translateOptions.waveSize = request.context.waveSize;
    translateOptions.userDataBaseRegister = request.context.userDataBaseRegister;
    translateOptions.userDataCount = static_cast<std::uint32_t>(request.context.userData.size());
    translateOptions.embeddedFetch = nullptr;
    translateOptions.fragmentShaderBarycentricEnabled = request.target.fragmentShaderBarycentricEnabled;
    translateOptions.inputInfo = inputInfo;

    constexpr InstructionTranslator translator;

    EmbeddedFetchPlan embeddedFetch;
    if ((stageKind == ShaderStageKind::Vertex || stageKind == ShaderStageKind::Local) && inputInfo.vertex != nullptr && inputInfo.vertex->fetchEmbedded) {
        constexpr EmbeddedVertexFetchAnalyzer embeddedFetchAnalyzer;
        embeddedFetch = embeddedFetchAnalyzer.Analyze(decoded, inputInfo.vertex->fetchAttribReg, inputInfo.vertex->fetchBufferReg, request.context.userDataBaseRegister, static_cast<std::uint32_t>(request.context.userData.size()), request.context.waveSize);
    }
    translateOptions.embeddedFetch = embeddedFetch.loads.empty() ? nullptr : &embeddedFetch;

    auto program = translator.Translate(decoded, cfg, translateOptions);

    constexpr SsaBuilder ssaBuilder;
    ssaBuilder.Rewrite(program);

    constexpr ConstantFolder constantFolder;
    constexpr DeadCodeEliminator deadCodeEliminator;

    constantFolder.Fold(program);
    ResolveControlFlowIdentities(program);
    deadCodeEliminator.RemoveIdentities(program);
    deadCodeEliminator.Eliminate(program);

    constexpr ReadLaneEliminator readLaneEliminator;
    const auto readLaneStats = readLaneEliminator.Eliminate(program, translateOptions.waveSize);
    if (readLaneStats.rewrittenReads != 0u) {
        constantFolder.Fold(program);
        ResolveControlFlowIdentities(program);
        deadCodeEliminator.RemoveIdentities(program);
        deadCodeEliminator.Eliminate(program);
    }

    constexpr SrtWalker srtWalker;
    srtWalker.BuildPlan(program);
    deadCodeEliminator.Eliminate(program);

    constexpr ResourceTracker resourceTracker;
    resourceTracker.Track(program);
    deadCodeEliminator.Eliminate(program);

    return program;
}

namespace {

struct CompiledVariant {
    ResourceSpecialization specialization;
    BindingLayout layout;
    CompiledShaderInfo info;
    BindingAllocationResult bindings;
    RecompileResult result;
};

struct ResourceProgram {
    explicit ResourceProgram(const RecompileRequest& request) : program(PrepareResourceProgram(request)), plan(ResourceMaterializer{}.ExtractPlan(program)) {}

    IrProgram program;
    IrResourcePlan plan;
};

std::shared_ptr<const IrResourcePlan> makeResourcePlan(const RecompileRequest& request) {
    const auto resource = std::make_shared<ResourceProgram>(request);
    return std::shared_ptr<const IrResourcePlan>(resource, &resource->plan);
}

struct SourceEntry {
    std::mutex mutex;
    std::shared_ptr<const IrResourcePlan> plan;
    std::vector<std::shared_ptr<const CompiledVariant>> variants;
};

struct SourceKeyHash {
    std::size_t operator()(const std::vector<std::uint64_t>& key) const {
        std::size_t hash = 0;
        for (const auto value : key) {
            hash ^= static_cast<std::size_t>(value) + static_cast<std::size_t>(0x9e3779b97f4a7c15ull) + (hash << 6u) + (hash >> 2u);
            if constexpr (sizeof(std::size_t) < sizeof(value)) hash ^= static_cast<std::size_t>(value >> 32u);
        }
        return hash;
    }
};

std::shared_ptr<SourceEntry> getSource(const RecompileRequest& request) {
    static std::shared_mutex mutex;
    static std::unordered_map<std::vector<std::uint64_t>, std::shared_ptr<SourceEntry>, SourceKeyHash> sources;
    thread_local std::vector<std::uint64_t> key;
    RecompileCacheKey::Build(request, key);
    std::shared_ptr<SourceEntry> source;
    {
        std::shared_lock lock(mutex);
        const auto found = sources.find(key);
        if (found != sources.end()) source = found->second;
    }
    if (source == nullptr) {
        std::unique_lock lock(mutex);
        const auto found = sources.find(key);
        if (found != sources.end()) source = found->second;
        else {
            source = std::make_shared<SourceEntry>();
            sources.emplace(key, source);
        }
    }
    {
        std::lock_guard lock(source->mutex);
        if (source->plan == nullptr) {
            source->plan = makeResourcePlan(request);
        }
    }
    return source;
}

CompiledVariant compileVariant(const RecompileRequest& request, IrProgram program, const ResourceSnapshot& resourceSnapshot, const ResourceSpecialization& resourceSpecialization) {
    const auto inputInfo = BuildShaderStageInputInfo(toShaderStageKind(request.shader.stage), request.context);
    constexpr DeadCodeEliminator deadCodeEliminator;
    constexpr ResourceMaterializer resourceMaterializer;
    resourceMaterializer.Apply(program, resourceSpecialization);

    deadCodeEliminator.RemoveIdentities(program);
    deadCodeEliminator.Eliminate(program);

    constexpr ShaderInfoCollector shaderInfoCollector;
    shaderInfoCollector.Collect(program, inputInfo);

    constexpr BindingAllocator bindingAllocator;
    auto bindings = bindingAllocator.Allocate(program, request.layout);

    constexpr DescriptorBindingBuilder descriptorBindingBuilder;
    descriptorBindingBuilder.Populate(bindings, program, resourceSnapshot);

    SpirvTargetOptions targetOptions {};
    targetOptions.vulkanVersion = request.target.vulkanVersion;
    targetOptions.spirvVersion = request.target.spirvVersion;
    targetOptions.subgroupSize = request.target.subgroupSize;
    targetOptions.bdaAbiVersion = request.target.bdaAbiVersion;
    targetOptions.supportedCapabilities = request.target.supportedCapabilities;
    targetOptions.supportedExtensions = request.target.supportedExtensions;

    constexpr SpirvEmitter spirvEmitter;
    RecompileResult result;
    result.spirv = spirvEmitter.Emit(program, inputInfo, bindings, targetOptions);

#if ANYPS5_ENABLE_SPIRV_TOOLS
    result.spirv = ValidateAndOptimizeSpirv(result.spirv, request.target.vulkanVersion, request.target.spirvVersion);
#endif

    result.bdaAbiVersion = program.Info().usesDma ? request.target.bdaAbiVersion : 0u;
    result.vertexOffsetSgpr = program.Info().vertexOffsetSgpr;
    result.instanceOffsetSgpr = program.Info().instanceOffsetSgpr;
    for (const auto& output : program.Info().outputs) {
        if (output.kind == StageOutputKind::Parameter) result.parameterExports.push_back(output.location);
    }
    if (request.shader.stage == ShaderStage::Fragment) result.fragmentParameters = DescribeFragmentParameters(program, inputInfo);
    if (request.shader.stage == ShaderStage::Vertex || request.shader.stage == ShaderStage::Local) {
        if (inputInfo.vertex == nullptr) throw std::runtime_error("vertex input metadata is missing");
        for (const auto& input : program.Info().inputs) {
            if (input.kind != StageInputKind::Parameter) continue;
            if (input.location >= static_cast<std::uint32_t>(inputInfo.vertex->resourcesNum)) throw std::runtime_error("vertex attribute location exceeds resource count");
            result.vertexAttributes.push_back({input.location, input.componentCount, {inputInfo.vertex->resources[input.location].fields}, inputInfo.vertex->resourcesDst[input.location].fetchIndex});
        }
    }

    result.bindings.clear();
    result.pushConstants.clear();
    for (auto& attribute : result.vertexAttributes) attribute.resource = {};
    bindings.bindings.clear();
    bindings.pushConstants.clear();
    return {resourceSpecialization, request.layout, std::move(program).TakeCompiledInfo(), std::move(bindings), std::move(result)};
}

RecompileResult materializeResult(const CompiledVariant& variant, const RecompileRequest& request, const ResourceSnapshot& snapshot) {
    auto result = variant.result;
    BindingAllocationResult bindings;
    bindings.layout = variant.bindings.layout;
    bindings.pushConstantOffsetBytes = variant.bindings.pushConstantOffsetBytes;
    bindings.pushConstantSizeBytes = variant.bindings.pushConstantSizeBytes;
    DescriptorBindingBuilder{}.Populate(bindings, variant.info.info, variant.info.stage, variant.info.userDataBase, snapshot);
    result.bindings = std::move(bindings.bindings);
    result.pushConstants = std::move(bindings.pushConstants);
    for (auto& attribute : result.vertexAttributes) {
        if (!request.context.vertex || attribute.location >= request.context.vertex->resourcesNum) throw std::runtime_error("Shader cache: invalid vertex attribute metadata");
        attribute.resource = request.context.vertex->resources[attribute.location];
    }
    return result;
}

bool sameLayout(const BindingLayout& left, const BindingLayout& right) {
    return left.descriptorSet == right.descriptorSet && left.firstBinding == right.firstBinding && left.pushConstantOffsetBytes == right.pushConstantOffsetBytes && left.pushConstantSizeBytes == right.pushConstantSizeBytes;
}

RecompileResult RecompileImpl(const RecompileRequest& request) {
    static_cast<void>(BuildShaderStageInputInfo(toShaderStageKind(request.shader.stage), request.context));
    RequestMemoryView memory(request.context.memory);
    const auto runtime = memory.MakeRuntime(request.context.userData, request.shader.codeAddress);
    ResourceSnapshot snapshot;
    ResourceSpecialization specialization;
    constexpr ResourceMaterializer materializer;
    if (!request.useCache) {
        auto program = PrepareResourceProgram(request);
        const auto plan = materializer.ExtractPlan(program);
        materializer.Materialize(plan, runtime, snapshot, specialization);
        const auto variant = compileVariant(request, std::move(program), snapshot, specialization);
        return materializeResult(variant, request, snapshot);
    }
    const auto source = getSource(request);
    materializer.Materialize(*source->plan, runtime, snapshot, specialization);
    std::shared_ptr<const CompiledVariant> variant;
    bool cacheHit = false;
    {
        std::lock_guard lock(source->mutex);
        for (const auto& candidate : source->variants) {
            if (sameLayout(candidate->layout, request.layout) && candidate->specialization == specialization) {
                variant = candidate;
                cacheHit = true;
                break;
            }
        }
        if (variant == nullptr) {
            auto program = PrepareResourceProgram(request);
            variant = std::make_shared<CompiledVariant>(compileVariant(request, std::move(program), snapshot, specialization));
            source->variants.push_back(variant);
        }
    }
    auto result = materializeResult(*variant, request, snapshot);
    result.cacheHit = cacheHit;
    return result;
}

}

std::shared_ptr<const IrResourcePlan> GetResourcePlan(const RecompileRequest& request) {
    static_cast<void>(BuildShaderStageInputInfo(toShaderStageKind(request.shader.stage), request.context));
    if (request.useCache) return getSource(request)->plan;
    return makeResourcePlan(request);
}

RecompileResult Recompile(const RecompileRequest& request) {
    try {
        return RecompileImpl(request);
    } catch (const std::exception& e) {
        constexpr auto requestSerializer = RequestSerializer{};
        const auto inputInfo = "\nRecompileRequest:\n" + requestSerializer.Serialize(request);
        throw std::runtime_error(std::string("ShaderRecompiler::Recompile: ") + e.what() + inputInfo);
    } catch (...) {
        throw std::runtime_error("ShaderRecompiler::Recompile: unknown exception");
    }
}

}
