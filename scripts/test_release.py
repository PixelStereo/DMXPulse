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
            Path(directory, "DMXPulse-macos-arm64.dmg").write_bytes(b"broken")
            with self.assertRaisesRegex(ValueError, "Missing release assets"):
                verify(directory)


class PackageSetTests(unittest.TestCase):
    def make_release(self, directory):
        import io
        import plistlib
        import tarfile
        import zipfile
        from release import REQUIRED
        root = Path(directory)
        for name in REQUIRED:
            path = root / name
            if name.endswith('.dmg'):
                path.write_bytes(b'disk data' + b'koly' + b'\0' * 508)
            elif name.endswith('.deb'):
                path.write_bytes(b'!<arch>\npackage')
            elif name.endswith('.zip'):
                with zipfile.ZipFile(path, 'w') as archive:
                    archive.writestr('DMXPulse/DMXPulse.exe', b'executable')
                    archive.writestr('DMXPulse/platforms/qwindows.dll', b'plugin')
            elif name.endswith('.apk'):
                with zipfile.ZipFile(path, 'w') as archive:
                    for member in ('AndroidManifest.xml', 'classes.dex', 'lib/arm64-v8a/libDMXPulse.so'):
                        archive.writestr(member, b'content')
            else:
                with tarfile.open(path, 'w:gz') as archive:
                    member = tarfile.TarInfo('DMXPulse/CMakeLists.txt')
                    member.size = 5
                    archive.addfile(member, io.BytesIO(b'cmake'))
        with zipfile.ZipFile(root / 'DMXPulse-ios-unsigned.ipa', 'w') as archive:
            archive.writestr('Payload/DMXPulse.app/Info.plist', plistlib.dumps({
                'CFBundleIdentifier': 'fr.pixelstereo.dmxpulse',
                'CFBundleExecutable': 'DMXPulse',
                'CFBundleSupportedPlatforms': ['iPhoneOS']}))
            archive.writestr('Payload/DMXPulse.app/DMXPulse', b'executable')
        return root

    def test_complete_dmg_and_ipa_set(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_release(directory)
            files = verify(root)
            self.assertEqual(len(files), 8)
            (root / 'DMXPulse-ios-unsigned.ipa').rename(root / 'DMXPulse-ios-device-unsigned.zip')
            with self.assertRaisesRegex(ValueError, 'one iOS device IPA'):
                verify(root)

    def test_dmg_trailer_and_obsolete_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_release(directory)
            (root / 'DMXPulse-macos-arm64.zip').write_bytes(b'old download')
            with self.assertRaisesRegex(ValueError, 'Obsolete'):
                verify(root)
            (root / 'DMXPulse-macos-arm64.zip').unlink()
            (root / 'DMXPulse-macos-arm64.dmg').write_bytes(b'corrupt')
            with self.assertRaisesRegex(ValueError, 'disk image'):
                verify(root)

    def test_exactly_one_ipa_and_signing_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_release(directory)
            import shutil
            shutil.copy2(root / 'DMXPulse-ios-unsigned.ipa', root / 'DMXPulse-ios.ipa')
            with self.assertRaisesRegex(ValueError, 'exactly one'):
                verify(root)
            (root / 'DMXPulse-ios-unsigned.ipa').unlink()
            with self.assertRaisesRegex(ValueError, 'profile/signature'):
                verify(root)

    def test_prune_preserves_required_and_unrelated_downloads(self):
        from release import stale_assets
        with tempfile.TemporaryDirectory() as directory:
            self.make_release(directory)
            self.assertEqual(stale_assets([
                'DMXPulse-ios.ipa', 'DMXPulse-ios-unsigned.ipa',
                'DMXPulse-macos-arm64.zip', 'DMXPulse-macos-arm64.dmg',
                'DMXPulse-windows-x86_64.zip', 'custom-user-file.zip'], directory),
                ['DMXPulse-ios.ipa', 'DMXPulse-macos-arm64.zip'])


if __name__ == "__main__":
    unittest.main()
