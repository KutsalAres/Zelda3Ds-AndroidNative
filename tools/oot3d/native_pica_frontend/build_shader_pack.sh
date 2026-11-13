#!/usr/bin/env bash
# ==============================================================================
# Script de Compilação Offline de Shaders PICA para SPIR-V AOT Pack (.o3ps)
# Extrai shaders diretamente da ROM do jogo (romfs.bin) e compila para Vulkan SPIR-V
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"

PROFILES_DIR="$REPO_ROOT/tools/oot3d/native_pica_frontend/profiles"
ASSETS_GAME_DIR="$REPO_ROOT/ports/android/app/src/main/assets/game"
OUTPUT_PACK="$ASSETS_GAME_DIR/oot3d_pica_default.o3ps"
OUTPUT_MANIFEST="$ASSETS_GAME_DIR/oot3d_pica_default_manifest.json"
BUILD_DIR="${TRIAEVUM_BUILD_DIR:-$REPO_ROOT/build}"
EXTRACTOR_SCRIPT="$REPO_ROOT/tools/oot3d/rom_shader_extractor.py"

ROM_INPUT=""
FORCE_REBUILD=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --rom)
            ROM_INPUT="$2"
            shift 2
            ;;
        --force|-f)
            FORCE_REBUILD=1
            shift
            ;;
        *)
            if [ -z "$ROM_INPUT" ]; then
                ROM_INPUT="$1"
            fi
            shift
            ;;
    esac
done

if [ -n "${FORCE_REBUILD_SHADERS:-}" ] && [ "$FORCE_REBUILD_SHADERS" -ne 0 ]; then
    FORCE_REBUILD=1
fi

# Detectar ROM se não especificada
ROMFS_PATH=""
if [ -n "$ROM_INPUT" ]; then
    if [ -f "$ROM_INPUT" ]; then
        if [[ "$ROM_INPUT" == *"romfs.bin"* ]]; then
            ROMFS_PATH="$ROM_INPUT"
        else
            ROM_DIR="$(dirname "$ROM_INPUT")"
            if [ -f "$ROM_DIR/romfs.bin" ]; then
                ROMFS_PATH="$ROM_DIR/romfs.bin"
            fi
        fi
    elif [ -d "$ROM_INPUT" ]; then
        if [ -f "$ROM_INPUT/romfs.bin" ]; then
            ROMFS_PATH="$ROM_INPUT/romfs.bin"
        fi
    fi
fi

if [ -z "$ROMFS_PATH" ]; then
    if [ -f "$REPO_ROOT/Rom/romfs.bin" ]; then
        ROMFS_PATH="$REPO_ROOT/Rom/romfs.bin"
    fi
fi

# Verificar se podemos reutilizar o pacote compilado
if [ -f "$OUTPUT_PACK" ] && [ -f "$OUTPUT_MANIFEST" ] && [ "$FORCE_REBUILD" -eq 0 ]; then
    if [ -z "$ROMFS_PATH" ] || [ "$OUTPUT_PACK" -nt "$ROMFS_PATH" ]; then
        echo "ℹ️  Pacote AOT de shaders já existe e está atualizado: $(du -h "$OUTPUT_PACK" | cut -f1) ($OUTPUT_PACK) - reutilizando."
        exit 0
    fi
fi

echo "======================================================================"
echo "          Compilador AOT de Shaders PICA para Vulkan (SPIR-V)          "
echo "======================================================================"

mkdir -p "$BUILD_DIR"
mkdir -p "$ASSETS_GAME_DIR"

INVENTORY_FILE="$BUILD_DIR/pica_aot_inventory.json"

# 1. Extrair Shaders da ROM
if [ -n "$ROMFS_PATH" ] && [ -f "$ROMFS_PATH" ]; then
    echo ">>> Extraindo shaders PICA da ROM: $ROMFS_PATH..."
    python3 "$EXTRACTOR_SCRIPT" \
        --romfs "$ROMFS_PATH" \
        --output-shbin "$BUILD_DIR/CmbVShader.shbin" \
        --output-profile "$BUILD_DIR/profile.shbin" \
        --output-inventory "$INVENTORY_FILE"
else
    echo "⚠️  ROM não encontrada (procurado em $REPO_ROOT/Rom/romfs.bin). Utilizando profiles padrão..."
    if [ -f "$PROFILES_DIR/oot3d_pica_boot_title_kokiri_pipeline.json" ]; then
        INVENTORY_FILE="$PROFILES_DIR/oot3d_pica_boot_title_kokiri_effective.json"
    else
        echo "ERRO: Nenhum inventário de shaders disponível!"
        exit 1
    fi
fi

# 2. Compilar ferramenta AOT nativa para host se necessário
AOT_COMPILER_BIN="$BUILD_DIR/pass_shader_prepare/oot3d_native_pica_aot_compiler"

if [ ! -f "$AOT_COMPILER_BIN" ] || [ "$FORCE_REBUILD" -eq 1 ]; then
    echo ">>> Compilando ferramenta nativa oot3d_native_pica_aot_compiler..."
    mkdir -p "$BUILD_DIR/pass_shader_prepare"
    cmake -S "$REPO_ROOT/tools/renderer/pass_shader_prepare" \
          -B "$BUILD_DIR/pass_shader_prepare" \
          -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR/pass_shader_prepare" --target oot3d_native_pica_aot_compiler -j"$(nproc 2>/dev/null || echo 4)"
fi

if [ ! -f "$AOT_COMPILER_BIN" ]; then
    echo "ERRO: Falha ao compilar $AOT_COMPILER_BIN!"
    exit 1
fi

# 3. Executar compilação AOT para o pacote de assets
echo ">>> Compilando shaders offline para $OUTPUT_PACK..."
"$AOT_COMPILER_BIN" \
    --inventory "$INVENTORY_FILE" \
    --pack "$OUTPUT_PACK" \
    --manifest "$OUTPUT_MANIFEST" \
    --threads "$(nproc 2>/dev/null || echo 4)"

echo "✅ Shaders offline compilados com sucesso: $(du -h "$OUTPUT_PACK" | cut -f1) ($OUTPUT_PACK)"
