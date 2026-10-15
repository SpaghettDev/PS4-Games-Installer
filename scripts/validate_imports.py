"""Check the generated Orbis ELF's packed launcher library/module imports."""
import base64
import hashlib
from pathlib import Path
import struct
import sys


def launcher_imports(path):
    data = Path(path).read_bytes()
    if data[:6] != b"\x7fELF\x02\x01":
        raise ValueError("Expected a little-endian ELF64")
    offset = struct.unpack_from("<Q", data, 32)[0]
    size, count = struct.unpack_from("<HH", data, 54)
    headers = [struct.unpack_from("<II6Q", data, offset + i * size) for i in range(count)]
    base = next(p[2] for p in headers if p[0] == 0x61000000)
    dynamic = next(p for p in headers if p[0] == 2)
    tags = [struct.unpack_from("<QQ", data, i) for i in range(dynamic[2], dynamic[2] + dynamic[5], 16)]
    values = dict(tags)
    strings_offset = base + values[0x61000035]
    strings = data[strings_offset:strings_offset + values[0x61000037]]

    def string(index):
        end = strings.index(0, index)
        return strings[index:end].decode("ascii")

    libraries = {value >> 48: string(value & 0xFFFFFFFF) for tag, value in tags if tag == 0x61000015}
    modules = {value >> 48: string(value & 0xFFFFFFFF) for tag, value in tags if tag == 0x6100000F}
    alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-"

    def identifier(encoded):
        result = 0
        for char in encoded:
            result = result * 64 + alphabet.index(char)
        return result

    symbols = {}
    start = base + values[0x61000039]
    for pos in range(start, start + values[0x6100003F], 24):
        name, _info, _other, section, _value, _length = struct.unpack_from("<IBBHQQ", data, pos)
        if name and section == 0:
            parts = string(name).split("#")
            if len(parts) == 3:
                symbols[parts[0]] = (libraries[identifier(parts[1])], modules[identifier(parts[2])])

    result = {}
    salt = bytes.fromhex("518D64A635DED8C1E6B039B1C3E55230")
    for name in ("sceLncUtilInitialize", "sceLncUtilLaunchApp"):
        digest = hashlib.sha1(name.encode("ascii") + salt).digest()[:8][::-1]
        nid = base64.b64encode(digest).decode("ascii").rstrip("=").replace("/", "-")
        result[name] = symbols.get(nid)
        if result[name] != ("libSceLncUtil", "libSceSystemService"):
            raise ValueError(f"Incorrect packed launcher import: {name}: {result[name]}")
    return result


if __name__ == "__main__":
    try:
        launcher_imports(sys.argv[1])
    except (ValueError, KeyError, IndexError, StopIteration, struct.error) as error:
        raise SystemExit(f"Launcher import validation failed: {error}") from None
    print("Packed launcher imports verified: LncUtil library / SystemService module")
