#include "ControlFlow/GraphBuilder.hpp"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

std::string toHexString(std::uint32_t value) {
    char buffer[11];
    std::snprintf(buffer, sizeof(buffer), "0x%08x", value);
    return std::string(buffer);
}

std::uint32_t instructionEndProgramCounter(const RdnaInstruction& instruction) {
    return instruction.programCounter + instruction.wordCount * 4u;
}

void addUnique(std::vector<std::uint32_t>& values, std::uint32_t value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

void sortUnique(std::vector<std::uint32_t>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

std::uint32_t remapId(std::uint32_t id, const std::vector<std::uint32_t>& idMap) {
    return id != InvalidControlFlowId && id < idMap.size() ? idMap[id] : id;
}

void remapIds(std::vector<std::uint32_t>& values, const std::vector<std::uint32_t>& idMap) {
    for (auto& value : values) {
        value = remapId(value, idMap);
    }
    sortUnique(values);
}

bool isValidTarget(std::uint32_t target, const std::set<std::uint32_t>& instructionProgramCounters, std::uint32_t firstProgramCounter, std::uint32_t endProgramCounter) {
    return target == endProgramCounter || (target >= firstProgramCounter && instructionProgramCounters.contains(target));
}

bool isRegister(const RdnaOperand& operand, RdnaOperandKind kind, std::uint32_t reg) {
    return operand.kind == kind && operand.reg == reg;
}

bool isImmediate(const RdnaOperand& operand, std::uint32_t& value) {
    if (operand.kind == RdnaOperandKind::IntegerInlineConstant || operand.kind == RdnaOperandKind::LiteralConstant || operand.kind == RdnaOperandKind::FloatInlineConstant) {
        value = operand.value;
        return true;
    }
    return false;
}

bool resolveSetpcTarget(const RdnaProgram& program, std::uint32_t setpcIndex, std::uint32_t& target) {
    if (setpcIndex >= program.instructions.size()) {
        return false;
    }

    const auto& setpc = program.instructions[setpcIndex];
    if (setpc.op != RdnaOpcode::SSetpcB64 || setpc.source0.kind != RdnaOperandKind::ScalarRegister) {
        return false;
    }

    const auto pcRegister = setpc.source0.reg;
    if (setpcIndex >= 2u) {
        const auto& arithmetic = program.instructions[setpcIndex - 1u];
        const auto& getProgramCounter = program.instructions[setpcIndex - 2u];
        if (getProgramCounter.op == RdnaOpcode::SGetpcB64 && getProgramCounter.destination.kind == RdnaOperandKind::ScalarRegister && getProgramCounter.destination.reg == pcRegister && arithmetic.destination.kind == RdnaOperandKind::ScalarRegister && arithmetic.destination.reg == pcRegister) {
            std::uint32_t immediate = 0;
            const bool adds = arithmetic.op == RdnaOpcode::SAddU32 || arithmetic.op == RdnaOpcode::SAddI32;
            const bool subtracts = arithmetic.op == RdnaOpcode::SSubU32 || arithmetic.op == RdnaOpcode::SSubI32;
            if ((adds || subtracts) && (isRegister(arithmetic.source0, RdnaOperandKind::ScalarRegister, pcRegister) || isRegister(arithmetic.source1, RdnaOperandKind::ScalarRegister, pcRegister)) && (isImmediate(arithmetic.source0, immediate) || isImmediate(arithmetic.source1, immediate))) {
                const auto base = instructionEndProgramCounter(getProgramCounter);
                target = (adds ? base + immediate : base - immediate) & ~3u;
                return true;
            }
        }
    }

    if (setpcIndex >= 1u) {
        const auto& getProgramCounter = program.instructions[setpcIndex - 1u];
        if (getProgramCounter.op == RdnaOpcode::SGetpcB64 && getProgramCounter.destination.kind == RdnaOperandKind::ScalarRegister && getProgramCounter.destination.reg == pcRegister) {
            target = instructionEndProgramCounter(getProgramCounter);
            return true;
        }
    }

    return false;
}

BranchCondition conditionForOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::SBranch: return BranchCondition::Always;
        case RdnaOpcode::SCbranchScc0: return BranchCondition::SccZero;
        case RdnaOpcode::SCbranchScc1: return BranchCondition::SccNonZero;
        case RdnaOpcode::SCbranchVccz: return BranchCondition::VccZero;
        case RdnaOpcode::SCbranchVccnz: return BranchCondition::VccNonZero;
        case RdnaOpcode::SCbranchExecz: return BranchCondition::ExecZero;
        case RdnaOpcode::SCbranchExecnz: return BranchCondition::ExecNonZero;
        case RdnaOpcode::SSubvectorLoopBegin:
        case RdnaOpcode::SSubvectorLoopEnd: return BranchCondition::ScalarInstruction;
        default: break;
    }
    throw std::logic_error("unreachable branch condition for opcode " + std::to_string(static_cast<int>(opcode)));
}

