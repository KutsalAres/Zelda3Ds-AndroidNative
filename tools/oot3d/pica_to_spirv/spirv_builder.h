#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <span>

namespace PicaToSpirv {

// Standard SPIR-V Definitions (Khronos SPIR-V Specification)
inline constexpr uint32_t kSpirvMagicNumber = 0x07230203U;
inline constexpr uint32_t kSpirvVersion1_0 = 0x00010000U;
inline constexpr uint32_t kSpirvVersion1_3 = 0x00010300U;
inline constexpr uint32_t kPicaToSpirvGeneratorId = 0x001B0001U;

enum SpvOp : uint32_t {
    SpvOpNop = 0,
    SpvOpSource = 3,
    SpvOpName = 5,
    SpvOpMemberName = 6,
    SpvOpExtension = 10,
    SpvOpExtInstImport = 11,
    SpvOpExtInst = 12,
    SpvOpMemoryModel = 14,
    SpvOpEntryPoint = 15,
    SpvOpExecutionMode = 16,
    SpvOpCapability = 17,
    SpvOpTypeVoid = 19,
    SpvOpTypeBool = 20,
    SpvOpTypeInt = 21,
    SpvOpTypeFloat = 22,
    SpvOpTypeVector = 23,
    SpvOpTypeMatrix = 24,
    SpvOpTypeImage = 25,
    SpvOpTypeSampler = 26,
    SpvOpTypeSampledImage = 27,
    SpvOpTypeArray = 28,
    SpvOpTypeRuntimeArray = 29,
    SpvOpTypeStruct = 30,
    SpvOpTypePointer = 32,
    SpvOpTypeFunction = 33,
    SpvOpConstantTrue = 41,
    SpvOpConstantFalse = 42,
    SpvOpConstant = 43,
    SpvOpConstantComposite = 44,
    SpvOpFunction = 54,
    SpvOpFunctionParameter = 55,
    SpvOpFunctionEnd = 56,
    SpvOpVariable = 59,
    SpvOpLoad = 61,
    SpvOpStore = 62,
    SpvOpAccessChain = 65,
    SpvOpDecorate = 71,
    SpvOpMemberDecorate = 72,
    SpvOpVectorShuffle = 79,
    SpvOpCompositeConstruct = 80,
    SpvOpCompositeExtract = 81,
    SpvOpCompositeInsert = 82,
    SpvOpCopyObject = 83,
    SpvOpImageSampleImplicitLod = 87,
    SpvOpConvertFToS = 110,
    SpvOpConvertSToF = 111,
    SpvOpFNegate = 127,
    SpvOpIAdd = 128,
    SpvOpFAdd = 129,
    SpvOpFSub = 131,
    SpvOpFMul = 133,
    SpvOpUDiv = 134,
    SpvOpSDiv = 135,
    SpvOpFDiv = 136,
    SpvOpDot = 148,
    SpvOpLogicalEqual = 164,
    SpvOpLogicalNotEqual = 165,
    SpvOpLogicalOr = 166,
    SpvOpLogicalAnd = 167,
    SpvOpLogicalNot = 168,
    SpvOpSelect = 169,
    SpvOpIEqual = 170,
    SpvOpINotEqual = 171,
    SpvOpFOrdEqual = 180,
    SpvOpFOrdNotEqual = 182,
    SpvOpFOrdLessThan = 184,
    SpvOpFOrdGreaterThan = 186,
    SpvOpFOrdLessThanEqual = 188,
    SpvOpFOrdGreaterThanEqual = 190,
    SpvOpLoopMerge = 246,
    SpvOpSelectionMerge = 247,
    SpvOpLabel = 248,
    SpvOpBranch = 249,
    SpvOpBranchConditional = 250,
    SpvOpSwitch = 251,
    SpvOpKill = 252,
    SpvOpReturn = 253,
    SpvOpReturnValue = 254,
    SpvOpUnreachable = 255
};

enum SpvDecoration : uint32_t {
    SpvDecorationRelaxedPrecision = 0,
    SpvDecorationBlock = 2,
    SpvDecorationBufferBlock = 3,
    SpvDecorationColMajor = 4,
    SpvDecorationRowMajor = 5,
    SpvDecorationGLSLShared = 6,
    SpvDecorationGLSLPacked = 7,
    SpvDecorationArrayStride = 6,
    SpvDecorationMatrixStride = 7,
    SpvDecorationBuiltIn = 11,
    SpvDecorationLocation = 30,
    SpvDecorationBinding = 33,
    SpvDecorationDescriptorSet = 34,
    SpvDecorationOffset = 35
};

enum SpvBuiltIn : uint32_t {
    SpvBuiltInPosition = 0,
    SpvBuiltInPointSize = 1,
    SpvBuiltInClipDistance = 3,
    SpvBuiltInCullDistance = 4,
    SpvBuiltInVertexId = 5,
    SpvBuiltInInstanceId = 6,
    SpvBuiltInFragCoord = 15,
    SpvBuiltInPointCoord = 16,
    SpvBuiltInFrontFacing = 17,
    SpvBuiltInFragDepth = 22
};

enum SpvStorageClass : uint32_t {
    SpvStorageClassUniformConstant = 0,
    SpvStorageClassInput = 1,
    SpvStorageClassUniform = 2,
    SpvStorageClassOutput = 3,
    SpvStorageClassFunction = 7,
    SpvStorageClassPushConstant = 9
};

enum SpvCapability : uint32_t {
    SpvCapabilityMatrix = 0,
    SpvCapabilityShader = 1,
    SpvCapabilityGeometry = 2
};

enum SpvExecutionModel : uint32_t {
    SpvExecutionModelVertex = 0,
    SpvExecutionModelFragment = 4,
    SpvExecutionModelGLCompute = 5
};

enum SpvExecutionMode : uint32_t {
    SpvExecutionModeOriginUpperLeft = 7
};

enum SpvMemoryModel : uint32_t {
    SpvMemoryModelSimple = 0,
    SpvMemoryModelGLSL450 = 1
};

enum SpvFunctionControl : uint32_t {
    SpvFunctionControlNone = 0,
    SpvFunctionControlInline = 1,
    SpvFunctionControlDontInline = 2,
    SpvFunctionControlPure = 4,
    SpvFunctionControlConst = 8
};

enum GLSLstd450 : uint32_t {
    GLSLstd450Round = 1,
    GLSLstd450FAbs = 4,
    GLSLstd450Floor = 8,
    GLSLstd450Ceil = 9,
    GLSLstd450Fract = 10,
    GLSLstd450Exp2 = 29,
    GLSLstd450Log2 = 30,
    GLSLstd450Sqrt = 31,
    GLSLstd450InverseSqrt = 32,
    GLSLstd450FMin = 37,
    GLSLstd450FMax = 40,
    GLSLstd450FClamp = 43,
    GLSLstd450FMix = 46,
    GLSLstd450Fma = 50,
    GLSLstd450Normalize = 69
};

/**
 * Lightweight, direct SPIR-V binary builder.
 * Emits SPIR-V modules without external dependencies or text parsing.
 */
class SpirvBuilder final {
public:
    explicit SpirvBuilder(uint32_t version = kSpirvVersion1_0);

