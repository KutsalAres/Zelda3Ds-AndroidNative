#include "spirv_builder.h"

#include <cstring>
#include <stdexcept>

namespace PicaToSpirv {

namespace {

void EmitInstruction(std::vector<uint32_t>& out, SpvOp op, std::span<const uint32_t> operands) {
    uint32_t wordCount = static_cast<uint32_t>(operands.size() + 1);
    out.push_back((wordCount << 16) | (static_cast<uint32_t>(op) & 0xFFFFU));
    out.insert(out.end(), operands.begin(), operands.end());
}

} // namespace

SpirvBuilder::SpirvBuilder(uint32_t version)
    : mVersion(version) {
}

uint32_t SpirvBuilder::AllocId() {
    return mNextId++;
}

void SpirvBuilder::AppendString(std::vector<uint32_t>& target, std::string_view str) {
    size_t len = str.length() + 1; // null terminator
    size_t words = (len + 3) / 4;
    size_t startIdx = target.size();
    target.resize(startIdx + words, 0);
    std::memcpy(&target[startIdx], str.data(), str.length());
}

void SpirvBuilder::AddCapability(SpvCapability cap) {
    uint32_t ops[] = { static_cast<uint32_t>(cap) };
    EmitInstruction(mCapabilities, SpvOpCapability, ops);
}

uint32_t SpirvBuilder::ImportExtInst(std::string_view name) {
    uint32_t id = AllocId();
    std::vector<uint32_t> ops;
    ops.push_back(id);
    AppendString(ops, name);
    EmitInstruction(mExtInstImports, SpvOpExtInstImport, ops);
    return id;
}

void SpirvBuilder::SetMemoryModel(SpvMemoryModel model) {
    uint32_t ops[] = {
        static_cast<uint32_t>(SpvOpMemoryModel),
        static_cast<uint32_t>(model)
    };
    // OpMemoryModel AddressingModel MemoryModel
    // In SPIR-V, AddressingModel Logical = 0
    uint32_t instr[] = { 0 /* Logical */, static_cast<uint32_t>(model) };
    EmitInstruction(mMemoryModel, SpvOpMemoryModel, instr);
}

void SpirvBuilder::SetEntryPoint(SpvExecutionModel model, uint32_t entryPointId, std::string_view name, std::span<const uint32_t> interfaces) {
    std::vector<uint32_t> ops;
    ops.push_back(static_cast<uint32_t>(model));
    ops.push_back(entryPointId);
    AppendString(ops, name);
    ops.insert(ops.end(), interfaces.begin(), interfaces.end());
    EmitInstruction(mEntryPoints, SpvOpEntryPoint, ops);
}

void SpirvBuilder::AddExecutionMode(uint32_t entryPointId, SpvExecutionMode mode) {
    uint32_t ops[] = { entryPointId, static_cast<uint32_t>(mode) };
    EmitInstruction(mExecutionModes, SpvOpExecutionMode, ops);
}

void SpirvBuilder::AddName(uint32_t targetId, std::string_view name) {
    std::vector<uint32_t> ops;
    ops.push_back(targetId);
    AppendString(ops, name);
    EmitInstruction(mDebugNames, SpvOpName, ops);
}

void SpirvBuilder::AddDecorate(uint32_t targetId, SpvDecoration decoration, std::span<const uint32_t> operands) {
    std::vector<uint32_t> ops;
    ops.push_back(targetId);
    ops.push_back(static_cast<uint32_t>(decoration));
    ops.insert(ops.end(), operands.begin(), operands.end());
    EmitInstruction(mAnnotations, SpvOpDecorate, ops);
}

void SpirvBuilder::AddMemberDecorate(uint32_t structId, uint32_t memberIndex, SpvDecoration decoration, std::span<const uint32_t> operands) {
    std::vector<uint32_t> ops;
    ops.push_back(structId);
    ops.push_back(memberIndex);
    ops.push_back(static_cast<uint32_t>(decoration));
    ops.insert(ops.end(), operands.begin(), operands.end());
    EmitInstruction(mAnnotations, SpvOpMemberDecorate, ops);
}

uint32_t SpirvBuilder::TypeVoid() {
    if (mTypeVoid == 0) {
        mTypeVoid = AllocId();
        uint32_t ops[] = { mTypeVoid };
        EmitInstruction(mTypesConstantsGlobals, SpvOpTypeVoid, ops);
    }
    return mTypeVoid;
}

uint32_t SpirvBuilder::TypeBool() {
    if (mTypeBool == 0) {
        mTypeBool = AllocId();
        uint32_t ops[] = { mTypeBool };
        EmitInstruction(mTypesConstantsGlobals, SpvOpTypeBool, ops);
    }
    return mTypeBool;
}

uint32_t SpirvBuilder::TypeInt(uint32_t width, bool isSigned) {
    if (width == 32 && isSigned && mTypeInt32 != 0) return mTypeInt32;
    if (width == 32 && !isSigned && mTypeUInt32 != 0) return mTypeUInt32;

    uint32_t id = AllocId();
    uint32_t ops[] = { id, width, isSigned ? 1U : 0U };
    EmitInstruction(mTypesConstantsGlobals, SpvOpTypeInt, ops);
    if (width == 32) {
        if (isSigned) mTypeInt32 = id;
        else mTypeUInt32 = id;
    }
    return id;
}

uint32_t SpirvBuilder::TypeFloat(uint32_t width) {
    if (width == 32 && mTypeFloat32 != 0) return mTypeFloat32;
    uint32_t id = AllocId();
    uint32_t ops[] = { id, width };
    EmitInstruction(mTypesConstantsGlobals, SpvOpTypeFloat, ops);
    if (width == 32) mTypeFloat32 = id;
    return id;
}

uint32_t SpirvBuilder::TypeVector(uint32_t componentType, uint32_t count) {
    uint64_t key = (static_cast<uint64_t>(componentType) << 32) | count;
    auto it = mVectorTypeCache.find(key);
    if (it != mVectorTypeCache.end()) {
        return it->second;
    }
    uint32_t id = AllocId();
    uint32_t ops[] = { id, componentType, count };
    EmitInstruction(mTypesConstantsGlobals, SpvOpTypeVector, ops);
    mVectorTypeCache[key] = id;
    return id;
}

uint32_t SpirvBuilder::TypeArray(uint32_t elementType, uint32_t lengthId) {
    uint32_t id = AllocId();
    uint32_t ops[] = { id, elementType, lengthId };
    EmitInstruction(mTypesConstantsGlobals, SpvOpTypeArray, ops);
    return id;
}

uint32_t SpirvBuilder::TypeRuntimeArray(uint32_t elementType) {
    uint32_t id = AllocId();
    uint32_t ops[] = { id, elementType };
    EmitInstruction(mTypesConstantsGlobals, SpvOpTypeRuntimeArray, ops);
    return id;
}

uint32_t SpirvBuilder::TypeStruct(std::span<const uint32_t> memberTypes) {
    uint32_t id = AllocId();
    std::vector<uint32_t> ops;
    ops.push_back(id);
    ops.insert(ops.end(), memberTypes.begin(), memberTypes.end());
    EmitInstruction(mTypesConstantsGlobals, SpvOpTypeStruct, ops);
    return id;
}

uint32_t SpirvBuilder::TypePointer(SpvStorageClass storageClass, uint32_t type) {
    uint64_t key = (static_cast<uint64_t>(storageClass) << 32) | type;
    auto it = mPointerTypeCache.find(key);
    if (it != mPointerTypeCache.end()) {
        return it->second;
    }
    uint32_t id = AllocId();
    uint32_t ops[] = { id, static_cast<uint32_t>(storageClass), type };
    EmitInstruction(mTypesConstantsGlobals, SpvOpTypePointer, ops);
    mPointerTypeCache[key] = id;
    return id;
}

uint32_t SpirvBuilder::TypeImage2D(uint32_t sampledType) {
    uint32_t id = AllocId();
    uint32_t ops[] = { id, sampledType, 1, 0, 0, 0, 1, 0 };
    EmitInstruction(mTypesConstantsGlobals, SpvOpTypeImage, ops);
    return id;
}

uint32_t SpirvBuilder::TypeSampledImage(uint32_t imageType) {
    uint32_t id = AllocId();
    uint32_t ops[] = { id, imageType };
    EmitInstruction(mTypesConstantsGlobals, SpvOpTypeSampledImage, ops);
    return id;
}

uint32_t SpirvBuilder::TypeFunction(uint32_t returnType, std::span<const uint32_t> paramTypes) {
    uint32_t id = AllocId();
    std::vector<uint32_t> ops;
    ops.push_back(id);
    ops.push_back(returnType);
    ops.insert(ops.end(), paramTypes.begin(), paramTypes.end());
    EmitInstruction(mTypesConstantsGlobals, SpvOpTypeFunction, ops);
    return id;
}

uint32_t SpirvBuilder::ConstantBool(bool value) {
    uint32_t id = AllocId();
    uint32_t typeId = TypeBool();
    uint32_t ops[] = { typeId, id };
    EmitInstruction(mTypesConstantsGlobals, value ? SpvOpConstantTrue : SpvOpConstantFalse, ops);
    return id;
}

uint32_t SpirvBuilder::ConstantFloat(float value) {
    uint32_t raw;
    std::memcpy(&raw, &value, sizeof(raw));
    auto it = mConstantFloatCache.find(raw);
    if (it != mConstantFloatCache.end()) return it->second;

    uint32_t id = AllocId();
    uint32_t typeId = TypeFloat(32);
    uint32_t ops[] = { typeId, id, raw };
    EmitInstruction(mTypesConstantsGlobals, SpvOpConstant, ops);
    mConstantFloatCache[raw] = id;
    return id;
}

uint32_t SpirvBuilder::ConstantInt(int32_t value) {
    auto it = mConstantIntCache.find(value);
    if (it != mConstantIntCache.end()) return it->second;

    uint32_t id = AllocId();
    uint32_t typeId = TypeInt(32, true);
    uint32_t raw = static_cast<uint32_t>(value);
    uint32_t ops[] = { typeId, id, raw };
    EmitInstruction(mTypesConstantsGlobals, SpvOpConstant, ops);
    mConstantIntCache[value] = id;
    return id;
}

uint32_t SpirvBuilder::ConstantUInt(uint32_t value) {
    auto it = mConstantUIntCache.find(value);
    if (it != mConstantUIntCache.end()) return it->second;

    uint32_t id = AllocId();
    uint32_t typeId = TypeInt(32, false);
    uint32_t ops[] = { typeId, id, value };
    EmitInstruction(mTypesConstantsGlobals, SpvOpConstant, ops);
    mConstantUIntCache[value] = id;
    return id;
}

uint32_t SpirvBuilder::ConstantComposite(uint32_t type, std::span<const uint32_t> constituents) {
    uint32_t id = AllocId();
    std::vector<uint32_t> ops;
    ops.push_back(type);
    ops.push_back(id);
    ops.insert(ops.end(), constituents.begin(), constituents.end());
    EmitInstruction(mTypesConstantsGlobals, SpvOpConstantComposite, ops);
    return id;
}

uint32_t SpirvBuilder::AddVariable(uint32_t pointerType, SpvStorageClass storageClass, uint32_t initializer) {
    uint32_t id = AllocId();
    std::vector<uint32_t> ops;
    ops.push_back(pointerType);
    ops.push_back(id);
    ops.push_back(static_cast<uint32_t>(storageClass));
    if (initializer != 0) {
        ops.push_back(initializer);
    }
    if (storageClass == SpvStorageClassFunction) {
        EmitInstruction(mFunctionDefinitions, SpvOpVariable, ops);
    } else {
        EmitInstruction(mTypesConstantsGlobals, SpvOpVariable, ops);
    }
    return id;
}

uint32_t SpirvBuilder::AddLocalVariable(uint32_t pointerType, uint32_t initializer) {
    return AddVariable(pointerType, SpvStorageClassFunction, initializer);
}

uint32_t SpirvBuilder::BeginFunction(uint32_t returnType, uint32_t funcType, SpvFunctionControl control, uint32_t funcId) {
    uint32_t id = (funcId != 0) ? funcId : AllocId();
    uint32_t ops[] = { returnType, id, static_cast<uint32_t>(control), funcType };
    EmitInstruction(mFunctionDefinitions, SpvOpFunction, ops);
    return id;
}

void SpirvBuilder::EndFunction() {
    uint32_t empty[] = { 0 };
    EmitInstruction(mFunctionDefinitions, SpvOpFunctionEnd, {});
}

uint32_t SpirvBuilder::CreateLabel() {
    return AllocId();
}

void SpirvBuilder::SetCurrentBlock(uint32_t labelId) {
    uint32_t ops[] = { labelId };
    EmitInstruction(mFunctionDefinitions, SpvOpLabel, ops);
}

uint32_t SpirvBuilder::OpLoad(uint32_t resultType, uint32_t pointerId) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, pointerId };
    EmitInstruction(mFunctionDefinitions, SpvOpLoad, ops);
    return id;
}

