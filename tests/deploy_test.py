"""Exercise deployment against a real local FTP socket, without a PS4 or secrets."""
import contextlib
import hashlib
import json
from pathlib import Path
import shutil
import socket
import socketserver
import subprocess
import sys
import tempfile
import threading
import unittest

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "scripts"))
import deploy
import package


class FTPHandler(socketserver.StreamRequestHandler):
    def reply(self, text):
        self.wfile.write((text + "\r\n").encode())
        self.wfile.flush()

    def handle(self):
        self.connection.settimeout(10)
        data_socket = None
        self.reply("220 Local deployment test")
        try:
            while line := self.rfile.readline():
                command, _, argument = line.decode().rstrip("\r\n").partition(" ")
                if command == "USER":
                    self.reply("331 Password required")
                elif command == "PASS":
                    self.server.password = argument
                    self.reply("230 Logged in")
                elif command == "CWD":
                    self.server.destination = argument
                    self.reply("250 Directory changed")
                elif command == "TYPE":
                    self.reply("200 Type set")
                elif command == "PASV":
                    data_socket = socket.socket()
                    data_socket.settimeout(10)
                    data_socket.bind(("127.0.0.1", 0))
                    data_socket.listen(1)
                    port = data_socket.getsockname()[1]
                    self.reply(f"227 Passive mode (127,0,0,1,{port // 256},{port % 256})")
                elif command == "STOR":
                    self.reply("150 Opening binary connection")
                    with data_socket.accept()[0] as connection:
                        connection.settimeout(10)
                        payload = bytearray()
                        while block := connection.recv(65536):
                            payload.extend(block)
                    data_socket.close()
                    data_socket = None
                    self.server.files[argument] = bytes(payload)
                    if self.server.fail_upload:
                        self.reply("451 Simulated failed transfer; secret must not be logged")
                    else:
                        self.reply("226 Transfer complete")
                elif command == "SIZE":
                    if self.server.omit_size:
                        self.reply("502 SIZE unsupported")
                    else:
                        size = len(self.server.files[argument]) + self.server.size_offset
                        self.reply(f"213 {size}")
                elif command == "RNFR":
                    self.server.rename_from = argument
                    self.reply("350 Ready for destination")
                elif command == "RNTO":
                    self.server.files[argument] = self.server.files.pop(self.server.rename_from)
                    self.reply("250 Renamed")
                elif command == "DELE":
                    self.server.files.pop(argument, None)
                    self.reply("250 Removed temporary file")
                elif command == "QUIT":
                    self.reply("221 Goodbye")
                    break
                else:
                    self.reply("502 Unsupported command")
        finally:
            if data_socket is not None:
                data_socket.close()


@contextlib.contextmanager
def ftp_server(**options):
    with socketserver.TCPServer(("127.0.0.1", 0), FTPHandler) as server:
        server.files = {}
        server.fail_upload = False
        server.omit_size = False
        server.size_offset = 0
        for key, value in options.items():
            setattr(server, key, value)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            yield server
        finally:
            server.shutdown()
            thread.join(timeout=10)


