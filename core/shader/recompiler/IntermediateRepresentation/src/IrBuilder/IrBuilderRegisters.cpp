#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrBuilderInternal.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

IrValue& IrBuilder::ReadRegister(const GuestRegister& reg) {
    switch (reg.bank) {
    case RegisterBank::Scalar:
        return GetScalarReg(static_cast<ScalarReg>(reg.index));
    case RegisterBank::Vector:
        return GetVectorReg(static_cast<VectorReg>(reg.index));
    case RegisterBank::VectorConditionCode:
        return GetVcc();
    case RegisterBank::ScalarConditionCode:
        return GetScc();
    case RegisterBank::ExecutionMask:
        return GetExec();
    case RegisterBank::Memory:
        throw std::invalid_argument("IrBuilder::ReadRegister memory bank has no register representation");
    case RegisterBank::ThreadBitScalar:
        return GetThreadBitScalarReg(static_cast<ScalarReg>(reg.index));
    case RegisterBank::ScalarMaskTag:
        return GetScalarMaskTag(static_cast<ScalarReg>(reg.index));
    case RegisterBank::GotoVariable:
        return GetGotoVariable(reg.index);
    case RegisterBank::M0:
        return GetM0();
    case RegisterBank::UserData:
        return GetUserData(static_cast<ScalarReg>(reg.index));
    default:
        throw std::invalid_argument("IrBuilder::ReadRegister unknown register bank");
    }
}

void IrBuilder::WriteRegister(const GuestRegister& reg, IrValue& value) {
    switch (reg.bank) {
    case RegisterBank::Scalar:
        SetScalarReg(static_cast<ScalarReg>(reg.index), value);
        return;
    case RegisterBank::Vector:
        SetVectorReg(static_cast<VectorReg>(reg.index), value);
        return;
    case RegisterBank::VectorConditionCode:
        SetVcc(value);
        return;
    case RegisterBank::ScalarConditionCode:
        SetScc(value);
        return;
    case RegisterBank::ExecutionMask:
        SetExec(value);
        return;
    case RegisterBank::Memory:
        throw std::invalid_argument("IrBuilder::WriteRegister memory bank has no register representation");
    case RegisterBank::ThreadBitScalar:
        SetThreadBitScalarReg(static_cast<ScalarReg>(reg.index), value);
        return;
    case RegisterBank::ScalarMaskTag:
        SetScalarMaskTag(static_cast<ScalarReg>(reg.index), value);
        return;
    case RegisterBank::GotoVariable:
        SetGotoVariable(reg.index, value);
        return;
    case RegisterBank::M0:
        SetM0(value);
        return;
    case RegisterBank::UserData:
        throw std::invalid_argument("IrBuilder::WriteRegister user data has no setter");
    default:
        throw std::invalid_argument("IrBuilder::WriteRegister unknown register bank");
    }
}

IrValue& IrBuilder::GetUserData(ScalarReg reg) {
    IrValue& operand = createRegisterOperand(program, RegisterBank::UserData, RegIndex(reg), IrType::ScalarReg);
    return Emit(IrOpcode::GetUserData, IrType::U32, {&operand});
}

IrValue& IrBuilder::GetScalarReg(ScalarReg reg) {
    IrValue& operand = createRegisterOperand(program, RegisterBank::Scalar, RegIndex(reg), IrType::ScalarReg);
    return Emit(IrOpcode::GetScalarRegister, IrType::U32, {&operand});
}

void IrBuilder::SetScalarReg(ScalarReg reg, IrValue& value) {
    IrValue& operand = createRegisterOperand(program, RegisterBank::Scalar, RegIndex(reg), IrType::ScalarReg);
    (void)Emit(IrOpcode::SetScalarRegister, IrType::Void, {&operand, &value});
}

IrValue& IrBuilder::GetThreadBitScalarReg(ScalarReg reg) {
    IrValue& operand = createRegisterOperand(program, RegisterBank::ThreadBitScalar, RegIndex(reg), IrType::ScalarReg);
    return Emit(IrOpcode::GetThreadBitScalarRegister, IrType::Bool, {&operand});
}

void IrBuilder::SetThreadBitScalarReg(ScalarReg reg, IrValue& value) {
    IrValue& operand = createRegisterOperand(program, RegisterBank::ThreadBitScalar, RegIndex(reg), IrType::ScalarReg);
    (void)Emit(IrOpcode::SetThreadBitScalarRegister, IrType::Void, {&operand, &value});
}

IrValue& IrBuilder::GetScalarMaskTag(ScalarReg reg) {
    IrValue& operand = createRegisterOperand(program, RegisterBank::ScalarMaskTag, RegIndex(reg), IrType::ScalarReg);
    return Emit(IrOpcode::GetScalarMaskTag, IrType::Bool, {&operand});
}

void IrBuilder::SetScalarMaskTag(ScalarReg reg, IrValue& value) {
    IrValue& operand = createRegisterOperand(program, RegisterBank::ScalarMaskTag, RegIndex(reg), IrType::ScalarReg);
    (void)Emit(IrOpcode::SetScalarMaskTag, IrType::Void, {&operand, &value});
}

IrValue& IrBuilder::GetVectorReg(VectorReg reg) {
    IrValue& operand = createRegisterOperand(program, RegisterBank::Vector, RegIndex(reg), IrType::VectorReg);
    return Emit(IrOpcode::GetVectorRegister, IrType::U32, {&operand});
}

void IrBuilder::SetVectorReg(VectorReg reg, IrValue& value) {
    IrValue& operand = createRegisterOperand(program, RegisterBank::Vector, RegIndex(reg), IrType::VectorReg);
    (void)Emit(IrOpcode::SetVectorRegister, IrType::Void, {&operand, &value});
}

}
