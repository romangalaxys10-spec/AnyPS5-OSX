#include "Optimization/ReadLaneEliminator.hpp"
#include <algorithm>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ShaderRecompiler {
namespace {

struct ChainResult {
    IrValue* value = nullptr;
    IrValue* write = nullptr;
};

bool isImmediateU32(const IrValue& value) {
    return value.HasImmediate() && value.Type() == IrType::U32;
}

ChainResult searchChain(IrValue* value, std::uint32_t lane, std::uint32_t waveSize) {
    for (;;) {
        value = value->Resolve();
        if (value->Opcode() != IrOpcode::WriteLane) {
            return ChainResult {value, nullptr};
        }
        IrValue* selector = value->Argument(1)->Resolve();
        if (!isImmediateU32(*selector)) {
            return ChainResult {value, nullptr};
        }
        if (selector->ImmediateU32() % waveSize == lane) {
            return ChainResult {value, value};
        }
        value = value->Argument(2);
    }
}

bool isPossibleToEliminate(IrValue* source, std::uint32_t lane, std::uint32_t waveSize) {
    std::queue<IrValue*> queue;
    std::unordered_set<IrValue*> visited;
    queue.push(source);

    while (!queue.empty()) {
        const ChainResult chain = searchChain(queue.front(), lane, waveSize);
        queue.pop();
        if (chain.write != nullptr) {
            continue;
        }
        IrValue* inst = chain.value;
        if (!inst->IsPhi() || inst->ArgumentCount() == 0u) {
            return false;
        }
        if (!visited.insert(inst).second) {
            continue;
        }
        for (std::size_t index = inst->ArgumentCount(); index-- > 0;) {
            queue.push(inst->Argument(index));
        }
    }
    return true;
}

using PhiMap = std::unordered_map<IrValue*, IrValue*>;

IrValue* getRealValue(IrProgram& program, PhiMap& phiMap, IrValue* source, std::uint32_t lane, std::uint32_t waveSize) {
    const ChainResult chain = searchChain(source, lane, waveSize);
    if (chain.write != nullptr) {
        return chain.write->Argument(0);
    }

    IrValue* inst = chain.value;
    if (!inst->IsPhi()) {
        throw std::runtime_error("ReadLaneEliminator::Eliminate reached a non-phi value while rewriting a lane chain");
    }
    const auto [entry, isNew] = phiMap.try_emplace(inst);
    if (!isNew) {
        return entry->second;
    }

    IrBlock* block = inst->Parent();
    IrValue& phi = program.CreateValue(IrOpcode::Phi, IrType::U32);
    block->InsertInstructionBefore(inst, &phi);
    entry->second = &phi;

    std::vector<IrValue*> arguments;
    arguments.reserve(inst->ArgumentCount());
    for (std::size_t index = 0; index < inst->ArgumentCount(); index++) {
        arguments.push_back(getRealValue(program, phiMap, inst->Argument(index), lane, waveSize));
    }
    IrValue* first = arguments.front()->Resolve();
    if (std::ranges::all_of(arguments, [first](IrValue* argument) { return argument->Resolve() == first; })) {
        phi.ReplaceUsesWith(first, true);
    } else {
        for (std::size_t index = 0; index < arguments.size(); index++) {
            phi.AddPhiOperand(inst->PhiBlock(index), arguments[index]);
        }
    }
    return &phi;
}

}

ReadLaneEliminationStats ReadLaneEliminator::Eliminate(IrProgram& program, std::uint32_t waveSize) const {
    ReadLaneEliminationStats stats;
    if (waveSize != 32u && waveSize != 64u) {
        return stats;
    }

    for (const auto& block : program.Blocks()) {
        for (IrValue* inst : block->Instructions()) {
            if (inst->Opcode() != IrOpcode::ReadLane) {
                continue;
            }
            IrValue* selector = inst->Argument(1)->Resolve();
            if (!isImmediateU32(*selector)) {
                continue;
            }

            const std::uint32_t lane = selector->ImmediateU32() % waveSize;
            const ChainResult chain = searchChain(inst->Argument(0), lane, waveSize);
            if (chain.write != nullptr) {
                inst->ReplaceUsesWith(chain.write->Argument(0), true);
                stats.rewrittenReads++;
                continue;
            }
            if (!chain.value->IsPhi() || !isPossibleToEliminate(chain.value, lane, waveSize)) {
                continue;
            }

            PhiMap phiMap;
            inst->ReplaceUsesWith(getRealValue(program, phiMap, chain.value, lane, waveSize), true);
            stats.rewrittenReads++;
        }
    }
    return stats;
}

}