void rebuildPredecessors(ControlFlowGraph& graph) {
    for (auto& block : graph.blocks) {
        block.predecessors.clear();
        sortUnique(block.successors);
    }
    for (const auto& block : graph.blocks) {
        for (const auto successor : block.successors) {
            addUnique(graph.blocks[successor].predecessors, block.id);
        }
    }
    for (auto& block : graph.blocks) {
        sortUnique(block.predecessors);
    }
}

void pruneUnreachableBlocks(ControlFlowGraph& graph) {
    if (graph.entryBlock >= graph.blocks.size()) {
        throw std::invalid_argument("control flow graph entry block is out of range");
    }

    std::vector<bool> reachable(graph.blocks.size(), false);
    std::vector<std::uint32_t> pending{graph.entryBlock};
    while (!pending.empty()) {
        const auto blockId = pending.back();
        pending.pop_back();
        if (blockId >= graph.blocks.size() || reachable[blockId]) {
            continue;
        }
        reachable[blockId] = true;
        for (const auto successor : graph.blocks[blockId].successors) {
            pending.push_back(successor);
        }
    }

    if (std::ranges::all_of(reachable, [](bool value) { return value; })) {
        return;
    }

    std::vector<std::uint32_t> idMap(graph.blocks.size(), InvalidControlFlowId);
    std::vector<BasicBlock> blocks;
    blocks.reserve(static_cast<std::size_t>(std::ranges::count(reachable, true)));
    for (std::uint32_t oldId = 0; oldId < graph.blocks.size(); ++oldId) {
        if (!reachable[oldId]) {
            continue;
        }
        idMap[oldId] = static_cast<std::uint32_t>(blocks.size());
        blocks.push_back(std::move(graph.blocks[oldId]));
    }

    graph.entryBlock = remapId(graph.entryBlock, idMap);
    graph.blocks = std::move(blocks);
    for (auto& block : graph.blocks) {
        block.id = remapId(block.id, idMap);
        remapIds(block.successors, idMap);
        block.predecessors.clear();
        block.terminator.trueBlock = remapId(block.terminator.trueBlock, idMap);
        block.terminator.falseBlock = remapId(block.terminator.falseBlock, idMap);
    }

    rebuildPredecessors(graph);
}

}

std::vector<BasicBlock> GraphBuilder::splitIntoBlocks(const RdnaProgram& program) const {
    if (program.instructions.empty()) {
        throw std::invalid_argument("cannot build a control flow graph for an empty program");
    }

    const std::uint32_t firstProgramCounter = program.instructions.front().programCounter;
    const std::uint32_t endProgramCounter = instructionEndProgramCounter(program.instructions.back());

    std::set<std::uint32_t> instructionProgramCounters;
    for (const auto& instruction : program.instructions) {
        instructionProgramCounters.insert(instruction.programCounter);
    }

    std::set<std::uint32_t> labels;
    labels.insert(firstProgramCounter);
    labels.insert(endProgramCounter);

    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        const std::uint32_t nextProgramCounter = instructionEndProgramCounter(instruction);

        if (IsDirectBranchOpcode(instruction.op)) {
            if (!isValidTarget(instruction.branchTarget, instructionProgramCounters, firstProgramCounter, endProgramCounter)) {
                throw std::invalid_argument("branch at program counter " + toHexString(instruction.programCounter) + " targets invalid program counter " + toHexString(instruction.branchTarget));
            }
            labels.insert(instruction.branchTarget);
            if (nextProgramCounter <= endProgramCounter) {
                labels.insert(nextProgramCounter);
            }
        } else if (instruction.op == RdnaOpcode::SSetpcB64) {
            std::uint32_t target = 0;
            if (!resolveSetpcTarget(program, index, target)) {
                throw std::invalid_argument("unsupported dynamic s_setpc_b64 at program counter " + toHexString(instruction.programCounter));
            }
            if (!isValidTarget(target, instructionProgramCounters, firstProgramCounter, endProgramCounter)) {
                throw std::invalid_argument("s_setpc_b64 at program counter " + toHexString(instruction.programCounter) + " targets invalid program counter " + toHexString(target));
            }
            labels.insert(target);
            if (nextProgramCounter <= endProgramCounter) {
                labels.insert(nextProgramCounter);
            }
        } else if (instruction.op == RdnaOpcode::SEndpgm) {
            labels.insert(nextProgramCounter);
        }
    }

    const std::vector<std::uint32_t> sortedLabels(labels.begin(), labels.end());

    std::vector<BasicBlock> blocks;
    blocks.reserve(sortedLabels.size());
    for (std::size_t i = 0; i < sortedLabels.size(); ++i) {
        const std::uint32_t start = sortedLabels[i];
        if (start > endProgramCounter) {
            continue;
        }
        if (start != endProgramCounter && !instructionProgramCounters.contains(start)) {
            throw std::invalid_argument("control flow graph label does not start on an instruction boundary: " + toHexString(start));
        }

        BasicBlock block;
        block.id = static_cast<std::uint32_t>(blocks.size());
        block.startProgramCounter = start;
        block.endProgramCounter = i + 1u < sortedLabels.size() ? sortedLabels[i + 1u] : endProgramCounter;
        block.instructionBegin = static_cast<std::uint32_t>(std::lower_bound(program.instructions.begin(), program.instructions.end(), block.startProgramCounter, [](const RdnaInstruction& instruction, std::uint32_t programCounter) { return instruction.programCounter < programCounter; }) - program.instructions.begin());
        block.instructionEnd = static_cast<std::uint32_t>(std::lower_bound(program.instructions.begin(), program.instructions.end(), block.endProgramCounter, [](const RdnaInstruction& instruction, std::uint32_t programCounter) { return instruction.programCounter < programCounter; }) - program.instructions.begin());
        blocks.push_back(std::move(block));
    }

    return blocks;
}

