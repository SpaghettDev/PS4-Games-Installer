"""Verify real package metadata keeps native IME buttons aligned with the app."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

PROJECT = Path(__file__).resolve().parents[1]


def read_sfo(path):
    data = path.read_bytes()
    magic, version, keys, values, count = struct.unpack_from("<4s4I", data)
    if magic != b"\0PSF" or version != 0x101:
        raise ValueError("Invalid SFO")
    result = {}
    for i in range(count):
        key, kind, length, capacity, offset = struct.unpack_from("<HHIII", data, 20 + 16 * i)
        name = data[keys + key:].split(b"\0", 1)[0].decode()
        value = data[values + offset:values + offset + length]
        result[name] = struct.unpack("<I", value)[0] if kind == 0x404 else value.rstrip(b"\0").decode()
    return result


class MetadataRegression(unittest.TestCase):
    def test_native_keyboard_uses_cross_confirm(self):
        sdk = os.environ["OO_PS4_TOOLCHAIN"]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ("assets", "third_party", "LICENSE", "THIRD_PARTY.md"):
                (root / name).symlink_to(PROJECT / name)
            (root / "build/pkg").mkdir(parents=True)
            (root / "build/pkg/eboot.bin").write_bytes(b"fixture executable")
            result = subprocess.run([sys.executable, str(PROJECT / "scripts/metadata.py"), sdk,
                                     "9", "IV0000-PGIN00001_00-PS4GAMEINSTALLER", "PGIN00001"],
                                    cwd=root, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            sfo = read_sfo(root / "build/pkg/sce_sys/param.sfo")
            self.assertEqual(sfo["ATTRIBUTE"] & 0x02, 0x02, "Common dialogs must confirm with Cross")
            self.assertEqual(sfo["ATTRIBUTE"] & 0x20, 0, "System assignment must not override app controls")
            self.assertEqual(sfo["ATTRIBUTE2"], 2)


if __name__ == "__main__":
    unittest.main()
