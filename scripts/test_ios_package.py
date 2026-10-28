import os
from pathlib import Path
import plistlib
import tempfile
import unittest
import zipfile

from package_ios import package, validate_ipa


class IosPackageTests(unittest.TestCase):
    def make_app(self, directory, simulator=False):
        app = Path(directory, "DMXPulse.app")
        app.mkdir()
        info = {"CFBundleIdentifier": "fr.pixelstereo.dmxpulse",
                "CFBundleExecutable": "DMXPulse",
                "CFBundleSupportedPlatforms": ["iPhoneSimulator" if simulator else "iPhoneOS"]}
        (app / "Info.plist").write_bytes(plistlib.dumps(info))
        (app / "DMXPulse").write_bytes(b"device executable")
        (app / "DMXPulse").chmod(0o755)
        return app

    def test_payload_layout_and_executable_permissions(self):
        with tempfile.TemporaryDirectory() as directory:
            app = self.make_app(directory)
            ipa = Path(directory, "device.ipa")
            package(app, ipa)
            with zipfile.ZipFile(ipa) as archive:
                validate_ipa(archive)
                self.assertIn("Payload/DMXPulse.app/DMXPulse", archive.namelist())
                self.assertNotIn("DMXPulse.app/DMXPulse", archive.namelist())
                self.assertTrue(archive.getinfo("Payload/DMXPulse.app/DMXPulse").external_attr >> 16 & 0o111)

    def test_simulator_is_not_a_device_package(self):
        with tempfile.TemporaryDirectory() as directory:
            app = self.make_app(directory, simulator=True)
            output = Path(directory, "device.ipa")
            output.write_bytes(b"previous complete package")
            with self.assertRaisesRegex(ValueError, "simulator"):
                package(app, output)
            self.assertEqual(output.read_bytes(), b"previous complete package")
            self.assertFalse(output.with_suffix(".ipa.tmp").exists())

    @unittest.skipIf(os.name == "nt", "symlink privilege is not guaranteed on Windows")
    def test_bundle_symlink_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            app = self.make_app(directory)
            (app / "alias").symlink_to("DMXPulse")
            output = Path(directory, "device.ipa")
            package(app, output)
            with zipfile.ZipFile(output) as archive:
                self.assertEqual(archive.read("Payload/DMXPulse.app/alias"), b"DMXPulse")
                self.assertEqual(archive.getinfo("Payload/DMXPulse.app/alias").external_attr >> 16 & 0o170000, 0o120000)