void GraphBuilder::linkBlocks(std::vector<BasicBlock>& blocks, const RdnaProgram& program) const {
    std::map<std::uint32_t, std::uint32_t> programCounterToBlock;
    for (const auto& block : blocks) {
        programCounterToBlock.emplace(block.startProgramCounter, block.id);
    }

    for (auto& block : blocks) {
        block.terminator = Terminator{};
        if (block.instructionBegin == block.instructionEnd) {
            block.terminator.kind = TerminatorKind::Return;
            continue;
        }

        const auto& last = program.instructions[block.instructionEnd - 1u];
        const std::uint32_t nextProgramCounter = instructionEndProgramCounter(last);

        if (last.op == RdnaOpcode::SEndpgm) {
            block.terminator.kind = TerminatorKind::Return;
        } else if (last.op == RdnaOpcode::SSetpcB64) {
            std::uint32_t target = 0;
            if (!resolveSetpcTarget(program, block.instructionEnd - 1u, target)) {
                throw std::invalid_argument("unsupported dynamic s_setpc_b64 at program counter " + toHexString(last.programCounter));
            }
            block.terminator.kind = TerminatorKind::Branch;
            block.terminator.condition = BranchCondition::Always;
            block.terminator.trueBlock = programCounterToBlock.at(target);
        } else if (last.op == RdnaOpcode::SBranch) {
            block.terminator.kind = TerminatorKind::Branch;
            block.terminator.condition = BranchCondition::Always;
            block.terminator.trueBlock = programCounterToBlock.at(last.branchTarget);
        } else if (IsConditionalBranchOpcode(last.op)) {
            block.terminator.kind = TerminatorKind::ConditionalBranch;
            block.terminator.condition = conditionForOpcode(last.op);
            block.terminator.trueBlock = programCounterToBlock.at(last.branchTarget);
            const auto fallthrough = programCounterToBlock.find(nextProgramCounter);
            if (fallthrough == programCounterToBlock.end()) {
                throw std::invalid_argument("conditional branch at program counter " + toHexString(last.programCounter) + " has no fallthrough block");
            }
            block.terminator.falseBlock = fallthrough->second;
        } else {
            const auto next = programCounterToBlock.find(block.endProgramCounter);
            if (next != programCounterToBlock.end() && block.endProgramCounter != block.startProgramCounter) {
                block.terminator.kind = TerminatorKind::Branch;
                block.terminator.condition = BranchCondition::Always;
                block.terminator.trueBlock = next->second;
            } else {
                block.terminator.kind = TerminatorKind::Return;
            }
        }

        switch (block.terminator.kind) {
            case TerminatorKind::Branch: addUnique(block.successors, block.terminator.trueBlock); break;
            case TerminatorKind::ConditionalBranch:
                addUnique(block.successors, block.terminator.trueBlock);
                addUnique(block.successors, block.terminator.falseBlock);
                break;
            case TerminatorKind::IndirectBranch:
            case TerminatorKind::Return:
            case TerminatorKind::Unsupported: break;
        }
    }

    for (auto& block : blocks) {
        sortUnique(block.successors);
    }
    for (const auto& block : blocks) {
        for (const auto successor : block.successors) {
            addUnique(blocks[successor].predecessors, block.id);
        }
    }
    for (auto& block : blocks) {
        sortUnique(block.predecessors);
    }
}

ControlFlowGraph GraphBuilder::Build(const RdnaProgram& program) const {
    ControlFlowGraph graph;
    graph.blocks = splitIntoBlocks(program);
    linkBlocks(graph.blocks, program);
    graph.entryBlock = 0;
    pruneUnreachableBlocks(graph);
    return graph;
}

}
