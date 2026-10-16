"""Exercise real HTTP streaming and staging through libcurl, including bad servers."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import tempfile
import threading
import unittest

RUNNER = Path("build/host/download-runner")
BODY = b"\x7fCNT" + bytes(i % 251 for i in range(4, 1024 * 1024 + 4))


class DownloadTests(unittest.TestCase):
    def test_package_routes_and_fallback(self):
        if not RUNNER.exists():
            self.skipTest("build the download runner with make test")
        calls = Counter()

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_args):
                pass

            def do_HEAD(self):
                raise AssertionError("A package probe must not depend on HEAD support")

            def do_GET(self):
                mode = self.path.split("/")[1]
                ranged = bool(self.headers.get("Range"))
                if ranged:
                    assert self.headers["Range"] == "bytes=0-3"
                calls[mode, ranged] += 1
                if mode == "missing":
                    self.send_error(404)
                    return
                data = b"<html>not a package</html>" if mode == "html" else BODY
                partial = mode == "range" and ranged
                if partial:
                    data = data[:4]
                self.send_response(206 if partial else 200)
                if mode != "unknown-length" or ranged:
                    self.send_header("Content-Length", str(len(data)))
                if partial:
                    self.send_header("Content-Range", f"bytes 0-3/{len(BODY)}")
                    self.send_header("Accept-Ranges", "bytes")
                self.end_headers()
                if mode == "truncated" and not ranged:
                    data = data[:10000]
                try:
                    for pos in range(0, len(data), 8192):
                        self.wfile.write(data[pos:pos + 8192])
                except (ConnectionError, OSError):
                    pass  # The four-byte probe intentionally closes a full-body response.

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            for mode in ("range", "local", "missing", "html", "cancel", "space", "unknown-length", "truncated", "safe-refusal", "uncertain-refusal"):
                with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix="psgi-download-") as directory:
                    url = f"http://127.0.0.1:{server.server_port}/{mode}/A%20Game%20%5BUS%5D.pkg?token=kept"
                    result = subprocess.run([str(RUNNER), url, directory, mode], capture_output=True, text=True, timeout=15)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(calls["range", True], 1)
            self.assertEqual(calls["range", False], 0)
            self.assertEqual(calls["local", True], 2)
            self.assertEqual(calls["local", False], 1)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
