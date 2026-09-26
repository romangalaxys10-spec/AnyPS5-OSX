#include "Translation/CompareInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <stdexcept>

namespace ShaderRecompiler {

void TranslateCompareInstruction(IrBuilder& builder, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateCompareInstruction not implemented");
}

void TranslationContext::emitCompareResult(const RdnaInstruction& inst, IrU1 value, bool scalar, bool cmpx) {
    if (scalar) {
        ir.SetScc(value.Value());
        return;
    }
    IrValue& masked = ir.LogicalAnd(ir.GetExec(), value.Value());
    if (cmpx) {
        const std::array<IrU32, 2> mask = ballotMask(IrU1(masked));
        ir.SetExec(masked);
        ir.SetExecLo(mask[0].Value());
        ir.SetExecHi(mask[1].Value());
        return;
    }
    writeMask(inst.destination, IrU1(masked));
}

void TranslationContext::emitCompareConstant(const RdnaInstruction& inst, bool value, bool scalar, bool cmpx) {
    emitCompareResult(inst, IrU1(ir.ConstantBool(value)), scalar, cmpx);
}

void TranslationContext::emitIntegerCompare(const RdnaInstruction& inst, IrOpcode opcode, IrType type, bool scalar, bool cmpx) {
    IrValue* lhs = readOperand(sourceAt(inst, 0u), type);
    IrValue* rhs = readOperand(sourceAt(inst, 1u), type);
    emitCompareResult(inst, IrU1(ir.Emit(opcode, IrType::U1, {lhs, rhs})), scalar, cmpx);
}

void TranslationContext::emitInteger16Compare(const RdnaInstruction& inst, IrOpcode opcode, bool signedValue, bool cmpx) {
    const IrU32 lhs = readU16AsU32(sourceAt(inst, 0u), signedValue);
    const IrU32 rhs = readU16AsU32(sourceAt(inst, 1u), signedValue);
    emitCompareResult(inst, IrU1(ir.Emit(opcode, IrType::U1, {&lhs.Value(), &rhs.Value()})), false, cmpx);
}

void TranslationContext::emitFloatCompare(const RdnaInstruction& inst, IrOpcode opcode, bool half, bool cmpx) {
    IrValue* lhs = nullptr;
    IrValue* rhs = nullptr;
    if (half) {
        lhs = &readF16AsF32(sourceAt(inst, 0u)).Value();
        rhs = &readF16AsF32(sourceAt(inst, 1u)).Value();
    } else {
        lhs = readOperand(sourceAt(inst, 0u), IrType::F32);
        rhs = readOperand(sourceAt(inst, 1u), IrType::F32);
    }
    emitCompareResult(inst, IrU1(ir.Emit(opcode, IrType::U1, {lhs, rhs})), false, cmpx);
}

void TranslationContext::emitFloatOrderedCompare(const RdnaInstruction& inst, bool ordered) {
    IrValue* lhs = readOperand(sourceAt(inst, 0u), IrType::F32);
    IrValue* rhs = readOperand(sourceAt(inst, 1u), IrType::F32);
    IrValue& lhsNan = ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {lhs});
    IrValue& rhsNan = ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {rhs});
    IrValue& unordered = ir.LogicalOr(lhsNan, rhsNan);
    IrValue& result = ordered ? ir.LogicalNot(unordered) : unordered;
    emitCompareResult(inst, IrU1(result), false, false);
}

void TranslationContext::emitFloatClassCompare(const RdnaInstruction& inst, bool cmpx) {
    IrValue* value = readOperand(sourceAt(inst, 0u), IrType::F32);
    IrValue* mask = readOperand(sourceAt(inst, 1u), IrType::U32);
    emitCompareResult(inst, IrU1(ir.Emit(IrOpcode::FPCmpClass32, IrType::U1, {value, mask})), false, cmpx);
}

void TranslateCompareInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateCompareInstruction not implemented");
}

}