    // ID allocation
    uint32_t AllocId();

    // Headers & Setup
    void AddCapability(SpvCapability cap);
    uint32_t ImportExtInst(std::string_view name);
    void SetMemoryModel(SpvMemoryModel model);
    void SetEntryPoint(SpvExecutionModel model, uint32_t entryPointId, std::string_view name, std::span<const uint32_t> interfaces);
    void SetEntryPoint(SpvExecutionModel model, uint32_t entryPointId, std::string_view name, std::initializer_list<uint32_t> interfaces) {
        SetEntryPoint(model, entryPointId, name, std::span<const uint32_t>(interfaces.begin(), interfaces.size()));
    }
    void AddExecutionMode(uint32_t entryPointId, SpvExecutionMode mode);

    // Annotations & Names
    void AddName(uint32_t targetId, std::string_view name);
    void AddDecorate(uint32_t targetId, SpvDecoration decoration, std::span<const uint32_t> operands = {});
    void AddDecorate(uint32_t targetId, SpvDecoration decoration, std::initializer_list<uint32_t> operands) {
        AddDecorate(targetId, decoration, std::span<const uint32_t>(operands.begin(), operands.size()));
    }
    void AddMemberDecorate(uint32_t structId, uint32_t memberIndex, SpvDecoration decoration, std::span<const uint32_t> operands = {});
    void AddMemberDecorate(uint32_t structId, uint32_t memberIndex, SpvDecoration decoration, std::initializer_list<uint32_t> operands) {
        AddMemberDecorate(structId, memberIndex, decoration, std::span<const uint32_t>(operands.begin(), operands.size()));
    }

