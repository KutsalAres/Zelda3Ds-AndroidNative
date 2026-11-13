#!/usr/bin/env python3
"""
tools/oot3d/rom_shader_extractor.py
Extrai os shaders e inventário de materiais diretamente da RomFS do OOT3D (Rom/romfs.bin ou .3ds/.cci)
e gera o inventário unificado para a compilação AOT (PicaToSpirv).
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def find_default_romfs(repo_root: Path) -> Path | None:
    candidates = [
        repo_root / "Rom/romfs.bin",
        repo_root / "Rom",
        repo_root.parent / "Rom/romfs.bin",
    ]
    for c in candidates:
        if c.is_file() and c.stat().st_size > 1024 * 1024:
            return c
        if c.is_dir() and (c / "romfs.bin").is_file():
            return c / "romfs.bin"
    return None


class RomFsReader:
    def __init__(self, romfs_path: Path):
        self.path = romfs_path
        self.stream = romfs_path.open("rb")
        self.stream.seek(0)
        magic = self.stream.read(4)
        self.base = 0x1000 if magic == b"IVFC" else 0
        self.stream.seek(self.base)
        self.header = struct.unpack("<10I", self.stream.read(40))
        if self.header[0] != 40:
            raise ValueError(f"Cabeçalho de RomFS inválido em {romfs_path}")

        self.dirs_offset = self.base + self.header[3]
        self.dirs_len = self.header[4]
        self.files_offset = self.base + self.header[7]
        self.files_len = self.header[8]
        self.data_base = self.base + self.header[9]

        self.stream.seek(self.dirs_offset)
        self.dirs_table = self.stream.read(self.dirs_len)
        self.stream.seek(self.files_offset)
        self.files_table = self.stream.read(self.files_len)

    def close(self):
        self.stream.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def list_files(self) -> list[tuple[str, int, int]]:
        """Retorna lista de tuplas (caminho_relativo, offset_payload, tamanho)"""
        results = []
        pending = [(0, "")]
        while pending:
            offset, prefix = pending.pop()
            parent, sibling, child, file, chain, _ = struct.unpack_from("<6I", self.dirs_table, offset)
            name_len = struct.unpack_from("<I", self.dirs_table, offset + 20)[0]
            dname = self.dirs_table[offset + 24 : offset + 24 + name_len].decode("utf-16-le", errors="ignore")
            full_dir = (prefix + "/" + dname) if prefix else dname
            if sibling != 0xFFFFFFFF:
                pending.append((sibling, prefix))
            if child != 0xFFFFFFFF:
                pending.append((child, full_dir))
            while file != 0xFFFFFFFF:
                parent_f, following, payload, length, chain_f, _ = struct.unpack_from("<IIQQII", self.files_table, file)
                fname_len = struct.unpack_from("<I", self.files_table, file + 28)[0]
                fname = self.files_table[file + 32 : file + 32 + fname_len].decode("utf-16-le", errors="ignore")
                full_path = (full_dir + "/" + fname) if full_dir else fname
                results.append((full_path, payload, length))
                file = following
        return results

    def read_file(self, payload_offset: int, length: int) -> bytes:
        self.stream.seek(self.data_base + payload_offset)
        return self.stream.read(length)


def extract_rom_shaders(
    romfs_path: Path,
    output_shbin: Path,
    output_profile: Path | None = None,
) -> tuple[bytes, bytes | None]:
    output_shbin.parent.mkdir(parents=True, exist_ok=True)
    with RomFsReader(romfs_path) as romfs:
        files = romfs.list_files()
        cmb_vshader_data = None
        profile_data = None
        for path, offset, length in files:
            norm_name = path.split("/")[-1].lower()
            if norm_name == "cmbvshader.shbin":
                cmb_vshader_data = romfs.read_file(offset, length)
                output_shbin.write_bytes(cmb_vshader_data)
                print(f"✅ Extraído CmbVShader.shbin: {len(cmb_vshader_data):,} bytes -> {output_shbin}")
            elif norm_name == "profile.shbin" and output_profile:
                profile_data = romfs.read_file(offset, length)
                output_profile.write_bytes(profile_data)
                print(f"✅ Extraído profile.shbin: {len(profile_data):,} bytes -> {output_profile}")

        if not cmb_vshader_data:
            raise FileNotFoundError("CmbVShader.shbin não foi encontrado na RomFS.")
        return cmb_vshader_data, profile_data


def build_unified_inventory(
    profiles_dir: Path,
    output_inventory: Path,
) -> int:
    """Mescla os perfis de pipeline e fragmentos observados da PICA num inventário unificado."""
    output_inventory.parent.mkdir(parents=True, exist_ok=True)
    merged_shaders = {}

    for json_file in profiles_dir.glob("*.json"):
        try:
            content = json.loads(json_file.read_text(encoding="utf-8"))
            if "shaders" in content:
                for entry in content["shaders"]:
                    source = entry.get("source", "")
                    stage = entry.get("stage", "fragment")
                    if not source:
                        continue
                    key = (stage, source)
                    if key not in merged_shaders:
                        merged_shaders[key] = entry
                    else:
                        existing = merged_shaders[key]
                        for field in ("canonical_pipeline_ids", "effective_shader_keys", "settings_revisions"):
                            if field in entry:
                                s = set(existing.get(field, []))
                                s.update(entry[field])
                                existing[field] = sorted(list(s))
                        existing["observation_count"] = existing.get("observation_count", 0) + entry.get("observation_count", 1)
        except Exception as e:
            print(f"⚠️ Aviso ao ler perfil {json_file.name}: {e}")

    inventory_data = {
        "descriptor_schema_version": 3,
        "format": "oot3d_pica_effective_shader_inventory_v1",
        "shader_count": len(merged_shaders),
        "shaders": list(merged_shaders.values()),
    }

    output_inventory.write_text(json.dumps(inventory_data, indent=2), encoding="utf-8")
    print(f"✅ Inventário unificado gerado: {len(merged_shaders)} shaders -> {output_inventory}")
    return len(merged_shaders)


def main():
    parser = argparse.ArgumentParser(description="Extrai shaders da RomFS do OOT3D e prepara inventário AOT")
    parser.add_argument("--romfs", type=Path, default=None, help="Caminho para Rom/romfs.bin")
    parser.add_argument("--output-shbin", type=Path, default=Path("build/CmbVShader.shbin"))
    parser.add_argument("--output-profile", type=Path, default=Path("build/profile.shbin"))
    parser.add_argument("--output-inventory", type=Path, default=Path("build/pica_aot_inventory.json"))
    parser.add_argument("--profiles-dir", type=Path, default=None)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    romfs_path = args.romfs or find_default_romfs(repo_root)
    if not romfs_path or not romfs_path.is_file():
        print(f"ERRO: romfs.bin não encontrado. Especifique com --romfs <caminho>.")
        sys.exit(1)

    print(f">>> Lendo RomFS: {romfs_path} ({romfs_path.stat().st_size:,} bytes)...")
    extract_rom_shaders(romfs_path, args.output_shbin, args.output_profile)

    profiles_dir = args.profiles_dir or (repo_root / "tools/oot3d/native_pica_frontend/profiles")
    if profiles_dir.is_dir():
        build_unified_inventory(profiles_dir, args.output_inventory)

    print("🎉 Extração e preparação do inventário concluídas com sucesso!")


if __name__ == "__main__":
    main()

