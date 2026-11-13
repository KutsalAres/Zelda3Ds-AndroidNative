#include "pica_vertex_translator.h"
#include "pica_fragment_translator.h"
#include "spirv_builder.h"
#include "three_ds_recomp/pica/pica_state.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

using namespace PicaToSpirv;
using nihstro::DestRegister;
using nihstro::Instruction;
using nihstro::OpCode;
using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

namespace {

bool ValidateWithSpirvVal(const std::vector<uint32_t>& spirv) {
    const char* tmpPath = "/tmp/test_pica_spirv.spv";
    std::ofstream out(tmpPath, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open temporary file for spirv-val" << std::endl;
        return false;
    }
    out.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(uint32_t));
    out.close();

    std::string cmd = "spirv-val " + std::string(tmpPath) + " 2>&1";
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

void TestSpirvBuilderBasics() {
    std::cout << "[TEST] SpirvBuilderBasics... ";
    SpirvBuilder b(kSpirvVersion1_0);
    b.AddCapability(SpvCapabilityShader);
    b.SetMemoryModel(SpvMemoryModelGLSL450);

    uint32_t voidType = b.TypeVoid();
    uint32_t funcType = b.TypeFunction(voidType, {});
    uint32_t funcId = b.BeginFunction(voidType, funcType);
    uint32_t entryLabel = b.CreateLabel();
    b.SetCurrentBlock(entryLabel);
    b.OpReturn();
    b.EndFunction();

    b.SetEntryPoint(SpvExecutionModelVertex, funcId, "main", {});

    std::vector<uint32_t> spv = b.Build();
    assert(spv.size() >= 5);
    assert(spv[0] == kSpirvMagicNumber);
    assert(spv[1] == kSpirvVersion1_0);
    assert(spv[2] == kPicaToSpirvGeneratorId);
    assert(spv[4] == 0); // schema

    std::cout << "PASSED (binary words: " << spv.size() << ")" << std::endl;
}

void TestPicaVertexSimpleArithmetic() {
    std::cout << "[TEST] PicaVertexSimpleArithmetic... ";

    std::vector<uint32_t> program;
    std::vector<uint32_t> swizzles;

    // Swizzle 0: Identity swizzle, all components enabled (.xyzw)
    SwizzlePattern sw0{};
    sw0.dest_mask = 0xFU;
    sw0.src1_selector_0 = SwizzlePattern::Selector::x;
    sw0.src1_selector_1 = SwizzlePattern::Selector::y;
    sw0.src1_selector_2 = SwizzlePattern::Selector::z;
    sw0.src1_selector_3 = SwizzlePattern::Selector::w;
    sw0.src2_selector_0 = SwizzlePattern::Selector::x;
    sw0.src2_selector_1 = SwizzlePattern::Selector::y;
    sw0.src2_selector_2 = SwizzlePattern::Selector::z;
    sw0.src2_selector_3 = SwizzlePattern::Selector::w;
    swizzles.push_back(sw0.hex);

    // Instr 0: ADD r0, c0, v0 (add uniform c0 to input v0, store to r0)
    Instruction addInstr{};
    addInstr.opcode = OpCode::Id::ADD;
    addInstr.common.dest = DestRegister::MakeTemporary(0);
    addInstr.common.src1 = SourceRegister::MakeFloat(0);
    addInstr.common.src2 = SourceRegister::MakeInput(0);
    addInstr.common.operand_desc_id = 0;
    program.push_back(addInstr.hex);

    // Instr 1: MUL o0, c1, r0 (multiply uniform c1 by r0, store to o0 = gl_Position)
    Instruction mulInstr{};
    mulInstr.opcode = OpCode::Id::MUL;
    mulInstr.common.dest = DestRegister::MakeOutput(0);
    mulInstr.common.src1 = SourceRegister::MakeFloat(1);
    mulInstr.common.src2 = SourceRegister::MakeTemporary(0);
    mulInstr.common.operand_desc_id = 0;
    program.push_back(mulInstr.hex);

    // Instr 2: END
    Instruction endInstr{};
    endInstr.opcode = OpCode::Id::END;
    program.push_back(endInstr.hex);

    PicaVertexTranslator translator;
    std::vector<uint32_t> spirv;
    std::string error;
    bool success = translator.Translate(program, swizzles, spirv, &error);
    if (!success) {
        std::cerr << "Translation failed: " << error << std::endl;
    }
    assert(success);
    assert(!spirv.empty());
    assert(spirv[0] == kSpirvMagicNumber);

    // Official Khronos validation
    bool valid = ValidateWithSpirvVal(spirv);
    if (!valid) {
        std::cerr << "spirv-val verification failed!" << std::endl;
    }
    assert(valid);

    std::cout << "PASSED (SPIR-V valid! words: " << spirv.size() << ")" << std::endl;
}

void TestPicaVertexSwizzlesAndMasks() {
    std::cout << "[TEST] PicaVertexSwizzlesAndMasks... ";

    std::vector<uint32_t> program;
    std::vector<uint32_t> swizzles;

    // Swizzle 0: Mask .xy, src1 .yyyy, src2 .-zzxx
    SwizzlePattern sw0{};
    sw0.dest_mask = 0xCU; // 1100 in binary: components 0 and 1 (x, y) enabled
    sw0.src1_selector_0 = SwizzlePattern::Selector::y;
    sw0.src1_selector_1 = SwizzlePattern::Selector::y;
    sw0.src1_selector_2 = SwizzlePattern::Selector::y;
    sw0.src1_selector_3 = SwizzlePattern::Selector::y;
    sw0.src2_selector_0 = SwizzlePattern::Selector::z;
    sw0.src2_selector_1 = SwizzlePattern::Selector::z;
    sw0.src2_selector_2 = SwizzlePattern::Selector::x;
    sw0.src2_selector_3 = SwizzlePattern::Selector::x;
    sw0.negate_src2 = 1;
    swizzles.push_back(sw0.hex);

    // Instr 0: MOV r1, v1
    Instruction movInstr{};
    movInstr.opcode = OpCode::Id::MOV;
    movInstr.common.dest = DestRegister::MakeTemporary(1);
    movInstr.common.src1 = SourceRegister::MakeInput(1);
    movInstr.common.operand_desc_id = 0;
    program.push_back(movInstr.hex);

    // Instr 1: DP4 o0, c2, r1
    Instruction dp4Instr{};
    dp4Instr.opcode = OpCode::Id::DP4;
    dp4Instr.common.dest = DestRegister::MakeOutput(0);
    dp4Instr.common.src1 = SourceRegister::MakeFloat(2);
    dp4Instr.common.src2 = SourceRegister::MakeTemporary(1);
    dp4Instr.common.operand_desc_id = 0;
    program.push_back(dp4Instr.hex);

    // Instr 2: END
    Instruction endInstr{};
    endInstr.opcode = OpCode::Id::END;
    program.push_back(endInstr.hex);

    PicaVertexTranslator translator;
    std::vector<uint32_t> spirv;
    std::string error;
    bool success = translator.Translate(program, swizzles, spirv, &error);
    assert(success);
    assert(ValidateWithSpirvVal(spirv));

    std::cout << "PASSED (SPIR-V valid! words: " << spirv.size() << ")" << std::endl;
}

void TestPicaVertexMultiplyAddAndSpecialOps() {
    std::cout << "[TEST] PicaVertexMultiplyAddAndSpecialOps... ";

    std::vector<uint32_t> program;
    std::vector<uint32_t> swizzles;

    SwizzlePattern sw{};
    sw.dest_mask = 0xFU;
    sw.src1_selector_0 = SwizzlePattern::Selector::x;
    sw.src1_selector_1 = SwizzlePattern::Selector::y;
    sw.src1_selector_2 = SwizzlePattern::Selector::z;
    sw.src1_selector_3 = SwizzlePattern::Selector::w;
    sw.src2_selector_0 = SwizzlePattern::Selector::x;
    sw.src2_selector_1 = SwizzlePattern::Selector::y;
    sw.src2_selector_2 = SwizzlePattern::Selector::z;
    sw.src2_selector_3 = SwizzlePattern::Selector::w;
    sw.src3_selector_0 = SwizzlePattern::Selector::x;
    sw.src3_selector_1 = SwizzlePattern::Selector::y;
    sw.src3_selector_2 = SwizzlePattern::Selector::z;
    sw.src3_selector_3 = SwizzlePattern::Selector::w;
    swizzles.push_back(sw.hex);

    // Instr 0: MAD r0, v0, c0, v1 (r0 = v0 * c0 + v1)
    Instruction madInstr{};
    madInstr.opcode = OpCode::Id::MAD;
    madInstr.mad.dest = DestRegister::MakeTemporary(0);
    madInstr.mad.src1 = SourceRegister::MakeInput(0);
    madInstr.mad.src2 = SourceRegister::MakeFloat(0);
    madInstr.mad.src3 = SourceRegister::MakeInput(1);
    madInstr.mad.operand_desc_id = 0;
    program.push_back(madInstr.hex);

    // Instr 1: RSQ r1, r0
    Instruction rsqInstr{};
    rsqInstr.opcode = OpCode::Id::RSQ;
    rsqInstr.common.dest = DestRegister::MakeTemporary(1);
    rsqInstr.common.src1 = SourceRegister::MakeTemporary(0);
    rsqInstr.common.operand_desc_id = 0;
    program.push_back(rsqInstr.hex);

    // Instr 2: RCP r2, r1
    Instruction rcpInstr{};
    rcpInstr.opcode = OpCode::Id::RCP;
    rcpInstr.common.dest = DestRegister::MakeTemporary(2);
    rcpInstr.common.src1 = SourceRegister::MakeTemporary(1);
    rcpInstr.common.operand_desc_id = 0;
    program.push_back(rcpInstr.hex);

    // Instr 3: EX2 r3, r2
    Instruction ex2Instr{};
    ex2Instr.opcode = OpCode::Id::EX2;
    ex2Instr.common.dest = DestRegister::MakeTemporary(3);
    ex2Instr.common.src1 = SourceRegister::MakeTemporary(2);
    ex2Instr.common.operand_desc_id = 0;
    program.push_back(ex2Instr.hex);

    // Instr 4: MOV o0, r3
    Instruction movInstr{};
    movInstr.opcode = OpCode::Id::MOV;
    movInstr.common.dest = DestRegister::MakeOutput(0);
    movInstr.common.src1 = SourceRegister::MakeTemporary(3);
    movInstr.common.operand_desc_id = 0;
    program.push_back(movInstr.hex);

    // Instr 5: END
    Instruction endInstr{};
    endInstr.opcode = OpCode::Id::END;
    program.push_back(endInstr.hex);

    PicaVertexTranslator translator;
    std::vector<uint32_t> spirv;
    std::string error;
    bool success = translator.Translate(program, swizzles, spirv, &error);
    assert(success);
    assert(ValidateWithSpirvVal(spirv));

    std::cout << "PASSED (SPIR-V valid! words: " << spirv.size() << ")" << std::endl;
}

void TestBenchmarkThroughput() {
    std::cout << "[TEST] BenchmarkDirectSpirvThroughput... ";

    std::vector<uint32_t> program;
    std::vector<uint32_t> swizzles;

    SwizzlePattern sw{};
    sw.dest_mask = 0xFU;
    swizzles.push_back(sw.hex);

    // 40 arithmetic instructions (typical complex PICA vertex shader)
    for (int i = 0; i < 39; ++i) {
        Instruction instr{};
        instr.opcode = OpCode::Id::MAD;
        instr.mad.dest = DestRegister::MakeTemporary(i % 16);
        instr.mad.src1 = SourceRegister::MakeInput(i % 16);
        instr.mad.src2 = SourceRegister::MakeFloat(i % 32);
        instr.mad.src3 = SourceRegister::MakeTemporary((i + 1) % 16);
        instr.mad.operand_desc_id = 0;
        program.push_back(instr.hex);
    }
    Instruction endInstr{};
    endInstr.opcode = OpCode::Id::END;
    program.push_back(endInstr.hex);

    PicaVertexTranslator translator;
    std::vector<uint32_t> spirv;

    // Warmup
    translator.Translate(program, swizzles, spirv);

    constexpr int kIterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < kIterations; ++iter) {
        spirv.clear();
        translator.Translate(program, swizzles, spirv);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double totalUs = std::chrono::duration<double, std::micro>(end - start).count();
    double perShaderUs = totalUs / kIterations;

    std::cout << "PASSED (" << perShaderUs << " µs per shader, ~"
              << static_cast<uint64_t>(1000000.0 / perShaderUs) << " shaders/sec)" << std::endl;
}

void TestPicaFragmentBasics() {
    std::cout << "[TEST] PicaFragmentBasics... ";

    ThreeDsRecomp::Pica::PicaDrawState state{};
    state.Tev.Stages[0].OpRgb = ThreeDsRecomp::Pica::TevOp::Modulate;
    state.Tev.Stages[0].SourceRgb[0] = ThreeDsRecomp::Pica::TevSource::Texture0;
    state.Tev.Stages[0].SourceRgb[1] = ThreeDsRecomp::Pica::TevSource::PrimaryColor;
    state.Tev.Stages[0].OpAlpha = ThreeDsRecomp::Pica::TevOp::Replace;
    state.Tev.Stages[0].SourceAlpha[0] = ThreeDsRecomp::Pica::TevSource::Texture0;

    ThreeDsRecomp::Pica::PicaFragmentShaderConfig config{};
    config.EnableTexture0 = true;
    config.EnableTexture1 = false;
    config.EnableTexture2 = false;
    config.EnableFog = false;
    config.EnableAlphaTest = false;

    PicaFragmentTranslator translator;
    std::vector<uint32_t> spirv;
    std::string err;
    bool ok = translator.Translate(state, config, spirv, &err);
    assert(ok);
    assert(!spirv.empty());
    assert(ValidateWithSpirvVal(spirv));

    std::cout << "PASSED (SPIR-V valid! words: " << spirv.size() << ")" << std::endl;
}

void TestPicaFragmentComplexFeatures() {
    std::cout << "[TEST] PicaFragmentComplexFeatures (Fog, AlphaTest, 6 TEV Stages)... ";

    ThreeDsRecomp::Pica::PicaDrawState state{};
    // Configure multiple TEV stages with various ops
    state.Tev.Stages[0].OpRgb = ThreeDsRecomp::Pica::TevOp::Dot3Rgb;
    state.Tev.Stages[0].SourceRgb[0] = ThreeDsRecomp::Pica::TevSource::Texture0;
    state.Tev.Stages[0].SourceRgb[1] = ThreeDsRecomp::Pica::TevSource::PrimaryColor;

    state.Tev.Stages[1].OpRgb = ThreeDsRecomp::Pica::TevOp::Interpolate;
    state.Tev.Stages[1].SourceRgb[0] = ThreeDsRecomp::Pica::TevSource::Previous;
    state.Tev.Stages[1].SourceRgb[1] = ThreeDsRecomp::Pica::TevSource::Texture1;
    state.Tev.Stages[1].SourceRgb[2] = ThreeDsRecomp::Pica::TevSource::Constant;

    state.Tev.Stages[2].OpRgb = ThreeDsRecomp::Pica::TevOp::MultiplyAdd;
    state.Tev.Stages[2].SourceRgb[0] = ThreeDsRecomp::Pica::TevSource::Previous;
    state.Tev.Stages[2].SourceRgb[1] = ThreeDsRecomp::Pica::TevSource::Texture2;
    state.Tev.Stages[2].SourceRgb[2] = ThreeDsRecomp::Pica::TevSource::PreviousBuffer;

    state.Tev.Stages[2].ScaleRgb = ThreeDsRecomp::Pica::TevScale::Scale2x;
    state.Tev.Stages[2].ScaleAlpha = ThreeDsRecomp::Pica::TevScale::Scale4x;

    state.Tev.CombinerBufferInputMask = 0x07;

    state.Fog.Enabled = true;
    state.DepthStencil.AlphaTest = true;
    state.DepthStencil.AlphaCompare = ThreeDsRecomp::Pica::CompareFunc::Greater;

    ThreeDsRecomp::Pica::PicaFragmentShaderConfig config{};
    config.EnableTexture0 = true;
    config.EnableTexture1 = true;
    config.EnableTexture2 = true;
    config.EnableFog = true;
    config.EnableAlphaTest = true;
    config.EnableCombinerBuffer = true;

    PicaFragmentTranslator translator;
    std::vector<uint32_t> spirv;
    std::string err;
    bool ok = translator.Translate(state, config, spirv, &err);
    assert(ok);
    assert(!spirv.empty());
    assert(ValidateWithSpirvVal(spirv));

    std::cout << "PASSED (SPIR-V valid! words: " << spirv.size() << ")" << std::endl;
}

void TestBenchmarkFragmentThroughput() {
    std::cout << "[TEST] BenchmarkFragmentDirectSpirvThroughput... ";

    ThreeDsRecomp::Pica::PicaDrawState state{};
    state.Tev.Stages[0].OpRgb = ThreeDsRecomp::Pica::TevOp::Modulate;
    state.Tev.Stages[0].SourceRgb[0] = ThreeDsRecomp::Pica::TevSource::Texture0;
    state.Tev.Stages[0].SourceRgb[1] = ThreeDsRecomp::Pica::TevSource::PrimaryColor;
    state.Tev.Stages[1].OpRgb = ThreeDsRecomp::Pica::TevOp::Add;
    state.Tev.Stages[1].SourceRgb[0] = ThreeDsRecomp::Pica::TevSource::Previous;
    state.Tev.Stages[1].SourceRgb[1] = ThreeDsRecomp::Pica::TevSource::Texture1;
    state.Fog.Enabled = true;
    state.DepthStencil.AlphaTest = true;
    state.DepthStencil.AlphaCompare = ThreeDsRecomp::Pica::CompareFunc::GreaterOrEqual;

    ThreeDsRecomp::Pica::PicaFragmentShaderConfig config{};
    PicaFragmentTranslator translator;
    std::vector<uint32_t> spirv;

    // Warmup
    translator.Translate(state, config, spirv);

    constexpr int kIterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < kIterations; ++iter) {
        spirv.clear();
        translator.Translate(state, config, spirv);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double totalUs = std::chrono::duration<double, std::micro>(end - start).count();
    double perShaderUs = totalUs / kIterations;

    std::cout << "PASSED (" << perShaderUs << " µs per shader, ~"
              << static_cast<uint64_t>(1000000.0 / perShaderUs) << " shaders/sec)" << std::endl;
}

void TestRomCmbVShaderTranslation() {
    std::cout << "[TEST] RomCmbVShaderTranslation (Actual ROM CmbVShader.shbin)... ";

    const std::string shbinPath = "build/CmbVShader.shbin";
    std::ifstream in(shbinPath, std::ios::binary);
    if (!in) {
        std::cout << "SKIPPED (build/CmbVShader.shbin not present)" << std::endl;
        return;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (data.size() < 32 || data[0] != 'D' || data[1] != 'V' || data[2] != 'L' || data[3] != 'B') {
        std::cout << "SKIPPED (invalid DVLB magic)" << std::endl;
        return;
    }

    uint32_t numPrograms = *reinterpret_cast<const uint32_t*>(&data[4]);
    uint32_t dvlpOffset = 8 + numPrograms * 4;
    uint32_t codeOffset = *reinterpret_cast<const uint32_t*>(&data[dvlpOffset + 8]);
    uint32_t codeWords = *reinterpret_cast<const uint32_t*>(&data[dvlpOffset + 12]);
    uint32_t swzOffset = *reinterpret_cast<const uint32_t*>(&data[dvlpOffset + 16]);
    uint32_t swzCount = *reinterpret_cast<const uint32_t*>(&data[dvlpOffset + 20]);

    const uint32_t* codePtr = reinterpret_cast<const uint32_t*>(&data[dvlpOffset + codeOffset]);
    std::vector<uint32_t> program(codePtr, codePtr + codeWords);

    std::vector<uint32_t> swizzles;
    swizzles.reserve(swzCount);
    for (uint32_t i = 0; i < swzCount; ++i) {
        const uint32_t* swzEntry = reinterpret_cast<const uint32_t*>(&data[dvlpOffset + swzOffset + i * 8]);
        swizzles.push_back(swzEntry[0]);
    }

    PicaVertexTranslator translator;
    std::vector<uint32_t> spirv;
    std::string err;
    bool ok = translator.Translate(program, swizzles, spirv, &err);
    if (!ok) {
        std::cerr << "CmbVShader translation failed: " << err << std::endl;
    }
    assert(ok);
    assert(!spirv.empty());
    assert(spirv[0] == 0x07230203U);
    assert(ValidateWithSpirvVal(spirv));

    std::cout << "PASSED (SPIR-V valid with 0 errors! words: " << spirv.size() << ")" << std::endl;
}

} // namespace

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " PicaToSpirv Translator Test Suite     " << std::endl;
    std::cout << "========================================" << std::endl;

    TestSpirvBuilderBasics();
    TestPicaVertexSimpleArithmetic();
    TestPicaVertexSwizzlesAndMasks();
    TestPicaVertexMultiplyAddAndSpecialOps();
    TestBenchmarkThroughput();

    TestPicaFragmentBasics();
    TestPicaFragmentComplexFeatures();
    TestBenchmarkFragmentThroughput();

    TestRomCmbVShaderTranslation();

    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}
