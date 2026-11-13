#include "pica_vertex_translator.h"

#include <array>
#include <sstream>

namespace PicaToSpirv {

using nihstro::DestRegister;
using nihstro::Instruction;
using nihstro::OpCode;
using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

namespace {

void SetError(std::string* errorLog, std::string_view message) {
    if (errorLog) {
        *errorLog = std::string(message);
    }
}

} // namespace

PicaVertexTranslator::PicaVertexTranslator(const PicaVertexShaderConfig& config)
    : mConfig(config) {
}

bool PicaVertexTranslator::Translate(
    std::span<const uint32_t> programCode,
    std::span<const uint32_t> swizzleData,
    std::vector<uint32_t>& outSpirv,
    std::string* errorLog) {

    if (programCode.empty()) {
        SetError(errorLog, "PICA program code is empty");
        return false;
    }

    if (mConfig.mainOffset >= programCode.size()) {
        SetError(errorLog, "PICA main offset is out of program bounds");
        return false;
    }

    SpirvBuilder b(mConfig.spirvVersion);

    // 1. Capabilities & Extended Instructions
    b.AddCapability(SpvCapabilityShader);
    uint32_t glslExtInst = b.ImportExtInst("GLSL.std.450");
    b.SetMemoryModel(SpvMemoryModelGLSL450);

    // 2. Fundamental Types
    uint32_t typeVoid = b.TypeVoid();
    uint32_t typeBool = b.TypeBool();
    uint32_t typeFloat = b.TypeFloat(32);
    uint32_t typeInt = b.TypeInt(32, true);
    uint32_t typeUInt = b.TypeInt(32, false);

    uint32_t typeVec2 = b.TypeVector(typeFloat, 2);
    uint32_t typeVec3 = b.TypeVector(typeFloat, 3);
    uint32_t typeVec4 = b.TypeVector(typeFloat, 4);
    uint32_t typeIVec2 = b.TypeVector(typeInt, 2);
    uint32_t typeUVec4 = b.TypeVector(typeUInt, 4);
    uint32_t typeBVec2 = b.TypeVector(typeBool, 2);
    uint32_t typeBVec4 = b.TypeVector(typeBool, 4);

    // Common Constants
    uint32_t cFloat0 = b.ConstantFloat(0.0f);
    uint32_t cFloat1 = b.ConstantFloat(1.0f);
    uint32_t cUInt0 = b.ConstantUInt(0);
    uint32_t cUInt1 = b.ConstantUInt(1);
    uint32_t cUInt2 = b.ConstantUInt(2);
    uint32_t cUInt3 = b.ConstantUInt(3);
    uint32_t cUInt4 = b.ConstantUInt(4);
    uint32_t cInt0 = b.ConstantInt(0);
    uint32_t cInt1 = b.ConstantInt(1);

    uint32_t cVec4Zero = b.ConstantComposite(typeVec4, {cFloat0, cFloat0, cFloat0, cFloat0});
    uint32_t cVec4One = b.ConstantComposite(typeVec4, {cFloat1, cFloat1, cFloat1, cFloat1});

    // Pointer Types
    uint32_t ptrInputVec4 = b.TypePointer(SpvStorageClassInput, typeVec4);
    uint32_t ptrOutputVec4 = b.TypePointer(SpvStorageClassOutput, typeVec4);
    uint32_t ptrOutputVec3 = b.TypePointer(SpvStorageClassOutput, typeVec3);
    uint32_t ptrOutputVec2 = b.TypePointer(SpvStorageClassOutput, typeVec2);
    uint32_t ptrOutputFloat = b.TypePointer(SpvStorageClassOutput, typeFloat);
    uint32_t ptrFunctionVec4 = b.TypePointer(SpvStorageClassFunction, typeVec4);
    uint32_t ptrFunctionIVec2 = b.TypePointer(SpvStorageClassFunction, typeIVec2);
    uint32_t ptrFunctionBVec2 = b.TypePointer(SpvStorageClassFunction, typeBVec2);

    // 3. gl_PerVertex Builtin Struct
    uint32_t cArrayLen1 = b.ConstantUInt(1);
    uint32_t typeFloatArray1 = b.TypeArray(typeFloat, cArrayLen1);
    uint32_t glPerVertexStruct = b.TypeStruct({typeVec4, typeFloat, typeFloatArray1, typeFloatArray1});
    b.AddDecorate(glPerVertexStruct, SpvDecorationBlock);
    b.AddMemberDecorate(glPerVertexStruct, 0, SpvDecorationBuiltIn, {SpvBuiltInPosition});
    b.AddMemberDecorate(glPerVertexStruct, 1, SpvDecorationBuiltIn, {SpvBuiltInPointSize});
    b.AddMemberDecorate(glPerVertexStruct, 2, SpvDecorationBuiltIn, {SpvBuiltInClipDistance});
    b.AddMemberDecorate(glPerVertexStruct, 3, SpvDecorationBuiltIn, {SpvBuiltInCullDistance});
    b.AddName(glPerVertexStruct, "gl_PerVertex");

    uint32_t ptrOutputPerVertex = b.TypePointer(SpvStorageClassOutput, glPerVertexStruct);
    uint32_t glPerVertexVar = b.AddVariable(ptrOutputPerVertex, SpvStorageClassOutput);
    b.AddName(glPerVertexVar, "");

    // 4. Output Varyings Interface
    // Location 0: pica_primary_color (vec4)
    uint32_t varColor = b.AddVariable(ptrOutputVec4, SpvStorageClassOutput);
    b.AddDecorate(varColor, SpvDecorationLocation, {0});
    b.AddName(varColor, "pica_primary_color");

    // Location 1: pica_texcoord0 (vec2)
    uint32_t varTex0 = b.AddVariable(ptrOutputVec2, SpvStorageClassOutput);
    b.AddDecorate(varTex0, SpvDecorationLocation, {1});
    b.AddName(varTex0, "pica_texcoord0");

    // Location 2: pica_texcoord1 (vec2)
    uint32_t varTex1 = b.AddVariable(ptrOutputVec2, SpvStorageClassOutput);
    b.AddDecorate(varTex1, SpvDecorationLocation, {2});
    b.AddName(varTex1, "pica_texcoord1");

    // Location 3: pica_texcoord2 (vec2)
    uint32_t varTex2 = b.AddVariable(ptrOutputVec2, SpvStorageClassOutput);
    b.AddDecorate(varTex2, SpvDecorationLocation, {3});
    b.AddName(varTex2, "pica_texcoord2");

    // Location 4: pica_texcoord0_w (float)
    uint32_t varTex0W = b.AddVariable(ptrOutputFloat, SpvStorageClassOutput);
    b.AddDecorate(varTex0W, SpvDecorationLocation, {4});
    b.AddName(varTex0W, "pica_texcoord0_w");

    // Location 5: pica_normquat (vec4)
    uint32_t varNormquat = b.AddVariable(ptrOutputVec4, SpvStorageClassOutput);
    b.AddDecorate(varNormquat, SpvDecorationLocation, {5});
    b.AddName(varNormquat, "pica_normquat");

    // Location 6: pica_view (vec3)
    uint32_t varView = b.AddVariable(ptrOutputVec3, SpvStorageClassOutput);
    b.AddDecorate(varView, SpvDecorationLocation, {6});
    b.AddName(varView, "pica_view");

    // 5. Input Attributes Interface (v0 - v15)
    std::array<uint32_t, 16> varInputs{};
    for (uint32_t i = 0; i < 16; ++i) {
        varInputs[i] = b.AddVariable(ptrInputVec4, SpvStorageClassInput);
        b.AddDecorate(varInputs[i], SpvDecorationLocation, {i});
        b.AddName(varInputs[i], "v" + std::to_string(i));
    }

    // 6. Uniform Buffer Layout (std140 PicaVertexUniforms)
    // Member 0: uint b (offset 0)
    // Member 1: int flip_viewport (offset 4)
    // Member 2: vec2 depth_scale_offset (offset 8)
    // Member 3: uvec4 i[4] (offset 16, stride 16)
    // Member 4: vec4 f[96] (offset 80, stride 16)
    uint32_t cArrayLen4 = b.ConstantUInt(4);
    uint32_t cArrayLen96 = b.ConstantUInt(96);
    uint32_t typeArrayIVec4 = b.TypeArray(typeUVec4, cArrayLen4);
    b.AddDecorate(typeArrayIVec4, SpvDecorationArrayStride, {16});

    uint32_t typeArrayFVec4 = b.TypeArray(typeVec4, cArrayLen96);
    b.AddDecorate(typeArrayFVec4, SpvDecorationArrayStride, {16});

    uint32_t uniformStruct = b.TypeStruct({typeUInt, typeInt, typeVec2, typeArrayIVec4, typeArrayFVec4});
    b.AddDecorate(uniformStruct, SpvDecorationBlock);
    b.AddMemberDecorate(uniformStruct, 0, SpvDecorationOffset, {0});
    b.AddMemberDecorate(uniformStruct, 1, SpvDecorationOffset, {4});
    b.AddMemberDecorate(uniformStruct, 2, SpvDecorationOffset, {8});
    b.AddMemberDecorate(uniformStruct, 3, SpvDecorationOffset, {16});
    b.AddMemberDecorate(uniformStruct, 4, SpvDecorationOffset, {80});
    b.AddName(uniformStruct, "PicaVertexUniforms");

    uint32_t ptrUniformStruct = b.TypePointer(SpvStorageClassUniform, uniformStruct);
    uint32_t ptrUniformVec4 = b.TypePointer(SpvStorageClassUniform, typeVec4);
    uint32_t ptrUniformVec2 = b.TypePointer(SpvStorageClassUniform, typeVec2);
    uint32_t ptrUniformInt = b.TypePointer(SpvStorageClassUniform, typeInt);

    uint32_t uniformVar = b.AddVariable(ptrUniformStruct, SpvStorageClassUniform);
    b.AddDecorate(uniformVar, SpvDecorationDescriptorSet, {mConfig.descriptorSet});
    b.AddDecorate(uniformVar, SpvDecorationBinding, {mConfig.uniformBinding});
    b.AddName(uniformVar, "uniforms");

    // 7. Entry Point Registration
    uint32_t entryPointId = b.AllocId();
    std::vector<uint32_t> interfaces;
    interfaces.reserve(24);
    interfaces.push_back(glPerVertexVar);
    interfaces.push_back(varColor);
    interfaces.push_back(varTex0);
    interfaces.push_back(varTex1);
    interfaces.push_back(varTex2);
    interfaces.push_back(varTex0W);
    interfaces.push_back(varNormquat);
    interfaces.push_back(varView);
    for (uint32_t inVar : varInputs) {
        interfaces.push_back(inVar);
    }
    b.SetEntryPoint(SpvExecutionModelVertex, entryPointId, "main", interfaces);

    // 8. Begin Function main()
    uint32_t typeFunc = b.TypeFunction(typeVoid, {});
    b.BeginFunction(typeVoid, typeFunc, SpvFunctionControlNone, entryPointId);
    uint32_t labelEntry = b.CreateLabel();
    b.SetCurrentBlock(labelEntry);

    // Allocate Local Registers (Function Scope)
    // Temporary registers r0 - r15
    std::array<uint32_t, 16> regTmp{};
    for (uint32_t i = 0; i < 16; ++i) {
        regTmp[i] = b.AddLocalVariable(ptrFunctionVec4);
        b.AddName(regTmp[i], "r" + std::to_string(i));
    }

    // Output staging registers o0 - o15
    std::array<uint32_t, 16> regOut{};
    for (uint32_t i = 0; i < 16; ++i) {
        regOut[i] = b.AddLocalVariable(ptrFunctionVec4);
        b.AddName(regOut[i], "o" + std::to_string(i));
    }

    // Address register a0 (ivec2)
    uint32_t regAddr = b.AddLocalVariable(ptrFunctionIVec2);
    b.AddName(regAddr, "a0");

    // Condition Code register cc (bvec2)
    uint32_t regCc = b.AddLocalVariable(ptrFunctionBVec2);
    b.AddName(regCc, "cc");

    // Initialize staging output registers to vec4(0.0)
    for (uint32_t i = 0; i < 16; ++i) {
        b.OpStore(regOut[i], cVec4Zero);
    }

    // Helper: Load Source Register
    auto loadSourceRegister = [&](const SourceRegister& srcReg, uint32_t addrRegIdx) -> uint32_t {
        uint32_t index = static_cast<uint32_t>(srcReg.GetIndex());
        switch (srcReg.GetRegisterType()) {
        case RegisterType::Input: {
            if (index < 16) {
                return b.OpLoad(typeVec4, varInputs[index]);
            }
            return cVec4Zero;
        }
        case RegisterType::Temporary: {
            if (index < 16) {
                return b.OpLoad(typeVec4, regTmp[index]);
            }
            return cVec4Zero;
        }
        case RegisterType::FloatUniform: {
            uint32_t finalIdx = 0;
            if (addrRegIdx == 0) {
                finalIdx = b.ConstantUInt(index);
            } else {
                uint32_t compIdx = (addrRegIdx == 1) ? 0 : 1;
                uint32_t loadedAddr = b.OpLoad(typeIVec2, regAddr);
                uint32_t offsetInt = b.OpCompositeExtract(typeInt, loadedAddr, {compIdx});
                uint32_t baseInt = b.ConstantInt(static_cast<int32_t>(index));
                finalIdx = b.OpIAdd(typeInt, baseInt, offsetInt);
            }
            uint32_t member4 = cUInt4;
            uint32_t uniformPtr = b.OpAccessChain(ptrUniformVec4, uniformVar, {member4, finalIdx});
            return b.OpLoad(typeVec4, uniformPtr);
        }
        default:
            return cVec4Zero;
        }
    };

    // Helper: Apply Swizzle and Negation to Source Vector
    auto applySwizzle = [&](uint32_t srcVec, const SwizzlePattern& swizzle, int srcNum) -> uint32_t {
        uint32_t sel[4];
        bool negate = false;
        if (srcNum == 1) {
            sel[0] = static_cast<uint32_t>(swizzle.GetSelectorSrc1(0));
            sel[1] = static_cast<uint32_t>(swizzle.GetSelectorSrc1(1));
            sel[2] = static_cast<uint32_t>(swizzle.GetSelectorSrc1(2));
            sel[3] = static_cast<uint32_t>(swizzle.GetSelectorSrc1(3));
            negate = bool(swizzle.negate_src1);
        } else if (srcNum == 2) {
            sel[0] = static_cast<uint32_t>(swizzle.GetSelectorSrc2(0));
            sel[1] = static_cast<uint32_t>(swizzle.GetSelectorSrc2(1));
            sel[2] = static_cast<uint32_t>(swizzle.GetSelectorSrc2(2));
            sel[3] = static_cast<uint32_t>(swizzle.GetSelectorSrc2(3));
            negate = bool(swizzle.negate_src2);
        } else {
            sel[0] = static_cast<uint32_t>(swizzle.GetSelectorSrc3(0));
            sel[1] = static_cast<uint32_t>(swizzle.GetSelectorSrc3(1));
            sel[2] = static_cast<uint32_t>(swizzle.GetSelectorSrc3(2));
            sel[3] = static_cast<uint32_t>(swizzle.GetSelectorSrc3(3));
            negate = bool(swizzle.negate_src3);
        }

        uint32_t swizzled = srcVec;
        if (sel[0] != 0 || sel[1] != 1 || sel[2] != 2 || sel[3] != 3) {
            swizzled = b.OpVectorShuffle(typeVec4, srcVec, srcVec, {sel[0], sel[1], sel[2], sel[3]});
        }
        if (negate) {
            swizzled = b.OpFNegate(typeVec4, swizzled);
        }
        return swizzled;
    };

    // Helper: Write Destination with Component Mask
    auto writeDestination = [&](const DestRegister& destReg, const SwizzlePattern& swizzle, uint32_t valueVec) {
        uint32_t destIdx = static_cast<uint32_t>(destReg.GetIndex());
        uint32_t destPtr = 0;
        if (destReg.GetRegisterType() == RegisterType::Output) {
            if (destIdx < 16) destPtr = regOut[destIdx];
        } else {
            if (destIdx < 16) destPtr = regTmp[destIdx];
        }

        if (destPtr == 0) return;

        uint32_t mask = swizzle.dest_mask;
        if (mask == 0) return;

        if (mask == 0xFU) {
            b.OpStore(destPtr, valueVec);
        } else {
            uint32_t oldVal = b.OpLoad(typeVec4, destPtr);
            uint32_t shuffleIndices[4];
            for (uint32_t c = 0; c < 4; ++c) {
                // Enabled component takes from valueVec (0..3), disabled from oldVal (4..7)
                shuffleIndices[c] = swizzle.DestComponentEnabled(c) ? c : (4 + c);
            }
            uint32_t merged = b.OpVectorShuffle(typeVec4, valueVec, oldVal, {shuffleIndices[0], shuffleIndices[1], shuffleIndices[2], shuffleIndices[3]});
            b.OpStore(destPtr, merged);
        }
    };

    // 9. Main Instruction Translation Loop
    for (size_t pc = mConfig.mainOffset; pc < programCode.size(); ++pc) {
        const Instruction instr = { programCode[pc] };
        OpCode::Id effectiveOp = instr.opcode.Value().EffectiveOpCode();

        if (effectiveOp == OpCode::Id::END) {
            break;
        }

        if (effectiveOp == OpCode::Id::NOP) {
            continue;
        }

        size_t swizzleOffset = (instr.opcode.Value().GetInfo().type == OpCode::Type::MultiplyAdd)
            ? instr.mad.operand_desc_id
            : instr.common.operand_desc_id;

        SwizzlePattern swizzle{};
        if (swizzleOffset < swizzleData.size()) {
            swizzle = { swizzleData[swizzleOffset] };
        }

        switch (instr.opcode.Value().GetInfo().type) {
        case OpCode::Type::Arithmetic: {
            bool isInv = (0 != (instr.opcode.Value().GetInfo().subtype & OpCode::Info::SrcInversed));
            uint32_t addrReg1 = !isInv * instr.common.address_register_index;
            uint32_t addrReg2 = isInv * instr.common.address_register_index;

            uint32_t rawSrc1 = loadSourceRegister(instr.common.GetSrc1(isInv), addrReg1);
            uint32_t src1 = applySwizzle(rawSrc1, swizzle, 1);

            uint32_t rawSrc2 = loadSourceRegister(instr.common.GetSrc2(isInv), addrReg2);
            uint32_t src2 = applySwizzle(rawSrc2, swizzle, 2);

            uint32_t result = 0;

            switch (effectiveOp) {
            case OpCode::Id::ADD:
                result = b.OpFAdd(typeVec4, src1, src2);
                break;
            case OpCode::Id::MUL:
                result = b.OpFMul(typeVec4, src1, src2);
                break;
            case OpCode::Id::FLR:
                result = b.OpExtInst(typeVec4, glslExtInst, GLSLstd450Floor, {src1});
                break;
            case OpCode::Id::MAX:
                result = b.OpExtInst(typeVec4, glslExtInst, GLSLstd450FMax, {src1, src2});
                break;
            case OpCode::Id::MIN:
                result = b.OpExtInst(typeVec4, glslExtInst, GLSLstd450FMin, {src1, src2});
                break;
            case OpCode::Id::DP3: {
                uint32_t s1_3 = b.OpVectorShuffle(typeVec3, src1, src1, {0, 1, 2});
                uint32_t s2_3 = b.OpVectorShuffle(typeVec3, src2, src2, {0, 1, 2});
                uint32_t dot = b.OpDot(typeFloat, s1_3, s2_3);
                result = b.OpCompositeConstruct(typeVec4, {dot, dot, dot, dot});
                break;
            }
            case OpCode::Id::DP4: {
                uint32_t dot = b.OpDot(typeFloat, src1, src2);
                result = b.OpCompositeConstruct(typeVec4, {dot, dot, dot, dot});
                break;
            }
            case OpCode::Id::DPH:
            case OpCode::Id::DPHI: {
                uint32_t s1_h = b.OpCompositeConstruct(typeVec4, {
                    b.OpCompositeExtract(typeFloat, src1, {0}),
                    b.OpCompositeExtract(typeFloat, src1, {1}),
                    b.OpCompositeExtract(typeFloat, src1, {2}),
                    cFloat1
                });
                uint32_t dot = b.OpDot(typeFloat, s1_h, src2);
                result = b.OpCompositeConstruct(typeVec4, {dot, dot, dot, dot});
                break;
            }
            case OpCode::Id::RCP: {
                uint32_t s1x = b.OpCompositeExtract(typeFloat, src1, {0});
                uint32_t rcp = b.OpFDiv(typeFloat, cFloat1, s1x);
                result = b.OpCompositeConstruct(typeVec4, {rcp, rcp, rcp, rcp});
                break;
            }
            case OpCode::Id::RSQ: {
                uint32_t s1x = b.OpCompositeExtract(typeFloat, src1, {0});
                uint32_t rsq = b.OpExtInst(typeFloat, glslExtInst, GLSLstd450InverseSqrt, {s1x});
                result = b.OpCompositeConstruct(typeVec4, {rsq, rsq, rsq, rsq});
                break;
            }
            case OpCode::Id::EX2: {
                uint32_t s1x = b.OpCompositeExtract(typeFloat, src1, {0});
                uint32_t ex2 = b.OpExtInst(typeFloat, glslExtInst, GLSLstd450Exp2, {s1x});
                result = b.OpCompositeConstruct(typeVec4, {ex2, ex2, ex2, ex2});
                break;
            }
            case OpCode::Id::LG2: {
                uint32_t s1x = b.OpCompositeExtract(typeFloat, src1, {0});
                uint32_t lg2 = b.OpExtInst(typeFloat, glslExtInst, GLSLstd450Log2, {s1x});
                result = b.OpCompositeConstruct(typeVec4, {lg2, lg2, lg2, lg2});
                break;
            }
            case OpCode::Id::MOV:
                result = src1;
                break;
            case OpCode::Id::MOVA: {
                uint32_t sx = b.OpCompositeExtract(typeFloat, src1, {0});
                uint32_t sy = b.OpCompositeExtract(typeFloat, src1, {1});
                uint32_t ix = b.OpConvertFToS(typeInt, sx);
                uint32_t iy = b.OpConvertFToS(typeInt, sy);
                uint32_t ivec = b.OpCompositeConstruct(typeIVec2, {ix, iy});
                b.OpStore(regAddr, ivec);
                break;
            }
            case OpCode::Id::SLT:
            case OpCode::Id::SLTI: {
                uint32_t cmp = b.OpFOrdLessThan(typeBVec4, src1, src2);
                result = b.OpSelect(typeVec4, cmp, cVec4One, cVec4Zero);
                break;
            }
            case OpCode::Id::SGE:
            case OpCode::Id::SGEI: {
                uint32_t cmp = b.OpFOrdGreaterThanEqual(typeBVec4, src1, src2);
                result = b.OpSelect(typeVec4, cmp, cVec4One, cVec4Zero);
                break;
            }
            case OpCode::Id::CMP: {
                using CompareOp = Instruction::Common::CompareOpType::Op;
                auto evaluateCmp = [&](CompareOp op, uint32_t val1, uint32_t val2) -> uint32_t {
                    switch (op) {
                    case CompareOp::Equal: return b.OpFOrdEqual(typeBool, val1, val2);
                    case CompareOp::NotEqual: return b.OpFOrdNotEqual(typeBool, val1, val2);
                    case CompareOp::LessThan: return b.OpFOrdLessThan(typeBool, val1, val2);
                    case CompareOp::LessEqual: return b.OpFOrdLessThanEqual(typeBool, val1, val2);
                    case CompareOp::GreaterThan: return b.OpFOrdGreaterThan(typeBool, val1, val2);
                    case CompareOp::GreaterEqual: return b.OpFOrdGreaterThanEqual(typeBool, val1, val2);
                    default: return b.OpFOrdEqual(typeBool, val1, val2);
                    }
                };

                uint32_t s1x = b.OpCompositeExtract(typeFloat, src1, {0});
                uint32_t s2x = b.OpCompositeExtract(typeFloat, src2, {0});
                uint32_t bx = evaluateCmp(instr.common.compare_op.x.Value(), s1x, s2x);

                uint32_t s1y = b.OpCompositeExtract(typeFloat, src1, {1});
                uint32_t s2y = b.OpCompositeExtract(typeFloat, src2, {1});
                uint32_t by = evaluateCmp(instr.common.compare_op.y.Value(), s1y, s2y);

                uint32_t cc = b.OpCompositeConstruct(typeBVec2, {bx, by});
                b.OpStore(regCc, cc);
                break;
            }
            default:
                break;
            }

            if (result != 0) {
                writeDestination(instr.common.dest.Value(), swizzle, result);
            }
            break;
        }

        case OpCode::Type::MultiplyAdd: {
            bool isInv = (effectiveOp == OpCode::Id::MADI);
            uint32_t addrReg2 = !isInv * instr.mad.address_register_index;
            uint32_t addrReg3 = isInv * instr.mad.address_register_index;

            uint32_t rawSrc1 = loadSourceRegister(instr.mad.GetSrc1(isInv), 0);
            uint32_t src1 = applySwizzle(rawSrc1, swizzle, 1);

            uint32_t rawSrc2 = loadSourceRegister(instr.mad.GetSrc2(isInv), addrReg2);
            uint32_t src2 = applySwizzle(rawSrc2, swizzle, 2);

            uint32_t rawSrc3 = loadSourceRegister(instr.mad.GetSrc3(isInv), addrReg3);
            uint32_t src3 = applySwizzle(rawSrc3, swizzle, 3);

            uint32_t mul = b.OpFMul(typeVec4, src1, src2);
            uint32_t result = b.OpFAdd(typeVec4, mul, src3);

            writeDestination(instr.mad.dest.Value(), swizzle, result);
            break;
        }

        default:
            break;
        }
    }

    // 10. Post-Processing: gl_Position and Output Varyings Export
    // Load o0 (Position)
    uint32_t posVec = b.OpLoad(typeVec4, regOut[0]);
    uint32_t posX = b.OpCompositeExtract(typeFloat, posVec, {0});
    uint32_t posY = b.OpCompositeExtract(typeFloat, posVec, {1});
    uint32_t posZ = b.OpCompositeExtract(typeFloat, posVec, {2});
    uint32_t posW = b.OpCompositeExtract(typeFloat, posVec, {3});

    // Optional Depth Emulation (PICA NDC depth mapping)
    if (mConfig.emulateDepth) {
        uint32_t depthScaleOffsetPtr = b.OpAccessChain(ptrUniformVec2, uniformVar, {cUInt2});
        uint32_t depthScaleOffset = b.OpLoad(typeVec2, depthScaleOffsetPtr);
        uint32_t depthScale = b.OpCompositeExtract(typeFloat, depthScaleOffset, {0});
        uint32_t depthOffset = b.OpCompositeExtract(typeFloat, depthScaleOffset, {1});

        uint32_t scaledZ = b.OpFMul(typeFloat, posZ, depthScale);
        uint32_t offsetW = b.OpFMul(typeFloat, depthOffset, posW);
        posZ = b.OpFAdd(typeFloat, scaledZ, offsetW);
    }

    // Optional Viewport Flip (uniform or static configuration)
    if (mConfig.flipViewport) {
        posY = b.OpFNegate(typeFloat, posY);
    } else {
        // Dynamic viewport flip check from uniforms.flip_viewport
        uint32_t flipPtr = b.OpAccessChain(ptrUniformInt, uniformVar, {cUInt1});
        uint32_t flipVal = b.OpLoad(typeInt, flipPtr);
        uint32_t shouldFlip = b.OpINotEqual(typeBool, flipVal, cInt0);
        uint32_t negPosY = b.OpFNegate(typeFloat, posY);
        posY = b.OpSelect(typeFloat, shouldFlip, negPosY, posY);
    }

    // Invert Z for Vulkan NDC convention: gl_Position = vec4(posX, posY, -posZ, posW)
    uint32_t negPosZ = b.OpFNegate(typeFloat, posZ);
    uint32_t finalGlPosition = b.OpCompositeConstruct(typeVec4, {posX, posY, negPosZ, posW});

    // Store to gl_PerVertex.Position
    uint32_t ptrGlPosition = b.OpAccessChain(ptrOutputVec4, glPerVertexVar, {cUInt0});
    b.OpStore(ptrGlPosition, finalGlPosition);

    // Export Varyings
    // 1. Primary Color: min(abs(o1), 1.0)
    uint32_t o1 = b.OpLoad(typeVec4, regOut[1]);
    uint32_t absO1 = b.OpExtInst(typeVec4, glslExtInst, GLSLstd450FAbs, {o1});
    uint32_t clampedColor = b.OpExtInst(typeVec4, glslExtInst, GLSLstd450FMin, {absO1, cVec4One});
    b.OpStore(varColor, clampedColor);

    // 2. Texcoord0: o2.xy
    uint32_t o2 = b.OpLoad(typeVec4, regOut[2]);
    uint32_t o2_xy = b.OpVectorShuffle(typeVec2, o2, o2, {0, 1});
    b.OpStore(varTex0, o2_xy);

    // 3. Texcoord0_w: o2.z
    uint32_t o2_z = b.OpCompositeExtract(typeFloat, o2, {2});
    b.OpStore(varTex0W, o2_z);

    // 4. Texcoord1: o3.xy
    uint32_t o3 = b.OpLoad(typeVec4, regOut[3]);
    uint32_t o3_xy = b.OpVectorShuffle(typeVec2, o3, o3, {0, 1});
    b.OpStore(varTex1, o3_xy);

    // 5. Texcoord2: o4.xy
    uint32_t o4 = b.OpLoad(typeVec4, regOut[4]);
    uint32_t o4_xy = b.OpVectorShuffle(typeVec2, o4, o4, {0, 1});
    b.OpStore(varTex2, o4_xy);

    // 6. Normquat: o5
    uint32_t o5 = b.OpLoad(typeVec4, regOut[5]);
    b.OpStore(varNormquat, o5);

    // 7. View: o6.xyz
    uint32_t o6 = b.OpLoad(typeVec4, regOut[6]);
    uint32_t o6_xyz = b.OpVectorShuffle(typeVec3, o6, o6, {0, 1, 2});
    b.OpStore(varView, o6_xyz);

    // 11. End of main()
    b.OpReturn();
    b.EndFunction();

    // 12. Assemble Final SPIR-V Binary Module
    outSpirv = b.Build();
    return true;
}

} // namespace PicaToSpirv
