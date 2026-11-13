#pragma once

#include "spirv_builder.h"
#include <nihstro/shader_bytecode.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace PicaToSpirv {

struct PicaVertexShaderConfig {
    uint32_t descriptorSet = 0;
    uint32_t uniformBinding = 0;
    bool flipViewport = false;
    bool emulateDepth = true;
    bool sanitizeMul = false;
    uint32_t mainOffset = 0;
    uint32_t spirvVersion = kSpirvVersion1_0;
};

class PicaVertexTranslator final {
public:
    explicit PicaVertexTranslator(const PicaVertexShaderConfig& config = {});

    /**
     * Translates PICA200 vertex shader bytecode and swizzle tables directly
     * into a valid Khronos SPIR-V binary module.
     *
     * @param programCode 32-bit PICA instruction words.
     * @param swizzleData 32-bit PICA swizzle descriptor words.
     * @param outSpirv Output vector containing the generated SPIR-V binary words.
     * @param errorLog Optional string pointer to receive error details on failure.
     * @return true if translation succeeded, false otherwise.
     */
    bool Translate(
        std::span<const uint32_t> programCode,
        std::span<const uint32_t> swizzleData,
        std::vector<uint32_t>& outSpirv,
        std::string* errorLog = nullptr);

private:
    PicaVertexShaderConfig mConfig;
};

} // namespace PicaToSpirv

