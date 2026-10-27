import tempfile
import unittest
from pathlib import Path
from run_mobile_test import find_android_test_apk

class MobileToolTests(unittest.TestCase):
    def test_qt_default_apk_path(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "android-build/build/outputs/apk/debug/android-build-debug.apk")
            path.parent.mkdir(parents=True)
            path.touch()
            self.assertEqual(find_android_test_apk(directory), path)
    def test_ambiguous_apks_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            for name in ("first-debug.apk", "second-debug.apk"):
                Path(directory, name).touch()
            with self.assertRaises(RuntimeError):
                find_android_test_apk(directory)

if __name__ == "__main__":
    unittest.main()
