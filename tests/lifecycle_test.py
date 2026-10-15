"""Native shell termination, credential restoration, and failure recovery."""
from pathlib import Path
import subprocess
import unittest

PROJECT = Path(__file__).resolve().parents[1]


class NativeLifecycleRegression(unittest.TestCase):
    def test_native_exit_and_credential_failures(self):
        for scenario in ("normal", "read-failure", "prepare-failure", "apply-failure",
                         "restore-failure", "exit-failure"):
            with self.subTest(scenario=scenario):
                result = subprocess.run([str(PROJECT / "build/host/lifecycle-test"), scenario],
                                        capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn(f"Native lifecycle passed: {scenario}", result.stdout)
                if scenario == "restore-failure":
                    self.assertIn("Restore homebrew credentials", result.stderr)
                if scenario == "exit-failure":
                    self.assertIn("PS4 shell exit failed", result.stderr)
                    self.assertIn("application status 1", result.stderr)


if __name__ == "__main__":
    unittest.main()
