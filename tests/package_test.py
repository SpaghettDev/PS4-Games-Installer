"""Exercise revision transactions using a real Make subprocess and isolated outputs."""
import concurrent.futures
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

PROJECT = Path(__file__).resolve().parents[1]


class PackageTransactions(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        for name in ("src", "assets", "scripts", "third_party", "sdk/bin/linux"):
            (self.root / name).mkdir(parents=True)
        shutil.copy(PROJECT / "scripts/package.py", self.root / "scripts/package.py")
        (self.root / "src/app.cpp").write_text("first source\n")
        (self.root / "Dockerfile").write_text("fixture toolchain\n")
        (self.root / "LICENSE").write_text("fixture license\n")
        (self.root / "THIRD_PARTY.md").write_text("fixture notices\n")
        (self.root / "revision.json").write_text('{"revision":1,"input_hash":"prototype"}')
        (self.root / "Makefile").write_text(
            "package-internal:\n"
            '\t@test "$$FAIL_PACKAGE" != "1"\n'
            "\t@mkdir -p build/package\n"
            "\t@python3 -c 'from pathlib import Path; import os, time; time.sleep(.1); "
            'Path("build/package/IV0000-PGIN00001_00-PS4GAMEINSTALLER.pkg").write_bytes(b"validated fixture pkg"); '
            'open("build-count", "a").write("built\\n"); '
            'os.environ.get("MUTATE_INPUT") and Path("src/app.cpp").write_text("changed during build")' + "'\n"
        )
        tool = self.root / "sdk/bin/linux/PkgTool.Core"
        tool.write_text("#!/bin/sh\nexit 0\n")
        tool.chmod(0o755)
        self.env = dict(os.environ, OO_PS4_TOOLCHAIN=str(self.root / "sdk"))

    def tearDown(self):
        self.temp.cleanup()

    def run_package(self, **flags):
        return subprocess.run([sys.executable, str(self.root / "scripts/package.py")],
                              env=dict(self.env, **flags), capture_output=True, text=True, timeout=30)

    def state(self):
        return json.loads((self.root / "revision.json").read_text())

    def test_changed_no_change_and_clean(self):
        self.assertEqual(self.run_package().returncode, 0)
        self.assertEqual(self.state()["revision"], 2)
        self.assertEqual(self.run_package().returncode, 0)
        self.assertEqual((self.root / "build-count").read_text().count("built"), 1)
        shutil.rmtree(self.root / "build")
        shutil.rmtree(self.root / "dist")
        self.assertEqual(self.run_package().returncode, 0)
        self.assertEqual(self.state()["revision"], 2)
        (self.root / "src/app.cpp").write_text("next source\n")
        self.assertEqual(self.run_package().returncode, 0)
        self.assertEqual(self.state()["revision"], 3)
        self.assertTrue((self.root / "dist/PS4Game_installer-r3.pkg").is_file())

    def test_failed_build_does_not_consume_revision(self):
        original = self.state()
        self.assertNotEqual(self.run_package(FAIL_PACKAGE="1").returncode, 0)
        self.assertEqual(self.state(), original)
        self.assertFalse((self.root / "dist").exists())

    def test_changed_inputs_during_build_do_not_commit(self):
        original = self.state()
        result = self.run_package(MUTATE_INPUT="1")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("changed during packaging", result.stderr)
        self.assertEqual(self.state(), original)

    def test_concurrent_builds_allocate_once(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(lambda _: self.run_package(), range(2)))
        self.assertTrue(all(r.returncode == 0 for r in results), [r.stderr for r in results])
        self.assertEqual(self.state()["revision"], 2)
        self.assertEqual((self.root / "build-count").read_text().count("built"), 1)


if __name__ == "__main__":
    unittest.main()