class Deployment(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.env = self.root / ".env"
        self.artifact = self.root / "PS4Game_installer-r4.pkg"
        self.payload = bytes(range(256)) * 4097
        self.artifact.write_bytes(self.payload)

    def tearDown(self):
        self.temp.cleanup()

    def target(self, server):
        return deploy.Target("127.0.0.1", server.server_address[1], "anonymous", "test password", "/data/pkg")

    def test_env_quotes_comments_and_no_shell_evaluation(self):
        marker = self.root / "must-not-exist"
        password = f"$(touch {marker}) # literal"
        self.env.write_text(
            "# private target\nexport DEPLOY_HOST = 127.0.0.1 # LAN\n"
            "DEPLOY_PORT=2121\nDEPLOY_USER='a user'\n"
            f'DEPLOY_PASS="{password}" # comment\nDEPLOY_PATH="/data/pkg folder"\n')
        target = deploy.read_target(self.env)
        self.assertEqual((target.host, target.port, target.user, target.path),
                         ("127.0.0.1", 2121, "a user", "/data/pkg folder"))
        self.assertEqual(target.password, password)
        self.assertFalse(marker.exists())
        self.assertNotIn(password, repr(target))
        self.env.write_text("DEPLOY_HOST=localhost\nDEPLOY_PASS=pass#hash # comment\n")
        self.assertEqual(deploy.read_target(self.env).password, "pass#hash")

    def test_invalid_or_missing_configuration(self):
        with self.assertRaisesRegex(deploy.DeployError, "Missing .env"):
            deploy.read_target(self.env)
        for content in ("", "DEPLOY_HOST=ftp://localhost", "DEPLOY_HOST=localhost\nDEPLOY_PORT=0",
                        "DEPLOY_HOST=localhost\nDEPLOY_PORT=65536", "DEPLOY_HOST=localhost\nDEPLOY_PORT=no",
                        "DEPLOY_HOST=localhost\nDEPLOY_PATH=relative", "DEPLOY_HOST=localhost\nDEPLOY_PATH=/../data",
                        'DEPLOY_HOST=localhost\nDEPLOY_PASS="secret', "bad assignment"):
            with self.subTest(content=content):
                self.env.write_text(content)
                with self.assertRaises(deploy.DeployError) as error:
                    deploy.read_target(self.env)
                self.assertNotIn("secret", str(error.exception))
        self.env.write_bytes(b"DEPLOY_PASS=secret\xff")
        with self.assertRaisesRegex(deploy.DeployError, "UTF-8") as error:
            deploy.read_target(self.env)
        self.assertNotIn("secret", str(error.exception))

    def test_upload_binary_and_atomic_publish(self):
        for omit_size in (False, True):
            with self.subTest(omit_size=omit_size), ftp_server(omit_size=omit_size) as server:
                deploy.upload(self.artifact, self.target(server))
                self.assertEqual(server.files, {self.artifact.name: self.payload})
                self.assertEqual(server.destination, "/data/pkg")
                self.assertEqual(server.password, "test password")

    def test_failed_transfer_and_size_mismatch_keep_existing_final(self):
        for options in ({"fail_upload": True}, {"size_offset": 1}):
            with self.subTest(options=options), ftp_server(**options) as server:
                server.files[self.artifact.name] = b"previous complete package"
                with self.assertRaises(deploy.DeployError) as error:
                    deploy.upload(self.artifact, self.target(server))
                self.assertNotIn("secret", str(error.exception))
                self.assertEqual(server.files, {self.artifact.name: b"previous complete package"})

    def fixture_project(self):
        for name in ("src", "assets", "scripts", "third_party", "dist"):
            (self.root / name).mkdir()
        for name in ("Dockerfile", "LICENSE", "THIRD_PARTY.md"):
            (self.root / name).write_text("fixture\n")
        (self.root / "Makefile").write_text("all:\n\t@touch build-was-run\n")
        for name in ("deploy.py", "package.py"):
            shutil.copy(PROJECT / "scripts" / name, self.root / "scripts" / name)
        artifact = self.root / "dist" / self.artifact.name
        artifact.write_bytes(self.payload)
        state = {"revision": 4, "input_hash": package.fingerprint(self.root)}
        (self.root / "revision.json").write_text(json.dumps(state))
        artifact.with_suffix(".json").write_text(json.dumps(dict(
            state, sha256=hashlib.sha256(self.payload).hexdigest())))
        # A newer-looking file must never displace the recorded revision.
        (self.root / "dist/PS4Game_installer-r999.pkg").write_bytes(b"stale build")
        return artifact

    def test_make_deploy_builds_and_uploads_recorded_revision(self):
        artifact = self.fixture_project()
        with ftp_server() as server:
            self.env.write_text(f"DEPLOY_HOST=127.0.0.1\nDEPLOY_PORT={server.server_address[1]}\n"
                                "DEPLOY_PASS='test password'\nDEPLOY_PATH=/data/pkg\n")
            result = subprocess.run(["make", "-f", str(PROJECT / "Makefile"),
                                     "PSGI_IN_CONTAINER=1", "deploy"], cwd=self.root,
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue((self.root / "build-was-run").is_file())
            self.assertEqual(server.files, {artifact.name: self.payload})
            self.assertNotIn("test password", result.stdout + result.stderr)

    def test_package_integrity_and_config_preflight(self):
        artifact = self.fixture_project()
        self.assertEqual(deploy.current_package(self.root), artifact)
        artifact.write_bytes(b"corrupt")
        with self.assertRaisesRegex(deploy.DeployError, "checksum"):
            deploy.current_package(self.root)
        artifact.write_bytes(self.payload)
        (self.root / "src/changed.cpp").write_text("new build input")
        with self.assertRaisesRegex(deploy.DeployError, "rebuild"):
            deploy.current_package(self.root)
        result = subprocess.run([sys.executable, str(self.root / "scripts/deploy.py")],
                                cwd=self.root, capture_output=True, text=True, timeout=30)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing .env", result.stderr)
        self.assertFalse((self.root / "build-was-run").exists())


if __name__ == "__main__":
    unittest.main()
