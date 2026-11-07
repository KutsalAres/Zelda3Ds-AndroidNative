#!/usr/bin/env python3
"""Runner that applies the OoT3D PT-BR translation patch to romfs.bin, process-manifest.json, and atlas_overrides.o3tu."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys

def main() -> int:
    work_dir = Path("/tmp/ptbr_device_patch").resolve()
    trad_root = Path("/home/anthero/OoT3D-PTBR-TriAevum/extracted_translation/TraducaoCompleta/citra/romfs").resolve()
    installer_script = Path("/home/anthero/OoT3D-PTBR-TriAevum/extracted_translation/Instalador_Traducao_PTBR_OoT3D_V4.py").resolve()

    if not work_dir.is_dir():
        print(f"ERRO: Diretório de trabalho não encontrado: {work_dir}", file=sys.stderr)
        return 1
    if not trad_root.is_dir():
        print(f"ERRO: TraducaoCompleta não encontrada: {trad_root}", file=sys.stderr)
        return 1

    # Configure structure expected by Instalador_Traducao_PTBR_OoT3D_V4
    # 1. TriAevum.launch.json
    launch_path = work_dir / "TriAevum.launch.json"
    launch_path.write_text(json.dumps({
        "format": "oot3d_native_game_launch_profile_v1",
        "arguments": [
            "--a32-process-manifest",
            "${profile_dir}/process-manifest.json"
        ]
    }, indent=2), encoding="utf-8")

    # 2. data/mods/topscreen/atlas_overrides.o3tu
    topscreen_dir = work_dir / "data" / "mods" / "topscreen"
    topscreen_dir.mkdir(parents=True, exist_ok=True)
    target_o3tu = topscreen_dir / "atlas_overrides.o3tu"
    if not target_o3tu.exists() and (work_dir / "atlas_overrides.o3tu").is_file():
        shutil.copy2(work_dir / "atlas_overrides.o3tu", target_o3tu)

    # Load module
    spec = importlib.util.spec_from_file_location("ptbr_v4", installer_script)
    if spec is None or spec.loader is None:
        print("ERRO ao carregar instalador V4", file=sys.stderr)
        return 1
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)

    # Override module globals
    mod.BASE = work_dir
    mod.LAUNCH = launch_path
    mod.TRAD_ROOT = trad_root

    print(">>> Executando instalação e validação das 126 operações de tradução...")
    mod.install()

    # Copy patched o3tu back to work_dir root for push
    if target_o3tu.is_file():
        shutil.copy2(target_o3tu, work_dir / "atlas_overrides.o3tu")

    # Update game_language.json with Portuguese
    lang_path = work_dir / "game_language.json"
    lang_doc = {
        "format": "triaevum_game_language_v1",
        "available": [
            {"code": "pt", "label": "Português (Brasil)", "system_id": 9},
            {"code": "en", "label": "English", "system_id": 1},
            {"code": "fr", "label": "Francais", "system_id": 2},
            {"code": "es", "label": "Espanol", "system_id": 5}
        ],
        "selected": "pt"
    }
    lang_path.write_text(json.dumps(lang_doc, indent=2), encoding="utf-8")
    print(">>> game_language.json atualizado com selected: 'pt'.")

    # Tag file pt_br.tag
    tag_path = work_dir / "pt_br.tag"
    tag_path.write_text("OoT3D PT-BR TriAevum v2.0 - Projeto Zelda Brasil\n", encoding="utf-8")
    print(">>> pt_br.tag gerada.")

    print("\n✅ Patch da tradução concluído com sucesso!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