    // Types
    uint32_t TypeVoid();
    uint32_t TypeBool();
    uint32_t TypeInt(uint32_t width, bool isSigned);
    uint32_t TypeFloat(uint32_t width = 32);
    uint32_t TypeVector(uint32_t componentType, uint32_t count);
    uint32_t TypeArray(uint32_t elementType, uint32_t lengthId);
    uint32_t TypeRuntimeArray(uint32_t elementType);
    uint32_t TypeStruct(std::span<const uint32_t> memberTypes);
    uint32_t TypeStruct(std::initializer_list<uint32_t> memberTypes) {
        return TypeStruct(std::span<const uint32_t>(memberTypes.begin(), memberTypes.size()));
    }
    uint32_t TypePointer(SpvStorageClass storageClass, uint32_t type);
    uint32_t TypeImage2D(uint32_t sampledType);
    uint32_t TypeSampledImage(uint32_t imageType);
    uint32_t TypeFunction(uint32_t returnType, std::span<const uint32_t> paramTypes);
    uint32_t TypeFunction(uint32_t returnType, std::initializer_list<uint32_t> paramTypes) {
        return TypeFunction(returnType, std::span<const uint32_t>(paramTypes.begin(), paramTypes.size()));
    }

    // Constants
    uint32_t ConstantBool(bool value);
    uint32_t ConstantFloat(float value);
    uint32_t ConstantInt(int32_t value);
    uint32_t ConstantUInt(uint32_t value);
    uint32_t ConstantComposite(uint32_t type, std::span<const uint32_t> constituents);
    uint32_t ConstantComposite(uint32_t type, std::initializer_list<uint32_t> constituents) {
        return ConstantComposite(type, std::span<const uint32_t>(constituents.begin(), constituents.size()));
    }

    // Variables
    uint32_t AddVariable(uint32_t pointerType, SpvStorageClass storageClass, uint32_t initializer = 0);
    uint32_t AddLocalVariable(uint32_t pointerType, uint32_t initializer = 0);

    // Functions and Basic Blocks
    uint32_t BeginFunction(uint32_t returnType, uint32_t funcType, SpvFunctionControl control = SpvFunctionControlNone, uint32_t funcId = 0);
    void EndFunction();
    uint32_t CreateLabel();
    void SetCurrentBlock(uint32_t labelId);

    // Instructions
    uint32_t OpLoad(uint32_t resultType, uint32_t pointerId);
    void OpStore(uint32_t pointerId, uint32_t objectId);
    uint32_t OpAccessChain(uint32_t resultType, uint32_t baseId, std::span<const uint32_t> indexes);
    uint32_t OpAccessChain(uint32_t resultType, uint32_t baseId, std::initializer_list<uint32_t> indexes) {
        return OpAccessChain(resultType, baseId, std::span<const uint32_t>(indexes.begin(), indexes.size()));
    }
    uint32_t OpVectorShuffle(uint32_t resultType, uint32_t vec1, uint32_t vec2, std::span<const uint32_t> components);
    uint32_t OpVectorShuffle(uint32_t resultType, uint32_t vec1, uint32_t vec2, std::initializer_list<uint32_t> components) {
        return OpVectorShuffle(resultType, vec1, vec2, std::span<const uint32_t>(components.begin(), components.size()));
    }
    uint32_t OpCompositeConstruct(uint32_t resultType, std::span<const uint32_t> constituents);
    uint32_t OpCompositeConstruct(uint32_t resultType, std::initializer_list<uint32_t> constituents) {
        return OpCompositeConstruct(resultType, std::span<const uint32_t>(constituents.begin(), constituents.size()));
    }
    uint32_t OpCompositeExtract(uint32_t resultType, uint32_t compositeId, std::span<const uint32_t> indexes);
    uint32_t OpCompositeExtract(uint32_t resultType, uint32_t compositeId, std::initializer_list<uint32_t> indexes) {
        return OpCompositeExtract(resultType, compositeId, std::span<const uint32_t>(indexes.begin(), indexes.size()));
    }
    uint32_t OpImageSampleImplicitLod(uint32_t resultType, uint32_t sampledImageId, uint32_t coordinateId);

