"""Build and upload the current package to the FTP target in the private .env."""
from dataclasses import dataclass
import fcntl
import ftplib
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import uuid

import package

ROOT = Path(__file__).resolve().parents[1]


class DeployError(Exception):
    """A deployment failed; messages must never include credentials."""


@dataclass(frozen=True, repr=False)
class Target:
    host: str
    port: int
    user: str
    password: str
    path: str


def read_target(path):
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except FileNotFoundError:
        raise DeployError("Missing .env: copy .env.example to .env and edit the FTP target") from None
    except (OSError, UnicodeError):
        raise DeployError("Cannot read .env as UTF-8; check its permissions and encoding") from None
    values = {}
    for number, line in enumerate(lines, 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(r"(?:export\s+)?([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(.*)", line)
        if not match:
            raise DeployError(f"Invalid .env assignment on line {number}")
        key, value = match.groups()
        if value.startswith(("'", '"')):
            try:
                lexer = shlex.shlex(value, posix=True)
                lexer.whitespace_split = True
                tokens = list(lexer)
            except ValueError:
                raise DeployError(f"Invalid .env quoting on line {number}") from None
            if len(tokens) != 1:
                raise DeployError(f"Invalid .env value on line {number}")
            value = tokens[0]
        else:
            value = re.split(r"\s+#", value, maxsplit=1)[0].rstrip()
        if any(ord(char) < 32 or ord(char) == 127 for char in value):
            raise DeployError(f"Invalid .env control character on line {number}")
        values[key] = value
    host = values.get("DEPLOY_HOST", "")
    if not host or any(char.isspace() or char in "/\\@?#" for char in host):
        raise DeployError("DEPLOY_HOST must be a hostname or IP address without a URL scheme")
    try:
        port = int(values.get("DEPLOY_PORT", "2121"))
        if not 1 <= port <= 65535:
            raise ValueError()
    except ValueError:
        raise DeployError("DEPLOY_PORT must be an integer from 1 to 65535") from None
    destination = values.get("DEPLOY_PATH", "/data/pkg")
    if not destination.startswith("/") or ".." in destination.split("/"):
        raise DeployError("DEPLOY_PATH must be an absolute FTP directory without '..'")
    return Target(host, port, values.get("DEPLOY_USER", "anonymous"),
                  values.get("DEPLOY_PASS", "anonymous@"), destination)


def current_package(root):
    """Use the committed build record, never an arbitrary old package in dist/."""
    try:
        state = json.loads((root / "revision.json").read_text())
        revision = state["revision"]
        if type(revision) is not int or revision < 1:
            raise ValueError()
        artifact = root / "dist" / f"PS4Game_installer-r{revision}.pkg"
        manifest = json.loads(artifact.with_suffix(".json").read_text())
        if (manifest["revision"] != revision or manifest["input_hash"] != state["input_hash"]
                or state["input_hash"] != package.fingerprint(root)):
            raise ValueError()
        with artifact.open("rb") as stream:
            digest = hashlib.sha256()
            while block := stream.read(1024 * 1024):
                digest.update(block)
            checksum = digest.hexdigest()
        if checksum != manifest["sha256"]:
            raise ValueError()
        return artifact
    except (OSError, ValueError, KeyError, TypeError):
        raise DeployError("Current package or checksum is invalid; rebuild with make before deploying") from None


def upload(artifact, target):
    # A unique temporary name keeps interrupted transfers out of the package list.
    temporary = f".{artifact.name}.{uuid.uuid4().hex}.upload"
    stage = "connecting"
    with ftplib.FTP(timeout=30) as ftp:
        try:
            ftp.connect(target.host, target.port)
            stage = "logging in"
            ftp.login(target.user, target.password)
            stage = "opening DEPLOY_PATH (create it on the PS4 if missing)"
            ftp.cwd(target.path)
            stage = "uploading"
            with artifact.open("rb") as source:
                ftp.storbinary(f"STOR {temporary}", source, blocksize=256 * 1024)
            stage = "checking remote size"
            # Some PS4 FTP servers omit SIZE; the completed STOR reply still
            # confirms the transfer in that case.
            try:
                remote_size = ftp.size(temporary)
            except ftplib.error_perm as error:
                if not str(error).startswith(("500", "502", "504")):
                    raise
                remote_size = None
            if remote_size is not None and remote_size != artifact.stat().st_size:
                raise DeployError("Remote package size does not match; final filename was not published")
            stage = "publishing package (FTP server must support rename)"
            ftp.rename(temporary, artifact.name)
        except (OSError, EOFError, ftplib.Error, DeployError) as error:
            try:
                ftp.delete(temporary)
            except (OSError, EOFError, ftplib.Error):
                pass
            if isinstance(error, DeployError):
                raise
            # FTP replies can echo credentials. Report only the operation here.
            raise DeployError(f"FTP deployment failed while {stage}; check the target and FTP server") from None
    print(f"Deployed {artifact.name} to {target.host}:{target.port}{target.path.rstrip('/')}/")


def main():
    target = read_target(ROOT / ".env")
    # Validate the target before building; the Makefile remains the build entry.
    subprocess.run(["make", "--no-print-directory", "all"], cwd=ROOT, check=True)
    with (ROOT / ".package.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        artifact = current_package(ROOT)
        print(f"Uploading {artifact.name} ({artifact.stat().st_size:,} bytes)", flush=True)
        upload(artifact, target)


if __name__ == "__main__":
    try:
        main()
    except (DeployError, OSError, subprocess.CalledProcessError) as error:
        message = str(error) if isinstance(error, DeployError) else "Build or local file access failed"
        print(f"Deploy: {message}", file=sys.stderr)
        sys.exit(1)
