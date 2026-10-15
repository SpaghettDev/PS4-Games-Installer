"""Exercise the real fetch/parser path without contacting a package server."""
import http.server
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/catalog")
            self.end_headers()
            return
        if self.path == "/missing":
            self.send_error(404)
            return
        if self.path == "/large":
            data = b" " * (8 * 1024 * 1024 + 1)
        elif self.path == "/malformed":
            data = b'{"DATA":'
        elif self.path == "/empty":
            data = b'{"DATA":{}}'
        else:
            data = json.dumps({"DATA": {"http://nas/game.pkg": {
                "name": "Test package", "size": 8589934592
            }}}).encode()
        self.send_response(200)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, *args):
        pass


def main():
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            config = Path(tmp) / "config.json"
            for endpoint, expected, text in [
                ("catalog", 0, "1 unique packages"),
                ("empty", 0, "0 unique packages"),
                ("redirect", 1, "HTTP 302"),
                ("missing", 1, "HTTP 404"),
                ("malformed", 1, "Expected a JSON object"),
                ("large", 1, "Catalog download failed"),
            ]:
                config.write_text(json.dumps({"content_urls": {
                    "games": f"http://127.0.0.1:{server.server_port}/{endpoint}"
                }}))
                result = subprocess.run([sys.argv[1], str(config)], capture_output=True, text=True, timeout=15)
                assert result.returncode == expected, (endpoint, result.stdout, result.stderr)
                assert text in result.stdout + result.stderr, (endpoint, result.stdout, result.stderr)
        print("HTTP integration tests passed")
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    main()
