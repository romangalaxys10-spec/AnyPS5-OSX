#include "Optimization/SrtWalker/SrtDescriptorEvaluation.hpp"
#include "Optimization/SrtWalker/SrtEvaluator.hpp"

#include <algorithm>

namespace ShaderRecompiler::Detail {

namespace {

const DescriptorSource* Source(const IrResourcePlan& program, std::uint32_t source) {
    if (source >= program.descriptorSources.size()) {
        return nullptr;
    }
    return &program.descriptorSources[source];
}

}

bool EvaluateRuntimeSourcesImpl(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, bool evaluateFlat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources) {
    if (!program.srtPlanComplete) {
        return false;
    }
    if (std::any_of(cleanFlatSlots.begin(), cleanFlatSlots.end(), [](std::uint8_t clean) { return clean != 0u; }) && runtime.readSpecializationMemory == nullptr) {
        return false;
    }
    SrtRuntime cleanRuntime = runtime;
    cleanRuntime.readMemory = runtime.readSpecializationMemory;
    Evaluator cleanEvaluator(program, cleanRuntime);
    Evaluator evaluator(program, runtime, cleanFlatSlots, &cleanEvaluator);
    std::vector<std::uint8_t> active;
    if (evaluateFlat) {
        active.assign(program.descriptorSources.size(), 1u);
    }
    if (evaluateFlat && !program.controlFlow.empty()) {
        for (const auto& block : program.controlFlow) {
            for (const auto source : block.sources) {
                active.at(source) = 0u;
            }
        }
        std::vector<std::uint8_t> visited(program.controlFlow.size());
        std::vector<std::uint32_t> pending {0};
        while (!pending.empty()) {
            const auto index = pending.back();
            pending.pop_back();
            if (visited.at(index)) {
                continue;
            }
            visited[index] = 1u;
            const auto& block = program.controlFlow[index];
            for (const auto source : block.sources) {
                active[source] = 1u;
            }
            std::uint32_t condition = 0;
            const bool cleanEvaluable = block.condition != nullptr && runtime.readSpecializationMemory != nullptr && cleanEvaluator.Evaluate(block.condition, condition);
            if (cleanEvaluable) {
                pending.push_back(block.successors[condition != 0u ? 0u : 1u]);
            } else {
                pending.insert(pending.end(), block.successors.begin(), block.successors.end());
            }
        }
    }
    std::vector<DescriptorValue> evaluated;
    evaluated.reserve(sources.size());
    for (const auto sourceIndex : sources) {
        const auto* source = Source(program, sourceIndex);
        if (source == nullptr) {
            return false;
        }
        DescriptorValue value;
        value.dwordCount = source->dwordCount;
        if (!evaluateFlat || active[sourceIndex]) {
            for (std::uint32_t index = 0; index < source->dwordCount; index++) {
                if (!evaluator.Evaluate(source->dwords[index], value.dwords[index])) {
                    return false;
                }
            }
        }
        evaluated.push_back(value);
    }
    std::vector<std::uint32_t> flattened;
    if (evaluateFlat) {
        flattened.resize(program.srtReads.size());
        for (const auto& read : program.srtReads) {
            const bool clean = read.flatOffset < cleanFlatSlots.size() && cleanFlatSlots[read.flatOffset] != 0u;
            auto& selected = clean ? cleanEvaluator : evaluator;
            if (read.flatOffset >= flattened.size() || !selected.Evaluate(read.value, flattened[read.flatOffset])) {
                return false;
            }
        }
    }
    results = std::move(evaluated);
    activeSources = std::move(active);
    if (evaluateFlat) {
        flat = std::move(flattened);
    }
    return true;
}

}