    // Arithmetic
    uint32_t OpFNegate(uint32_t resultType, uint32_t operandId);
    uint32_t OpIAdd(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpFAdd(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpFSub(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpFMul(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpFDiv(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpDot(uint32_t resultType, uint32_t vec1, uint32_t vec2);
    uint32_t OpExtInst(uint32_t resultType, uint32_t set, uint32_t instruction, std::span<const uint32_t> operands);
    uint32_t OpExtInst(uint32_t resultType, uint32_t set, uint32_t instruction, std::initializer_list<uint32_t> operands) {
        return OpExtInst(resultType, set, instruction, std::span<const uint32_t>(operands.begin(), operands.size()));
    }

    // Comparisons and Logic
    uint32_t OpFOrdLessThan(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpFOrdLessThanEqual(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpFOrdGreaterThan(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpFOrdGreaterThanEqual(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpFOrdEqual(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpFOrdNotEqual(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpINotEqual(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpLogicalAnd(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpLogicalOr(uint32_t resultType, uint32_t op1, uint32_t op2);
    uint32_t OpLogicalNot(uint32_t resultType, uint32_t op);
    uint32_t OpSelect(uint32_t resultType, uint32_t condition, uint32_t trueVal, uint32_t falseVal);
    uint32_t OpConvertFToS(uint32_t resultType, uint32_t floatVal);

    // Control Flow
    void OpBranch(uint32_t targetLabel);
    void OpBranchConditional(uint32_t condition, uint32_t trueLabel, uint32_t falseLabel);
    void OpSelectionMerge(uint32_t mergeBlock, uint32_t selectionControl = 0);
    void OpLoopMerge(uint32_t mergeBlock, uint32_t continueTarget, uint32_t loopControl = 0);
    void OpReturn();
    void OpReturnValue(uint32_t valueId);
    void OpKill();

    // Finalize and build complete binary SPIR-V stream
    std::vector<uint32_t> Build();

private:
    void AppendString(std::vector<uint32_t>& target, std::string_view str);

    uint32_t mVersion;
    uint32_t mNextId = 1;

    // SPIR-V Logical Sections (assembled in strict order on Build())
    std::vector<uint32_t> mCapabilities;
    std::vector<uint32_t> mExtensions;
    std::vector<uint32_t> mExtInstImports;
    std::vector<uint32_t> mMemoryModel;
    std::vector<uint32_t> mEntryPoints;
    std::vector<uint32_t> mExecutionModes;
    std::vector<uint32_t> mDebugNames;
    std::vector<uint32_t> mAnnotations;
    std::vector<uint32_t> mTypesConstantsGlobals;
    std::vector<uint32_t> mFunctionDefinitions;

    // Type caching for deduplication
    uint32_t mTypeVoid = 0;
    uint32_t mTypeBool = 0;
    uint32_t mTypeFloat32 = 0;
    uint32_t mTypeInt32 = 0;
    uint32_t mTypeUInt32 = 0;
    std::unordered_map<uint64_t, uint32_t> mVectorTypeCache;
    std::unordered_map<uint64_t, uint32_t> mPointerTypeCache;
    std::unordered_map<uint32_t, uint32_t> mConstantFloatCache;
    std::unordered_map<uint32_t, uint32_t> mConstantUIntCache;
    std::unordered_map<int32_t, uint32_t> mConstantIntCache;
};

} // namespace PicaToSpirv
