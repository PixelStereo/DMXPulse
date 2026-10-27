import tempfile
import unittest
from pathlib import Path
from release import numeric_version, verify

class ReleaseTests(unittest.TestCase):
    def test_versions(self):
        for tag, expected in (("v1.2.3", "1.2.3"), ("1.2", "1.2.0"), ("v2.0.0-rc.1", "2.0.0"), ("preview", "1.1.0"), ("", "1.1.0")):
            self.assertEqual(numeric_version(tag, "1.1.0"), expected)
    def test_invalid_version_file(self):
        with self.assertRaises(ValueError):
            numeric_version("preview", "broken")
    def test_platform_limits(self):
        with self.assertRaises(ValueError):
            numeric_version("v999999.0.0", "1.1.0")
    def test_no_partial_release(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "DMXPulse-macos-arm64.zip").write_bytes(b"broken")
            with self.assertRaisesRegex(ValueError, "Missing release assets"):
                verify(directory)

if __name__ == "__main__":
    unittest.main()
