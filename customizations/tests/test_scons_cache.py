"""Exercise pinned SCons CacheDir retrieval and content invalidation, not Godot."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class SConsCacheTests(unittest.TestCase):
    def test_reuses_unchanged_content_but_rebuilds_changed_source(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "SConstruct").write_text(
                "env = Environment(tools=[])\n"
                "env.Decider('content')\n"
                "env.CacheDir(ARGUMENTS['cache_path'])\n"
                "env.Command('out.txt', 'source.txt', Copy('$TARGET', '$SOURCE'))\n",
                encoding="utf-8",
            )
            source = root / "source.txt"
            target = root / "out.txt"

            def build():
                result = subprocess.run(
                    [sys.executable, "-m", "SCons", "-Q", f"cache_path={root / 'cache'}"],
                    cwd=root, capture_output=True, text=True, timeout=30,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result.stdout

            source.write_text("production-v1", encoding="utf-8")
            self.assertNotIn("Retrieved", build())
            self.assertEqual(target.read_text(), "production-v1")
            target.unlink()
            # Like CI, preserve only CacheDir, not the prior dependency database.
            (root / ".sconsign.dblite").unlink()
            self.assertIn("Retrieved", build())
            self.assertEqual(target.read_text(), "production-v1")
            target.unlink()
            (root / ".sconsign.dblite").unlink()
            source.write_text("changed-source-v2", encoding="utf-8")
            self.assertNotIn("Retrieved", build())
            self.assertEqual(target.read_text(), "changed-source-v2")


if __name__ == "__main__":
    unittest.main()
