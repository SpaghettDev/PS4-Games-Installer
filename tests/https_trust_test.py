"""Keep the reviewed trust bundle and its license in the produced package."""
import hashlib
import os
from pathlib import Path
import runpy
import tempfile
import unittest
from unittest.mock import patch

PROJECT = Path(__file__).resolve().parents[1]
BUNDLE_SHA256 = "a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505"


class HttpsTrustPackage(unittest.TestCase):
    def test_reviewed_bundle_and_license(self):
        bundle = (PROJECT / "assets/cacert.pem").read_bytes()
        self.assertEqual(hashlib.sha256(bundle).hexdigest(), BUNDLE_SHA256)
        self.assertEqual(bundle.count(b"-----BEGIN CERTIFICATE-----"), 121)
        self.assertEqual(bundle.count(b"-----END CERTIFICATE-----"), 121)
        license_text = (PROJECT / "assets/cacert-LICENSE.txt").read_text()
        self.assertIn("Mozilla Public License Version 2.0", license_text)
        self.assertIn(BUNDLE_SHA256, (PROJECT / "THIRD_PARTY.md").read_text())

    def test_metadata_stages_bundle_and_license(self):
        original = Path.cwd()
        with tempfile.TemporaryDirectory() as temporary:
            try:
                os.chdir(temporary)
                with patch("sys.argv", ["metadata.py", "/fixture-sdk", "1", "CONTENT", "TITLE"]), \
                     patch("shutil.copyfile") as copy_file, patch("subprocess.run") as run:
                    runpy.run_path(str(PROJECT / "scripts/metadata.py"), run_name="__main__")
                for name in ("assets/cacert.pem", "assets/cacert-LICENSE.txt"):
                    copy_file.assert_any_call(Path(name), Path("build/pkg") / name)
                    package_files = run.call_args_list[-1].args[0][-1].split()
                    self.assertIn(name, package_files)
            finally:
                os.chdir(original)


if __name__ == "__main__":
    unittest.main()
