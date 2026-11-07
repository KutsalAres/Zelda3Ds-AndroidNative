#!/usr/bin/env python3
"""
Valida, inspeciona e aplica o pacote de tradução em Português do Brasil (PT-BR)
na RomFS do The Legend of Zelda: Ocarina of Time 3D para o TriAevum (Android e PC).
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import struct
import sys
import zipfile

# 23 texturas de menu localizadas do OoT3D
EXPECTED_MENU_TEXTURES = {
    "cam_interface00.ctxb",
    "common_bg00.ctxb",
    "common_bg01.ctxb",
    "fieldmap_cloud_parts00.ctxb",
    "hud_all00.ctxb",
    "hud_menu_title00.ctxb",
    "icon_item_menu00.ctxb",
    "menu_cursor00.ctxb",
    "menu_equip_parts00.ctxb",
    "menu_file_select_parts00.ctxb",
    "menu_file_select_parts01.ctxb",
    "menu_hint_movie_parts00.ctxb",
    "menu_item_parts00.ctxb",
    "menu_item_parts01.ctxb",
    "menu_map_parts00.ctxb",
    "menu_map_parts01.ctxb",
    "menu_okarina_parts00.ctxb",
    "menu_option_parts00.ctxb",
    "menu_top_map00.ctxb",
    "menu_top_parts00.ctxb",
    "name_entry00_parts00.ctxb",
    "name_entry00_parts01.ctxb",
    "num_all00.ctxb",
}


def inspect_qm_header(qm_bytes: bytes) -> dict[str, int]:
    if len(qm_bytes) < 16:
        raise ValueError("Arquivo QM truncado ou inválido (menos de 16 bytes)")
    magic, version, count, reserved = struct.unpack_from("<4sIII", qm_bytes, 0)
    if magic[:2] != b"QM":
        raise ValueError(f"Magic QM inesperado: {magic!r}")
    return {"version": version, "count": count, "reserved": reserved}


def inspect_qbf_header(qbf_bytes: bytes) -> dict[str, int | str]:
    if len(qbf_bytes) < 16:
        raise ValueError("Arquivo QBF truncado")
    magic = qbf_bytes[:4].decode("ascii", errors="replace")
    width, height, glyphs = struct.unpack_from("<HHH", qbf_bytes, 4)
    return {"magic": magic, "width": width, "height": height, "glyphs": glyphs}


def inspect_translation_pack(pack_root: Path) -> dict[str, object]:
    """Inspeciona os arquivos de uma pasta ou ZIP de tradução PT-BR."""
    report: dict[str, object] = {
        "valid": False,
        "qm_found": [],
        "qbf_found": [],
        "ctxb_found": [],
        "misc_found": [],
        "details": {},
    }

    all_files = list(pack_root.rglob("*")) if pack_root.is_dir() else []
    for file_path in all_files:
        if not file_path.is_file():
            continue
        rel = file_path.relative_to(pack_root).as_posix().lower()
        name = file_path.name.lower()

        if name.endswith(".qm"):
            report["qm_found"].append(rel)
            try:
                report["details"][rel] = inspect_qm_header(file_path.read_bytes())
            except Exception as e:
                report["details"][rel] = {"error": str(e)}

        elif name.endswith(".qbf"):
            report["qbf_found"].append(rel)
            try:
                report["details"][rel] = inspect_qbf_header(file_path.read_bytes())
            except Exception as e:
                report["details"][rel] = {"error": str(e)}

        elif name.endswith(".ctxb"):
            report["ctxb_found"].append(rel)

        elif any(name.endswith(ext) for ext in (".qly", ".qsp", ".qan", ".qcl", ".qbr", ".qhm")):
            report["misc_found"].append(rel)

    report["valid"] = len(report["qm_found"]) > 0 or len(report["ctxb_found"]) > 0
    return report


def apply_translation_to_romfs_directory(
    romfs_root: Path,
    pack_root: Path,
    target_menu_dir: str = "02_EU_ENGLISH",
) -> dict[str, int]:
    """
    Aplica os arquivos de tradução em uma pasta extraída de RomFS.
    Substitui diálogos (.qm), fontes (.qbf), texturas de menus (.ctxb) e layouts.
    """
    if not romfs_root.is_dir():
        raise FileNotFoundError(f"Diretório RomFS não encontrado: {romfs_root}")

    stats = {"qm_copied": 0, "qbf_copied": 0, "ctxb_copied": 0, "misc_copied": 0}

    # 1. Copiar diálogos QM
    for qm_candidate in pack_root.rglob("*.qm"):
        if qm_candidate.is_file():
            # Copiar para message/eu e message/us
            for region in ("eu", "us"):
                target = romfs_root / "message" / region / f"{region}.qm"
                if target.parent.exists():
                    shutil.copy2(qm_candidate, target)
                    stats["qm_copied"] += 1

    # 2. Copiar fontes QBF
    for qbf_candidate in pack_root.rglob("*.qbf"):
        if qbf_candidate.is_file():
            name = qbf_candidate.name
            for region in ("eu", "us"):
                target = romfs_root / "message" / region / name
                if target.parent.exists():
                    shutil.copy2(qbf_candidate, target)
                    stats["qbf_copied"] += 1
            # Também no raiz de message se for sys8
            sys_target = romfs_root / "message" / name
            if sys_target.parent.exists():
                shutil.copy2(qbf_candidate, sys_target)
                stats["qbf_copied"] += 1

    # 3. Copiar texturas de menu CTXB
    target_menu_path = romfs_root / "menu" / target_menu_dir
    target_menu_path.mkdir(parents=True, exist_ok=True)

    # Também garantir cópia para pasta dedicada 10_PT_PORTUGUESE
    pt_menu_path = romfs_root / "menu" / "10_PT_PORTUGUESE"
    pt_menu_path.mkdir(parents=True, exist_ok=True)

    for ctxb_candidate in pack_root.rglob("*.ctxb"):
        if ctxb_candidate.is_file():
            name = ctxb_candidate.name
            shutil.copy2(ctxb_candidate, target_menu_path / name)
            shutil.copy2(ctxb_candidate, pt_menu_path / name)
            stats["ctxb_copied"] += 1

    # 4. Copiar layouts e misc
    for misc_candidate in pack_root.rglob("*"):
        if misc_candidate.is_file() and misc_candidate.suffix in (".qly", ".qsp", ".qan", ".qcl"):
            name = misc_candidate.name
            for region in ("eu", "us"):
                target = romfs_root / "misc" / region / target_menu_dir / name
                if target.parent.exists():
                    shutil.copy2(misc_candidate, target)
                    stats["misc_copied"] += 1

    # 5. Adicionar marcador de detecção PT-BR
    tag_path = romfs_root / "menu" / "pt_br.tag"
    tag_path.write_text("TriAevum Português do Brasil Translation Tag\n", encoding="utf-8")

    return stats


def update_game_language_config(config_dir: Path, select_pt: bool = True) -> Path:
    """Atualiza ou cria config/game_language.json com suporte ao Português."""
    config_dir.mkdir(parents=True, exist_ok=True)
    lang_file = config_dir / "game_language.json"

    data = {
        "format": "triaevum_game_language_v1",
        "available": [
            {"code": "en", "label": "English", "system_id": 1},
            {"code": "pt", "label": "Português (Brasil)", "system_id": 9},
            {"code": "es", "label": "Español", "system_id": 5},
            {"code": "fr", "label": "Français", "system_id": 2},
            {"code": "de", "label": "Deutsch", "system_id": 3},
            {"code": "it", "label": "Italiano", "system_id": 4},
        ],
        "selected": "pt" if select_pt else "en",
    }

    if lang_file.exists():
        try:
            existing = json.loads(lang_file.read_text(encoding="utf-8"))
            available = existing.get("available", [])
            codes = {item.get("code") for item in available if isinstance(item, dict)}
            if "pt" not in codes:
                available.append({"code": "pt", "label": "Português (Brasil)", "system_id": 9})
                existing["available"] = available
            if select_pt:
                existing["selected"] = "pt"
            data = existing
        except Exception:
            pass

    lang_file.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return lang_file


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Aplica pacote de tradução PT-BR na RomFS do OoT3D para TriAevum"
    )
    parser.add_argument(
        "--romfs-dir",
        type=Path,
        required=True,
        help="Caminho do diretório RomFS extraído (ex: /inputs/romfs)",
    )
    parser.add_argument(
        "--translation-dir",
        type=Path,
        required=True,
        help="Caminho do pacote com a tradução PT-BR (pasta com .qm, .ctxb, .qbf)",
    )
    parser.add_argument(
        "--config-dir",
        type=Path,
        default=None,
        help="Caminho da pasta de configuração (onde fica game_language.json)",
    )
    parser.add_argument(
        "--target-menu-dir",
        type=str,
        default="02_EU_ENGLISH",
        help="Diretório de menu que receberá os menus em PT-BR (padrão: 02_EU_ENGLISH)",
    )
    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="Apenas verifica e relata o conteúdo do pacote de tradução",
    )

    args = parser.parse_args()

    print(f"Inspecionando pacote de tradução em: {args.translation_dir}")
    report = inspect_translation_pack(args.translation_dir)
    print(f"  Arquivos QM encontrados: {len(report['qm_found'])}")
    print(f"  Fontes QBF encontradas: {len(report['qbf_found'])}")
    print(f"  Texturas CTXB de menu: {len(report['ctxb_found'])}")
    print(f"  Arquivos misc/layout: {len(report['misc_found'])}")

    if not report["valid"]:
        print("ERRO: Nenhum arquivo de tradução válido (.qm ou .ctxb) foi encontrado no pacote.")
        return 1

    if args.verify_only:
        print("Modo de verificação concluído.")
        return 0

    print(f"Aplicando tradução na RomFS: {args.romfs_dir}")
    stats = apply_translation_to_romfs_directory(
        args.romfs_dir, args.translation_dir, args.target_menu_dir
    )
    print(f"  Diálogos copiados: {stats['qm_copied']}")
    print(f"  Fontes copiadas: {stats['qbf_copied']}")
    print(f"  Texturas de menu copiadas: {stats['ctxb_copied']}")
    print(f"  Layouts copiados: {stats['misc_copied']}")

    if args.config_dir:
        config_path = update_game_language_config(args.config_dir, select_pt=True)
        print(f"  Configuração atualizada: {config_path}")

    print("Tradução em Português do Brasil aplicada com sucesso!")
    return 0


if __name__ == "__main__":
    sys.exit(main())