void SpirvBuilder::OpStore(uint32_t pointerId, uint32_t objectId) {
    uint32_t ops[] = { pointerId, objectId };
    EmitInstruction(mFunctionDefinitions, SpvOpStore, ops);
}

uint32_t SpirvBuilder::OpAccessChain(uint32_t resultType, uint32_t baseId, std::span<const uint32_t> indexes) {
    uint32_t id = AllocId();
    std::vector<uint32_t> ops;
    ops.push_back(resultType);
    ops.push_back(id);
    ops.push_back(baseId);
    ops.insert(ops.end(), indexes.begin(), indexes.end());
    EmitInstruction(mFunctionDefinitions, SpvOpAccessChain, ops);
    return id;
}

uint32_t SpirvBuilder::OpVectorShuffle(uint32_t resultType, uint32_t vec1, uint32_t vec2, std::span<const uint32_t> components) {
    uint32_t id = AllocId();
    std::vector<uint32_t> ops;
    ops.push_back(resultType);
    ops.push_back(id);
    ops.push_back(vec1);
    ops.push_back(vec2);
    ops.insert(ops.end(), components.begin(), components.end());
    EmitInstruction(mFunctionDefinitions, SpvOpVectorShuffle, ops);
    return id;
}

uint32_t SpirvBuilder::OpCompositeConstruct(uint32_t resultType, std::span<const uint32_t> constituents) {
    uint32_t id = AllocId();
    std::vector<uint32_t> ops;
    ops.push_back(resultType);
    ops.push_back(id);
    ops.insert(ops.end(), constituents.begin(), constituents.end());
    EmitInstruction(mFunctionDefinitions, SpvOpCompositeConstruct, ops);
    return id;
}

