import importlib.util
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("package", Path(__file__).resolve().parents[1] / "scripts/package.py")
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class RevisionTests(unittest.TestCase):
    def test_changed_build_increments(self):
        self.assertEqual(package.next_revision({"revision": 2, "input_hash": "old"}, "new"), 3)

    def test_unchanged_build_and_clean_reuse_revision(self):
        self.assertEqual(package.next_revision({"revision": 7, "input_hash": "same"}, "same"), 7)

    def test_first_revision_is_two(self):
        self.assertEqual(package.next_revision({"revision": 1, "input_hash": "prototype"}, "new"), 2)


if __name__ == "__main__":
    unittest.main()
