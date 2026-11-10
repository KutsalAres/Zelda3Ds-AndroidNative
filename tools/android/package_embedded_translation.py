#!/usr/bin/env python3
"""
Gera o pacote de tradução PT-BR embutido (assets/translation/oot3d_ptbr_pack.zip)
e os atlas overrides de TopScreen (original e PT-BR) para inclusão no APK Android.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path
import struct
import sys
import zipfile

REPO_ROOT = Path(__file__).resolve().parents[2]
ASSETS_DIR = REPO_ROOT / "ports" / "android" / "app" / "src" / "main" / "assets"
TRANSLATION_ZIP = ASSETS_DIR / "translation" / "oot3d_ptbr_pack.zip"
ATLAS_ORIG = ASSETS_DIR / "game" / "atlas_overrides.original.o3tu"
ATLAS_PT = ASSETS_DIR / "game" / "atlas_overrides.pt.o3tu"
ATLAS_ACTIVE = ASSETS_DIR / "game" / "atlas_overrides.o3tu"

PTBR_ORIGINAL_HASH = 0xAC72735EF08ABEAB
ENGLISH_ORIGINAL_HASH = 0xB216011E1F24749C


def ensure_atlas_overrides() -> None:
    if not ATLAS_ACTIVE.is_file():
        print(f"AVISO: {ATLAS_ACTIVE} não encontrado. Pulando geração de atlas overrides.")
        return

    raw = ATLAS_ACTIVE.read_bytes()
    if not ATLAS_ORIG.is_file():
        ATLAS_ORIG.write_bytes(raw)
        print(f"✅ Gerado {ATLAS_ORIG}")

    if not ATLAS_PT.is_file():
        out = bytearray(raw)
        # Entry 2 (menu_top) header offset is 294968
        struct.pack_into("<Q", out, 294968, PTBR_ORIGINAL_HASH)
        ATLAS_PT.write_bytes(out)
        print(f"✅ Gerado {ATLAS_PT}")


def package_translation_zip() -> None:
    if TRANSLATION_ZIP.is_file() and TRANSLATION_ZIP.stat().st_size > 10_000_000:
        print(f"ℹ️  {TRANSLATION_ZIP.name} já existe ({TRANSLATION_ZIP.stat().st_size / (1024*1024):.2f} MB).")
        return

    installer_script = Path("/home/anthero/OoT3D-PTBR-TriAevum/extracted_translation/Instalador_Traducao_PTBR_OoT3D_V4.py")
    if not installer_script.is_file():
        print(f"AVISO: Instalador V4 não encontrado em {installer_script}. Não foi possível regenerar zip.")
        return

    spec = importlib.util.spec_from_file_location("ptbr_v4", installer_script)
    if spec is None or spec.loader is None:
        raise RuntimeError("Falha ao carregar instalador V4")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)

    items = mod.collect_v4_items()
    TRANSLATION_ZIP.parent.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(TRANSLATION_ZIP, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for item in items:
            payload = mod.prepare_payload(item)
            zf.writestr(item["target"], payload)

    print(f"✅ Pacote de tradução criado: {TRANSLATION_ZIP} ({TRANSLATION_ZIP.stat().st_size / (1024*1024):.2f} MB)")


def main() -> int:
    ensure_atlas_overrides()
    package_translation_zip()
    return 0


if __name__ == "__main__":
    sys.exit(main())

