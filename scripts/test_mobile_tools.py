import tempfile
import unittest
from pathlib import Path
from run_mobile_test import find_android_test_apk, select_ios_device

class MobileToolTests(unittest.TestCase):
    def test_ios_prefers_supported_mature_runtime_and_phone(self):
        def device(name, udid, state="Shutdown", available=True):
            return {"name": name, "udid": udid, "state": state, "isAvailable": available}
        inventory = {
            "com.apple.CoreSimulator.SimRuntime.iOS-26-0": [device("iPhone 17", "new")],
            "com.apple.CoreSimulator.SimRuntime.iOS-18-5": [device("iPad", "tablet"), device("iPhone 16", "phone")],
            "com.apple.CoreSimulator.SimRuntime.iOS-15-0": [device("iPhone 8", "old", "Booted")],
            "com.apple.CoreSimulator.SimRuntime.tvOS-18-0": [device("Apple TV", "tv", "Booted")],
        }
        self.assertEqual(select_ios_device(inventory)["udid"], "phone")
        inventory["com.apple.CoreSimulator.SimRuntime.iOS-26-0"][0]["state"] = "Booted"
        self.assertEqual(select_ios_device(inventory)["udid"], "new")
        with self.assertRaises(RuntimeError):
            select_ios_device({})

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