uint32_t SpirvBuilder::OpCompositeExtract(uint32_t resultType, uint32_t compositeId, std::span<const uint32_t> indexes) {
    uint32_t id = AllocId();
    std::vector<uint32_t> ops;
    ops.push_back(resultType);
    ops.push_back(id);
    ops.push_back(compositeId);
    ops.insert(ops.end(), indexes.begin(), indexes.end());
    EmitInstruction(mFunctionDefinitions, SpvOpCompositeExtract, ops);
    return id;
}

uint32_t SpirvBuilder::OpImageSampleImplicitLod(uint32_t resultType, uint32_t sampledImageId, uint32_t coordinateId) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, sampledImageId, coordinateId };
    EmitInstruction(mFunctionDefinitions, SpvOpImageSampleImplicitLod, ops);
    return id;
}

uint32_t SpirvBuilder::OpFNegate(uint32_t resultType, uint32_t operandId) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, operandId };
    EmitInstruction(mFunctionDefinitions, SpvOpFNegate, ops);
    return id;
}

uint32_t SpirvBuilder::OpIAdd(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpIAdd, ops);
    return id;
}

uint32_t SpirvBuilder::OpFAdd(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpFAdd, ops);
    return id;
}

uint32_t SpirvBuilder::OpFSub(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpFSub, ops);
    return id;
}

