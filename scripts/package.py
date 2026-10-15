"""Serialize packaging and commit a revision only after a validated package exists."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
CONTENT_ID = "IV0000-PGIN00001_00-PS4GAMEINSTALLER"


def fingerprint(root=ROOT):
    digest = hashlib.sha256()
    files = [root / name for name in ("Makefile", "Dockerfile", "LICENSE", "THIRD_PARTY.md")]
    for directory in ("src", "assets", "scripts", "third_party"):
        files.extend(p for p in (root / directory).rglob("*")
                     if p.is_file() and "__pycache__" not in p.parts and not p.name.endswith(".tmp"))
    for path in sorted(files):
        digest.update(str(path.relative_to(root)).encode() + b"\0" + path.read_bytes() + b"\0")
    return digest.hexdigest()


def next_revision(state, input_hash):
    return state["revision"] if state["input_hash"] == input_hash else max(2, state["revision"] + 1)


def atomic_json(path, data):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, indent=2) + "\n")
    os.replace(temporary, path)


def main():
    os.chdir(ROOT)
    # Kept outside build/ so cleaning cannot invalidate a held packaging lock.
    with open(ROOT / ".package.lock", "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state_path = ROOT / "revision.json"
        state = json.loads(state_path.read_text())
        input_hash = fingerprint()
        revision = next_revision(state, input_hash)
        output = ROOT / "dist" / f"PS4Game_installer-r{revision}.pkg"
        tool = Path(os.environ["OO_PS4_TOOLCHAIN"]) / "bin/linux/PkgTool.Core"
        if "--validate" in sys.argv:
            if state["input_hash"] != input_hash or not output.exists():
                raise SystemExit("Build the current sources with make first")
            subprocess.run([str(tool), "pkg_validate", str(output)], check=True)
            return
        if state["input_hash"] == input_hash and output.exists():
            print(f"Up to date: {output.relative_to(ROOT)}")
            return
        generated = ROOT / "build/generated/revision.hpp"
        generated.parent.mkdir(parents=True, exist_ok=True)
        header = f'#pragma once\n#define PSGI_REVISION {revision}\n#define PSGI_VERSION "r{revision}"\n'
        if not generated.exists() or generated.read_text() != header:
            generated.write_text(header)
        subprocess.run(["make", "--no-print-directory", "-j4", "package-internal", f"REVISION={revision}"], check=True)
        if fingerprint() != input_hash:
            raise SystemExit("Build inputs changed during packaging; retry make (revision was not consumed)")
        output.parent.mkdir(exist_ok=True)
        staged = output.with_suffix(".pkg.tmp")
        shutil.copyfile(ROOT / "build/package" / (CONTENT_ID + ".pkg"), staged)
        os.replace(staged, output)
        atomic_json(output.with_suffix(".json"), {"revision": revision, "input_hash": input_hash,
                    "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
                    "title_id": "PGIN00001", "hardware_tested": False})
        atomic_json(state_path, {"revision": revision, "input_hash": input_hash})
        print(f"Built {output.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
