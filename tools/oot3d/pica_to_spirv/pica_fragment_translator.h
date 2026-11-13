#pragma once

#include "spirv_builder.h"
#include "three_ds_recomp/pica/pica_state.h"
#include "three_ds_recomp/pica/pica_fragment_generator.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PicaToSpirv {

class PicaFragmentTranslator final {
public:
    explicit PicaFragmentTranslator(uint32_t spirvVersion = kSpirvVersion1_0);

    /**
     * Translates PICA200 fragment draw state (TEV stages, combiners, textures, lighting,
     * fog and alpha test) directly into a valid Khronos SPIR-V fragment shader module.
     *
     * @param drawState Decoded PICA draw state containing TEV stages and pipeline configuration.
     * @param config Fragment shader generation configuration (mediump TEV, fog, alpha test, etc.).
     * @param outSpirv Output vector containing the generated SPIR-V binary words.
     * @param errorLog Optional string pointer to receive error details on failure.
     * @return true if translation succeeded, false otherwise.
     */
    bool Translate(
        const ThreeDsRecomp::Pica::PicaDrawState& drawState,
        const ThreeDsRecomp::Pica::PicaFragmentShaderConfig& config,
        std::vector<uint32_t>& outSpirv,
        std::string* errorLog = nullptr);

private:
    uint32_t mSpirvVersion;
};

} // namespace PicaToSpirv