uint32_t SpirvBuilder::OpFMul(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpFMul, ops);
    return id;
}

uint32_t SpirvBuilder::OpFDiv(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpFDiv, ops);
    return id;
}

uint32_t SpirvBuilder::OpDot(uint32_t resultType, uint32_t vec1, uint32_t vec2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, vec1, vec2 };
    EmitInstruction(mFunctionDefinitions, SpvOpDot, ops);
    return id;
}

uint32_t SpirvBuilder::OpExtInst(uint32_t resultType, uint32_t set, uint32_t instruction, std::span<const uint32_t> operands) {
    uint32_t id = AllocId();
    std::vector<uint32_t> ops;
    ops.push_back(resultType);
    ops.push_back(id);
    ops.push_back(set);
    ops.push_back(instruction);
    ops.insert(ops.end(), operands.begin(), operands.end());
    EmitInstruction(mFunctionDefinitions, SpvOpExtInst, ops);
    return id;
}

uint32_t SpirvBuilder::OpFOrdLessThan(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpFOrdLessThan, ops);
    return id;
}

uint32_t SpirvBuilder::OpFOrdLessThanEqual(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpFOrdLessThanEqual, ops);
    return id;
}

uint32_t SpirvBuilder::OpFOrdGreaterThan(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpFOrdGreaterThan, ops);
    return id;
}

uint32_t SpirvBuilder::OpFOrdGreaterThanEqual(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpFOrdGreaterThanEqual, ops);
    return id;
}

uint32_t SpirvBuilder::OpFOrdEqual(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpFOrdEqual, ops);
    return id;
}

uint32_t SpirvBuilder::OpFOrdNotEqual(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpFOrdNotEqual, ops);
    return id;
}

uint32_t SpirvBuilder::OpINotEqual(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpINotEqual, ops);
    return id;
}

