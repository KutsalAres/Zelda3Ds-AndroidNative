#!/usr/bin/env python3
"""Testes unitários para o aplicador de tradução PT-BR."""

import json
from pathlib import Path
import struct
import tempfile
import unittest

from apply_ptbr_translation import (
    apply_translation_to_romfs_directory,
    inspect_qm_header,
    inspect_qbf_header,
    inspect_translation_pack,
    update_game_language_config,
)


class TestApplyPtbrTranslation(unittest.TestCase):
    def test_inspect_qm_header_valid(self):
        qm_data = bytearray(16)
        struct.pack_into("<4sIII", qm_data, 0, b"QM\0\0", 4, 2510, 0)
        info = inspect_qm_header(bytes(qm_data))
        self.assertEqual(info["version"], 4)
        self.assertEqual(info["count"], 2510)

    def test_inspect_qm_header_invalid(self):
        with self.assertRaises(ValueError):
            inspect_qm_header(b"SHORT")
        with self.assertRaises(ValueError):
            inspect_qm_header(b"INVALID_HEADER_X")

    def test_inspect_qbf_header(self):
        qbf_data = bytearray(16)
        qbf_data[:4] = b"QBF1"
        struct.pack_into("<HHH", qbf_data, 4, 199, 208, 120)
        info = inspect_qbf_header(bytes(qbf_data))
        self.assertEqual(info["magic"], "QBF1")
        self.assertEqual(info["width"], 199)
        self.assertEqual(info["height"], 208)
        self.assertEqual(info["glyphs"], 120)

    def test_inspect_translation_pack(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "message/eu").mkdir(parents=True)
            (root / "menu/02_EU_ENGLISH").mkdir(parents=True)

            qm = bytearray(16)
            struct.pack_into("<4sIII", qm, 0, b"QM\0\0", 4, 2510, 0)
            (root / "message/eu/eu.qm").write_bytes(qm)
            (root / "menu/02_EU_ENGLISH/hud_all00.ctxb").write_bytes(b"CTXB_DUMMY")

            report = inspect_translation_pack(root)
            self.assertTrue(report["valid"])
            self.assertEqual(len(report["qm_found"]), 1)
            self.assertEqual(len(report["ctxb_found"]), 1)

    def test_apply_translation_to_romfs_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            romfs = work / "romfs"
            pack = work / "ptbr_pack"

            # Setup romfs structure
            (romfs / "message/eu").mkdir(parents=True)
            (romfs / "menu/02_EU_ENGLISH").mkdir(parents=True)
            (romfs / "message/eu/eu.qm").write_bytes(b"ORIGINAL_QM")

            # Setup pack structure
            pack.mkdir(parents=True)
            (pack / "eu.qm").write_bytes(b"TRANSLATED_QM")
            (pack / "hud_all00.ctxb").write_bytes(b"TRANSLATED_HUD")

            stats = apply_translation_to_romfs_directory(romfs, pack, "02_EU_ENGLISH")
            self.assertGreater(stats["qm_copied"], 0)
            self.assertGreater(stats["ctxb_copied"], 0)

            # Check files were replaced/created
            self.assertEqual((romfs / "message/eu/eu.qm").read_bytes(), b"TRANSLATED_QM")
            self.assertEqual((romfs / "menu/02_EU_ENGLISH/hud_all00.ctxb").read_bytes(), b"TRANSLATED_HUD")
            self.assertEqual((romfs / "menu/10_PT_PORTUGUESE/hud_all00.ctxb").read_bytes(), b"TRANSLATED_HUD")
            self.assertTrue((romfs / "menu/pt_br.tag").exists())

    def test_update_game_language_config(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_dir = Path(tmp)
            conf = update_game_language_config(config_dir, select_pt=True)
            self.assertTrue(conf.exists())
            data = json.loads(conf.read_text(encoding="utf-8"))
            self.assertEqual(data["selected"], "pt")
            available_codes = [c["code"] for c in data["available"]]
            self.assertIn("pt", available_codes)
            self.assertIn("en", available_codes)


if __name__ == "__main__":
    unittest.main()
