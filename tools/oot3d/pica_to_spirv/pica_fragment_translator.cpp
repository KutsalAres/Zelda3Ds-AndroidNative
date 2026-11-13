#include "pica_fragment_translator.h"

#include <array>
#include <sstream>

namespace PicaToSpirv {

using ThreeDsRecomp::Pica::CompareFunc;
using ThreeDsRecomp::Pica::PicaDrawState;
using ThreeDsRecomp::Pica::PicaFragmentShaderConfig;
using ThreeDsRecomp::Pica::TevOp;
using ThreeDsRecomp::Pica::TevScale;
using ThreeDsRecomp::Pica::TevSource;

namespace {

void SetError(std::string* errorLog, std::string_view message) {
    if (errorLog) {
        *errorLog = std::string(message);
    }
}

} // namespace

PicaFragmentTranslator::PicaFragmentTranslator(uint32_t spirvVersion)
    : mSpirvVersion(spirvVersion) {
}

bool PicaFragmentTranslator::Translate(
    const PicaDrawState& drawState,
    const PicaFragmentShaderConfig& config,
    std::vector<uint32_t>& outSpirv,
    std::string* errorLog) {

    SpirvBuilder b(mSpirvVersion);

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

    // Constants
    uint32_t cFloat0 = b.ConstantFloat(0.0f);
    uint32_t cFloat05 = b.ConstantFloat(0.5f);
    uint32_t cFloat1 = b.ConstantFloat(1.0f);
    uint32_t cFloat2 = b.ConstantFloat(2.0f);
    uint32_t cFloat4 = b.ConstantFloat(4.0f);
    uint32_t cFloat255 = b.ConstantFloat(255.0f);
    uint32_t cFloatRecip255 = b.ConstantFloat(1.0f / 255.0f);

    uint32_t cVec3Zero = b.ConstantComposite(typeVec3, {cFloat0, cFloat0, cFloat0});
    uint32_t cVec3Half = b.ConstantComposite(typeVec3, {cFloat05, cFloat05, cFloat05});
    uint32_t cVec3One = b.ConstantComposite(typeVec3, {cFloat1, cFloat1, cFloat1});
    uint32_t cVec3_255 = b.ConstantComposite(typeVec3, {cFloat255, cFloat255, cFloat255});
    uint32_t cVec3_recip255 = b.ConstantComposite(typeVec3, {cFloatRecip255, cFloatRecip255, cFloatRecip255});
    uint32_t cVec4_255 = b.ConstantComposite(typeVec4, {cFloat255, cFloat255, cFloat255, cFloat255});
    uint32_t cVec4_recip255 = b.ConstantComposite(typeVec4, {cFloatRecip255, cFloatRecip255, cFloatRecip255, cFloatRecip255});

    uint32_t cVec4Zero = b.ConstantComposite(typeVec4, {cFloat0, cFloat0, cFloat0, cFloat0});
    uint32_t cVec4Half = b.ConstantComposite(typeVec4, {cFloat05, cFloat05, cFloat05, cFloat05});
    uint32_t cVec4One = b.ConstantComposite(typeVec4, {cFloat1, cFloat1, cFloat1, cFloat1});
    uint32_t cVec4_4 = b.ConstantComposite(typeVec4, {cFloat4, cFloat4, cFloat4, cFloat4});
    uint32_t cVec3_4 = b.ConstantComposite(typeVec3, {cFloat4, cFloat4, cFloat4});

    // Pointers
    uint32_t ptrInputVec4 = b.TypePointer(SpvStorageClassInput, typeVec4);
    uint32_t ptrInputVec3 = b.TypePointer(SpvStorageClassInput, typeVec3);
    uint32_t ptrInputVec2 = b.TypePointer(SpvStorageClassInput, typeVec2);
    uint32_t ptrInputFloat = b.TypePointer(SpvStorageClassInput, typeFloat);
    uint32_t ptrOutputVec4 = b.TypePointer(SpvStorageClassOutput, typeVec4);
    uint32_t ptrFunctionVec4 = b.TypePointer(SpvStorageClassFunction, typeVec4);
    uint32_t ptrFunctionVec3 = b.TypePointer(SpvStorageClassFunction, typeVec3);
    uint32_t ptrFunctionFloat = b.TypePointer(SpvStorageClassFunction, typeFloat);

    // 3. Texture Samplers (Set 0, Bindings 1, 2, 3)
    uint32_t typeImg2D = b.TypeImage2D(typeFloat);
    uint32_t typeSampledImg = b.TypeSampledImage(typeImg2D);
    uint32_t ptrUniformConstantSampledImg = b.TypePointer(SpvStorageClassUniformConstant, typeSampledImg);

    uint32_t varTex0 = b.AddVariable(ptrUniformConstantSampledImg, SpvStorageClassUniformConstant);
    b.AddDecorate(varTex0, SpvDecorationDescriptorSet, {0});
    b.AddDecorate(varTex0, SpvDecorationBinding, {1});
    b.AddName(varTex0, "pica_texture0");

    uint32_t varTex1 = b.AddVariable(ptrUniformConstantSampledImg, SpvStorageClassUniformConstant);
    b.AddDecorate(varTex1, SpvDecorationDescriptorSet, {0});
    b.AddDecorate(varTex1, SpvDecorationBinding, {2});
    b.AddName(varTex1, "pica_texture1");

    uint32_t varTex2 = b.AddVariable(ptrUniformConstantSampledImg, SpvStorageClassUniformConstant);
    b.AddDecorate(varTex2, SpvDecorationDescriptorSet, {0});
    b.AddDecorate(varTex2, SpvDecorationBinding, {3});
    b.AddName(varTex2, "pica_texture2");

    // 4. Fragment Uniforms (Set 0, Binding 4, std140)
    uint32_t cArrayLen6 = b.ConstantUInt(6);
    uint32_t cArrayLen8 = b.ConstantUInt(8);
    uint32_t cArrayLen64 = b.ConstantUInt(64);

    uint32_t typeArrVec4_6 = b.TypeArray(typeVec4, cArrayLen6);
    b.AddDecorate(typeArrVec4_6, SpvDecorationArrayStride, {16});

    uint32_t typeArrVec4_64 = b.TypeArray(typeVec4, cArrayLen64);
    b.AddDecorate(typeArrVec4_64, SpvDecorationArrayStride, {16});

    uint32_t typeArrVec4_8 = b.TypeArray(typeVec4, cArrayLen8);
    b.AddDecorate(typeArrVec4_8, SpvDecorationArrayStride, {16});

    uint32_t uniformStruct = b.TypeStruct({
        typeArrVec4_6,   // 0: tev_constants[6] (offset 0)
        typeVec4,        // 1: combiner_buffer_color (offset 96)
        typeFloat,       // 2: alpha_reference (offset 112)
        typeFloat,       // 3: reserved0 (offset 116)
        typeFloat,       // 4: reserved1 (offset 120)
        typeFloat,       // 5: reserved2 (offset 124)
        typeVec4,        // 6: fog_color (offset 128)
        typeArrVec4_64,  // 7: fog_lut[64] (offset 144)
        typeVec4,        // 8: texture_lod_bias (offset 1168)
        typeArrVec4_8,   // 9: lighting_specular0[8] (offset 1184)
        typeArrVec4_8,   // 10: lighting_diffuse[8] (offset 1312)
        typeArrVec4_8,   // 11: lighting_ambient[8] (offset 1440)
        typeArrVec4_8,   // 12: lighting_position[8] (offset 1568)
        typeVec4         // 13: lighting_global_ambient (offset 1696)
    });
    b.AddDecorate(uniformStruct, SpvDecorationBlock);
    b.AddMemberDecorate(uniformStruct, 0, SpvDecorationOffset, {0});
    b.AddMemberDecorate(uniformStruct, 1, SpvDecorationOffset, {96});
    b.AddMemberDecorate(uniformStruct, 2, SpvDecorationOffset, {112});
    b.AddMemberDecorate(uniformStruct, 3, SpvDecorationOffset, {116});
    b.AddMemberDecorate(uniformStruct, 4, SpvDecorationOffset, {120});
    b.AddMemberDecorate(uniformStruct, 5, SpvDecorationOffset, {124});
    b.AddMemberDecorate(uniformStruct, 6, SpvDecorationOffset, {128});
    b.AddMemberDecorate(uniformStruct, 7, SpvDecorationOffset, {144});
    b.AddMemberDecorate(uniformStruct, 8, SpvDecorationOffset, {1168});
    b.AddMemberDecorate(uniformStruct, 9, SpvDecorationOffset, {1184});
    b.AddMemberDecorate(uniformStruct, 10, SpvDecorationOffset, {1312});
    b.AddMemberDecorate(uniformStruct, 11, SpvDecorationOffset, {1440});
    b.AddMemberDecorate(uniformStruct, 12, SpvDecorationOffset, {1568});
    b.AddMemberDecorate(uniformStruct, 13, SpvDecorationOffset, {1696});
    b.AddName(uniformStruct, "PicaFragmentUniforms");

    uint32_t ptrUniformStruct = b.TypePointer(SpvStorageClassUniform, uniformStruct);
    uint32_t ptrUniformVec4 = b.TypePointer(SpvStorageClassUniform, typeVec4);
    uint32_t ptrUniformFloat = b.TypePointer(SpvStorageClassUniform, typeFloat);

    uint32_t uniformVar = b.AddVariable(ptrUniformStruct, SpvStorageClassUniform);
    b.AddDecorate(uniformVar, SpvDecorationDescriptorSet, {0});
    b.AddDecorate(uniformVar, SpvDecorationBinding, {4});
    b.AddName(uniformVar, "fragment_uniforms");

    // 5. Input Varyings (from vertex shader)
    uint32_t inPrimary = b.AddVariable(ptrInputVec4, SpvStorageClassInput);
    b.AddDecorate(inPrimary, SpvDecorationLocation, {0});
    b.AddName(inPrimary, "pica_primary_color");

    uint32_t inTex0 = b.AddVariable(ptrInputVec2, SpvStorageClassInput);
    b.AddDecorate(inTex0, SpvDecorationLocation, {1});
    b.AddName(inTex0, "pica_texcoord0");

    uint32_t inTex1 = b.AddVariable(ptrInputVec2, SpvStorageClassInput);
    b.AddDecorate(inTex1, SpvDecorationLocation, {2});
    b.AddName(inTex1, "pica_texcoord1");

    uint32_t inTex2 = b.AddVariable(ptrInputVec2, SpvStorageClassInput);
    b.AddDecorate(inTex2, SpvDecorationLocation, {3});
    b.AddName(inTex2, "pica_texcoord2");

    uint32_t inTex0W = b.AddVariable(ptrInputFloat, SpvStorageClassInput);
    b.AddDecorate(inTex0W, SpvDecorationLocation, {4});
    b.AddName(inTex0W, "pica_texcoord0_w");

    uint32_t inNormquat = b.AddVariable(ptrInputVec4, SpvStorageClassInput);
    b.AddDecorate(inNormquat, SpvDecorationLocation, {5});
    b.AddName(inNormquat, "pica_normquat");

    uint32_t inView = b.AddVariable(ptrInputVec3, SpvStorageClassInput);
    b.AddDecorate(inView, SpvDecorationLocation, {6});
    b.AddName(inView, "pica_view");

    // Built-in gl_FragCoord (for fog)
    uint32_t inFragCoord = b.AddVariable(ptrInputVec4, SpvStorageClassInput);
    b.AddDecorate(inFragCoord, SpvDecorationBuiltIn, {SpvBuiltInFragCoord});
    b.AddName(inFragCoord, "gl_FragCoord");

    // 6. Output Fragment Color
    uint32_t outColor = b.AddVariable(ptrOutputVec4, SpvStorageClassOutput);
    b.AddDecorate(outColor, SpvDecorationLocation, {0});
    b.AddName(outColor, "pica_color");

    // 7. Entry Point Registration
    uint32_t entryPointId = b.AllocId();
    std::vector<uint32_t> interfaces = {
        inPrimary, inTex0, inTex1, inTex2, inTex0W, inNormquat, inView, inFragCoord, outColor
    };
    b.SetEntryPoint(SpvExecutionModelFragment, entryPointId, "main", interfaces);
    b.AddExecutionMode(entryPointId, SpvExecutionModeOriginUpperLeft);

    // 8. Begin Function main()
    uint32_t typeFunc = b.TypeFunction(typeVoid, {});
    b.BeginFunction(typeVoid, typeFunc, SpvFunctionControlNone, entryPointId);
    uint32_t labelEntry = b.CreateLabel();
    b.SetCurrentBlock(labelEntry);

    // Function Scope Variables (must be declared first in first basic block)
    uint32_t varCombinerBuffer = b.AddLocalVariable(ptrFunctionVec4);
    b.AddName(varCombinerBuffer, "combiner_buffer");

    uint32_t varNextCombinerBuffer = b.AddLocalVariable(ptrFunctionVec4);
    b.AddName(varNextCombinerBuffer, "next_combiner_buffer");

    uint32_t varCombinerOutput = b.AddLocalVariable(ptrFunctionVec4);
    b.AddName(varCombinerOutput, "combiner_output");

    uint32_t varPrimaryFragColor = b.AddLocalVariable(ptrFunctionVec4);
    b.AddName(varPrimaryFragColor, "primary_fragment_color");

    uint32_t varSecondaryFragColor = b.AddLocalVariable(ptrFunctionVec4);
    b.AddName(varSecondaryFragColor, "secondary_fragment_color");

    // Initializations
    // Load rounded primary color
    uint32_t rawPrimary = b.OpLoad(typeVec4, inPrimary);
    uint32_t scaledP = b.OpFMul(typeVec4, rawPrimary, cVec4_255);
    uint32_t addedHalfP = b.OpFAdd(typeVec4, scaledP, cVec4Half);
    uint32_t flooredP = b.OpExtInst(typeVec4, glslExtInst, GLSLstd450Floor, {addedHalfP});
    uint32_t roundedPrimary = b.OpFMul(typeVec4, flooredP, cVec4_recip255);

    b.OpStore(varCombinerOutput, roundedPrimary);
    b.OpStore(varCombinerBuffer, cVec4Zero);
    b.OpStore(varPrimaryFragColor, cVec4Zero);
    b.OpStore(varSecondaryFragColor, cVec4Zero);

    // Initialize next_combiner_buffer from fragment_uniforms.combiner_buffer_color (Member 1)
    uint32_t cUInt1 = b.ConstantUInt(1);
    uint32_t ptrBufferColor = b.OpAccessChain(ptrUniformVec4, uniformVar, {cUInt1});
    uint32_t initialBufferColor = b.OpLoad(typeVec4, ptrBufferColor);
    b.OpStore(varNextCombinerBuffer, initialBufferColor);

    // Sample Textures
    uint32_t tex0Color = cVec4One;
    if (config.EnableTexture0) {
        uint32_t s0 = b.OpLoad(typeSampledImg, varTex0);
        uint32_t uv0 = b.OpLoad(typeVec2, inTex0);
        tex0Color = b.OpImageSampleImplicitLod(typeVec4, s0, uv0);
    }

    uint32_t tex1Color = cVec4One;
    if (config.EnableTexture1) {
        uint32_t s1 = b.OpLoad(typeSampledImg, varTex1);
        uint32_t uv1 = b.OpLoad(typeVec2, inTex1);
        tex1Color = b.OpImageSampleImplicitLod(typeVec4, s1, uv1);
    }

    uint32_t tex2Color = cVec4One;
    if (config.EnableTexture2) {
        uint32_t s2 = b.OpLoad(typeSampledImg, varTex2);
        uint32_t uv2 = b.OpLoad(typeVec2, inTex2);
        tex2Color = b.OpImageSampleImplicitLod(typeVec4, s2, uv2);
    }

    // Helper: Source Register Color (vec4)
    auto resolveSourceColor = [&](TevSource src, int stage) -> uint32_t {
        switch (src) {
        case TevSource::PrimaryColor: return roundedPrimary;
        case TevSource::PrimaryFragmentColor: return b.OpLoad(typeVec4, varPrimaryFragColor);
        case TevSource::SecondaryFragmentColor: return b.OpLoad(typeVec4, varSecondaryFragColor);
        case TevSource::Texture0: return tex0Color;
        case TevSource::Texture1: return tex1Color;
        case TevSource::Texture2: return tex2Color;
        case TevSource::Texture3: return cVec4Zero;
        case TevSource::PreviousBuffer: return b.OpLoad(typeVec4, varCombinerBuffer);
        case TevSource::Constant: {
            uint32_t cUInt0 = b.ConstantUInt(0);
            uint32_t cStageIdx = b.ConstantUInt(static_cast<uint32_t>(stage));
            uint32_t ptrConst = b.OpAccessChain(ptrUniformVec4, uniformVar, {cUInt0, cStageIdx});
            return b.OpLoad(typeVec4, ptrConst);
        }
        case TevSource::Previous:
        default:
            return b.OpLoad(typeVec4, varCombinerOutput);
        }
    };

    // Helper: Apply RGB Modifier -> returns vec3
    auto applyRgbModifier = [&](uint32_t srcVec4, uint8_t mod) -> uint32_t {
        switch (mod) {
        case 0: return b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {0, 1, 2}); // .rgb
        case 1: { // 1.0 - .rgb
            uint32_t rgb = b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {0, 1, 2});
            return b.OpFSub(typeVec3, cVec3One, rgb);
        }
        case 2: return b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {3, 3, 3}); // .aaa
        case 3: { // 1.0 - .aaa
            uint32_t aaa = b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {3, 3, 3});
            return b.OpFSub(typeVec3, cVec3One, aaa);
        }
        case 4: return b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {0, 0, 0}); // .rrr
        case 5: { // 1.0 - .rrr
            uint32_t rrr = b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {0, 0, 0});
            return b.OpFSub(typeVec3, cVec3One, rrr);
        }
        case 6: return b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {1, 1, 1}); // .ggg
        case 7: { // 1.0 - .ggg
            uint32_t ggg = b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {1, 1, 1});
            return b.OpFSub(typeVec3, cVec3One, ggg);
        }
        case 8: return b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {2, 2, 2}); // .bbb
        case 9: { // 1.0 - .bbb
            uint32_t bbb = b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {2, 2, 2});
            return b.OpFSub(typeVec3, cVec3One, bbb);
        }
        default:
            return b.OpVectorShuffle(typeVec3, srcVec4, srcVec4, {0, 1, 2});
        }
    };

    // Helper: Apply Alpha Modifier -> returns float
    auto applyAlphaModifier = [&](uint32_t srcVec4, uint8_t mod) -> uint32_t {
        switch (mod) {
        case 0: return b.OpCompositeExtract(typeFloat, srcVec4, {3}); // .a
        case 1: { // 1.0 - .a
            uint32_t a = b.OpCompositeExtract(typeFloat, srcVec4, {3});
            return b.OpFSub(typeFloat, cFloat1, a);
        }
        case 2: return b.OpCompositeExtract(typeFloat, srcVec4, {0}); // .r
        case 3: { // 1.0 - .r
            uint32_t r = b.OpCompositeExtract(typeFloat, srcVec4, {0});
            return b.OpFSub(typeFloat, cFloat1, r);
        }
        case 4: return b.OpCompositeExtract(typeFloat, srcVec4, {1}); // .g
        case 5: { // 1.0 - .g
            uint32_t g = b.OpCompositeExtract(typeFloat, srcVec4, {1});
            return b.OpFSub(typeFloat, cFloat1, g);
        }
        case 6: return b.OpCompositeExtract(typeFloat, srcVec4, {2}); // .b
        case 7: { // 1.0 - .b
            uint32_t bVal = b.OpCompositeExtract(typeFloat, srcVec4, {2});
            return b.OpFSub(typeFloat, cFloat1, bVal);
        }
        default:
            return b.OpCompositeExtract(typeFloat, srcVec4, {3});
        }
    };

    // 9. TEV 6-stage Combiners
    for (int stage = 0; stage < 6; ++stage) {
        const auto& s = drawState.Tev.Stages[stage];

        if (config.EnableCombinerBuffer) {
            uint32_t nextBuf = b.OpLoad(typeVec4, varNextCombinerBuffer);
            b.OpStore(varCombinerBuffer, nextBuf);
        }

        // RGB Sources
        uint32_t rgbS0 = applyRgbModifier(resolveSourceColor(s.SourceRgb[0], stage), s.OperandRgb[0]);
        uint32_t rgbS1 = applyRgbModifier(resolveSourceColor(s.SourceRgb[1], stage), s.OperandRgb[1]);
        uint32_t rgbS2 = applyRgbModifier(resolveSourceColor(s.SourceRgb[2], stage), s.OperandRgb[2]);

        // Alpha Sources
        uint32_t aS0 = applyAlphaModifier(resolveSourceColor(s.SourceAlpha[0], stage), s.OperandAlpha[0]);
        uint32_t aS1 = applyAlphaModifier(resolveSourceColor(s.SourceAlpha[1], stage), s.OperandAlpha[1]);
        uint32_t aS2 = applyAlphaModifier(resolveSourceColor(s.SourceAlpha[2], stage), s.OperandAlpha[2]);

        // Evaluate RGB Op
        uint32_t rgbRes = rgbS0;
        switch (s.OpRgb) {
        case TevOp::Replace:
            rgbRes = rgbS0;
            break;
        case TevOp::Modulate:
            rgbRes = b.OpFMul(typeVec3, rgbS0, rgbS1);
            break;
        case TevOp::Add:
            rgbRes = b.OpFAdd(typeVec3, rgbS0, rgbS1);
            break;
        case TevOp::AddSigned: {
            uint32_t sum = b.OpFAdd(typeVec3, rgbS0, rgbS1);
            rgbRes = b.OpFSub(typeVec3, sum, cVec3Half);
            break;
        }
        case TevOp::Interpolate: {
            // mix(rgbS1, rgbS0, rgbS2)
            rgbRes = b.OpExtInst(typeVec3, glslExtInst, GLSLstd450FMix, {rgbS1, rgbS0, rgbS2});
            break;
        }
        case TevOp::Subtract:
            rgbRes = b.OpFSub(typeVec3, rgbS0, rgbS1);
            break;
        case TevOp::Dot3Rgb: {
            uint32_t s0_b = b.OpFSub(typeVec3, rgbS0, cVec3Half);
            uint32_t s1_b = b.OpFSub(typeVec3, rgbS1, cVec3Half);
            uint32_t dot = b.OpDot(typeFloat, s0_b, s1_b);
            uint32_t dot4 = b.OpFMul(typeFloat, dot, cFloat4);
            uint32_t clamped = b.OpExtInst(typeFloat, glslExtInst, GLSLstd450FClamp, {dot4, cFloat0, cFloat1});
            rgbRes = b.OpCompositeConstruct(typeVec3, {clamped, clamped, clamped});
            break;
        }
        case TevOp::Dot3Rgba: {
            uint32_t v4_0 = b.OpCompositeConstruct(typeVec4, {
                b.OpCompositeExtract(typeFloat, rgbS0, {0}),
                b.OpCompositeExtract(typeFloat, rgbS0, {1}),
                b.OpCompositeExtract(typeFloat, rgbS0, {2}),
                cFloat1
            });
            uint32_t v4_1 = b.OpCompositeConstruct(typeVec4, {
                b.OpCompositeExtract(typeFloat, rgbS1, {0}),
                b.OpCompositeExtract(typeFloat, rgbS1, {1}),
                b.OpCompositeExtract(typeFloat, rgbS1, {2}),
                cFloat1
            });
            uint32_t s0_b = b.OpFSub(typeVec4, v4_0, cVec4Half);
            uint32_t s1_b = b.OpFSub(typeVec4, v4_1, cVec4Half);
            uint32_t dot = b.OpDot(typeFloat, s0_b, s1_b);
            uint32_t dot4 = b.OpFMul(typeFloat, dot, cFloat4);
            uint32_t clamped = b.OpExtInst(typeFloat, glslExtInst, GLSLstd450FClamp, {dot4, cFloat0, cFloat1});
            rgbRes = b.OpCompositeConstruct(typeVec3, {clamped, clamped, clamped});
            break;
        }
        case TevOp::MultiplyAdd: {
            uint32_t mul = b.OpFMul(typeVec3, rgbS0, rgbS1);
            rgbRes = b.OpFAdd(typeVec3, mul, rgbS2);
            break;
        }
        case TevOp::AddMultiply: {
            uint32_t add = b.OpFAdd(typeVec3, rgbS0, rgbS1);
            rgbRes = b.OpFMul(typeVec3, add, rgbS2);
            break;
        }
        }

        // Evaluate Alpha Op
        uint32_t aRes = aS0;
        switch (s.OpAlpha) {
        case TevOp::Replace:
            aRes = aS0;
            break;
        case TevOp::Modulate:
            aRes = b.OpFMul(typeFloat, aS0, aS1);
            break;
        case TevOp::Add:
            aRes = b.OpFAdd(typeFloat, aS0, aS1);
            break;
        case TevOp::AddSigned: {
            uint32_t sum = b.OpFAdd(typeFloat, aS0, aS1);
            aRes = b.OpFSub(typeFloat, sum, cFloat05);
            break;
        }
        case TevOp::Interpolate:
            aRes = b.OpExtInst(typeFloat, glslExtInst, GLSLstd450FMix, {aS1, aS0, aS2});
            break;
        case TevOp::Subtract:
            aRes = b.OpFSub(typeFloat, aS0, aS1);
            break;
        case TevOp::Dot3Rgb:
        case TevOp::Dot3Rgba: {
            uint32_t v4_0 = b.OpCompositeConstruct(typeVec4, {
                b.OpCompositeExtract(typeFloat, rgbS0, {0}),
                b.OpCompositeExtract(typeFloat, rgbS0, {1}),
                b.OpCompositeExtract(typeFloat, rgbS0, {2}),
                aS0
            });
            uint32_t v4_1 = b.OpCompositeConstruct(typeVec4, {
                b.OpCompositeExtract(typeFloat, rgbS1, {0}),
                b.OpCompositeExtract(typeFloat, rgbS1, {1}),
                b.OpCompositeExtract(typeFloat, rgbS1, {2}),
                aS1
            });
            uint32_t s0_b = b.OpFSub(typeVec4, v4_0, cVec4Half);
            uint32_t s1_b = b.OpFSub(typeVec4, v4_1, cVec4Half);
            uint32_t dot = b.OpDot(typeFloat, s0_b, s1_b);
            uint32_t dot4 = b.OpFMul(typeFloat, dot, cFloat4);
            aRes = b.OpExtInst(typeFloat, glslExtInst, GLSLstd450FClamp, {dot4, cFloat0, cFloat1});
            break;
        }
        case TevOp::MultiplyAdd: {
            uint32_t mul = b.OpFMul(typeFloat, aS0, aS1);
            aRes = b.OpFAdd(typeFloat, mul, aS2);
            break;
        }
        case TevOp::AddMultiply: {
            uint32_t add = b.OpFAdd(typeFloat, aS0, aS1);
            aRes = b.OpFMul(typeFloat, add, aS2);
            break;
        }
        }

        // Apply Scale RGB
        if (s.ScaleRgb == TevScale::Scale2x) {
            uint32_t cVec3_2 = b.ConstantComposite(typeVec3, {cFloat2, cFloat2, cFloat2});
            rgbRes = b.OpFMul(typeVec3, rgbRes, cVec3_2);
        } else if (s.ScaleRgb == TevScale::Scale4x) {
            rgbRes = b.OpFMul(typeVec3, rgbRes, cVec3_4);
        }

        // Apply Scale Alpha
        if (s.ScaleAlpha == TevScale::Scale2x) {
            aRes = b.OpFMul(typeFloat, aRes, cFloat2);
        } else if (s.ScaleAlpha == TevScale::Scale4x) {
            aRes = b.OpFMul(typeFloat, aRes, cFloat4);
        }

        // Clamp to [0, 1]
        uint32_t rgbClamped = b.OpExtInst(typeVec3, glslExtInst, GLSLstd450FClamp, {rgbRes, cVec3Zero, cVec3One});
        uint32_t aClamped = b.OpExtInst(typeFloat, glslExtInst, GLSLstd450FClamp, {aRes, cFloat0, cFloat1});

        uint32_t finalStageOutput = b.OpCompositeConstruct(typeVec4, {
            b.OpCompositeExtract(typeFloat, rgbClamped, {0}),
            b.OpCompositeExtract(typeFloat, rgbClamped, {1}),
            b.OpCompositeExtract(typeFloat, rgbClamped, {2}),
            aClamped
        });

        b.OpStore(varCombinerOutput, finalStageOutput);

        if (config.EnableCombinerBuffer && (drawState.Tev.CombinerBufferInputMask & (1 << stage))) {
            b.OpStore(varNextCombinerBuffer, finalStageOutput);
        }
    }

    uint32_t finalColor = b.OpLoad(typeVec4, varCombinerOutput);

    // 10. Fog Blending
    if (config.EnableFog && drawState.Fog.Enabled) {
        uint32_t rawFragCoord = b.OpLoad(typeVec4, inFragCoord);
        uint32_t fragZ = b.OpCompositeExtract(typeFloat, rawFragCoord, {2});
        uint32_t fogFactor = b.OpExtInst(typeFloat, glslExtInst, GLSLstd450FClamp, {fragZ, cFloat0, cFloat1});

        uint32_t cUInt6 = b.ConstantUInt(6);
        uint32_t ptrFogColor = b.OpAccessChain(ptrUniformVec4, uniformVar, {cUInt6});
        uint32_t fogColorVec4 = b.OpLoad(typeVec4, ptrFogColor);
        uint32_t fogColorRgb = b.OpVectorShuffle(typeVec3, fogColorVec4, fogColorVec4, {0, 1, 2});

        uint32_t curRgb = b.OpVectorShuffle(typeVec3, finalColor, finalColor, {0, 1, 2});
        uint32_t fogVec3 = b.OpCompositeConstruct(typeVec3, {fogFactor, fogFactor, fogFactor});
        uint32_t foggedRgb = b.OpExtInst(typeVec3, glslExtInst, GLSLstd450FMix, {curRgb, fogColorRgb, fogVec3});

        finalColor = b.OpCompositeConstruct(typeVec4, {
            b.OpCompositeExtract(typeFloat, foggedRgb, {0}),
            b.OpCompositeExtract(typeFloat, foggedRgb, {1}),
            b.OpCompositeExtract(typeFloat, foggedRgb, {2}),
            b.OpCompositeExtract(typeFloat, finalColor, {3})
        });
    }

    // 11. Alpha Test
    if (config.EnableAlphaTest && drawState.DepthStencil.AlphaTest) {
        uint32_t cUInt2 = b.ConstantUInt(2);
        uint32_t ptrAlphaRef = b.OpAccessChain(ptrUniformFloat, uniformVar, {cUInt2});
        uint32_t alphaRef = b.OpLoad(typeFloat, ptrAlphaRef);
        uint32_t curAlpha = b.OpCompositeExtract(typeFloat, finalColor, {3});

        uint32_t condPass = 0;
        switch (drawState.DepthStencil.AlphaCompare) {
        case CompareFunc::Never: condPass = b.ConstantBool(false); break; // false
        case CompareFunc::Always: condPass = b.ConstantBool(true); break; // true
        case CompareFunc::Equal: condPass = b.OpFOrdEqual(typeBool, curAlpha, alphaRef); break;
        case CompareFunc::NotEqual: condPass = b.OpFOrdNotEqual(typeBool, curAlpha, alphaRef); break;
        case CompareFunc::Less: condPass = b.OpFOrdLessThan(typeBool, curAlpha, alphaRef); break;
        case CompareFunc::LessOrEqual: condPass = b.OpFOrdLessThanEqual(typeBool, curAlpha, alphaRef); break;
        case CompareFunc::Greater: condPass = b.OpFOrdGreaterThan(typeBool, curAlpha, alphaRef); break;
        case CompareFunc::GreaterOrEqual: condPass = b.OpFOrdGreaterThanEqual(typeBool, curAlpha, alphaRef); break;
        }

        uint32_t labelPass = b.CreateLabel();
        uint32_t labelKill = b.CreateLabel();

        b.OpSelectionMerge(labelPass, 0);
        b.OpBranchConditional(condPass, labelPass, labelKill);

        b.SetCurrentBlock(labelKill);
        b.OpKill();

        b.SetCurrentBlock(labelPass);
    }

    // 12. Output Color Store
    b.OpStore(outColor, finalColor);
    b.OpReturn();
    b.EndFunction();

    // 13. Build SPIR-V binary words
    outSpirv = b.Build();
    return true;
}

} // namespace PicaToSpirv