uint32_t SpirvBuilder::OpLogicalAnd(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpLogicalAnd, ops);
    return id;
}

uint32_t SpirvBuilder::OpLogicalOr(uint32_t resultType, uint32_t op1, uint32_t op2) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op1, op2 };
    EmitInstruction(mFunctionDefinitions, SpvOpLogicalOr, ops);
    return id;
}

uint32_t SpirvBuilder::OpLogicalNot(uint32_t resultType, uint32_t op) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, op };
    EmitInstruction(mFunctionDefinitions, SpvOpLogicalNot, ops);
    return id;
}

uint32_t SpirvBuilder::OpSelect(uint32_t resultType, uint32_t condition, uint32_t trueVal, uint32_t falseVal) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, condition, trueVal, falseVal };
    EmitInstruction(mFunctionDefinitions, SpvOpSelect, ops);
    return id;
}

uint32_t SpirvBuilder::OpConvertFToS(uint32_t resultType, uint32_t floatVal) {
    uint32_t id = AllocId();
    uint32_t ops[] = { resultType, id, floatVal };
    EmitInstruction(mFunctionDefinitions, SpvOpConvertFToS, ops);
    return id;
}

void SpirvBuilder::OpBranch(uint32_t targetLabel) {
    uint32_t ops[] = { targetLabel };
    EmitInstruction(mFunctionDefinitions, SpvOpBranch, ops);
}

void SpirvBuilder::OpBranchConditional(uint32_t condition, uint32_t trueLabel, uint32_t falseLabel) {
    uint32_t ops[] = { condition, trueLabel, falseLabel };
    EmitInstruction(mFunctionDefinitions, SpvOpBranchConditional, ops);
}

void SpirvBuilder::OpSelectionMerge(uint32_t mergeBlock, uint32_t selectionControl) {
    uint32_t ops[] = { mergeBlock, selectionControl };
    EmitInstruction(mFunctionDefinitions, SpvOpSelectionMerge, ops);
}

void SpirvBuilder::OpLoopMerge(uint32_t mergeBlock, uint32_t continueTarget, uint32_t loopControl) {
    uint32_t ops[] = { mergeBlock, continueTarget, loopControl };
    EmitInstruction(mFunctionDefinitions, SpvOpLoopMerge, ops);
}

void SpirvBuilder::OpReturn() {
    EmitInstruction(mFunctionDefinitions, SpvOpReturn, {});
}

void SpirvBuilder::OpReturnValue(uint32_t valueId) {
    uint32_t ops[] = { valueId };
    EmitInstruction(mFunctionDefinitions, SpvOpReturnValue, ops);
}

void SpirvBuilder::OpKill() {
    EmitInstruction(mFunctionDefinitions, SpvOpKill, {});
}

std::vector<uint32_t> SpirvBuilder::Build() {
    std::vector<uint32_t> result;

    // Estimate size to avoid reallocations
    size_t totalWords = 5 + mCapabilities.size() + mExtensions.size() + mExtInstImports.size() +
                        mMemoryModel.size() + mEntryPoints.size() + mExecutionModes.size() +
                        mDebugNames.size() + mAnnotations.size() + mTypesConstantsGlobals.size() +
                        mFunctionDefinitions.size();
    result.reserve(totalWords);

    // SPIR-V Header (5 words)
    result.push_back(kSpirvMagicNumber);
    result.push_back(mVersion);
    result.push_back(kPicaToSpirvGeneratorId);
    result.push_back(mNextId); // Bound
    result.push_back(0);        // Reserved Schema

    // Sections in strict order mandated by Section 2.4 of the SPIR-V Specification:
    result.insert(result.end(), mCapabilities.begin(), mCapabilities.end());
    result.insert(result.end(), mExtensions.begin(), mExtensions.end());
    result.insert(result.end(), mExtInstImports.begin(), mExtInstImports.end());
    result.insert(result.end(), mMemoryModel.begin(), mMemoryModel.end());
    result.insert(result.end(), mEntryPoints.begin(), mEntryPoints.end());
    result.insert(result.end(), mExecutionModes.begin(), mExecutionModes.end());
    result.insert(result.end(), mDebugNames.begin(), mDebugNames.end());
    result.insert(result.end(), mAnnotations.begin(), mAnnotations.end());
    result.insert(result.end(), mTypesConstantsGlobals.begin(), mTypesConstantsGlobals.end());
    result.insert(result.end(), mFunctionDefinitions.begin(), mFunctionDefinitions.end());

    return result;
}

} // namespace PicaToSpirv
