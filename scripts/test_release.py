import os
import tempfile
import unittest
from pathlib import Path
import names
from release import numeric_version, verify

os.environ["RELEASE_TAG"] = "v1.2.3"
MAC, WIN = names.name("macos", "arm64", ".zip"), names.name("windows", "x64", "-Setup.exe")
IOS_UNSIGNED, IOS_SIGNED = names.name("ios", "arm64", "-unsigned.ipa"), names.name("ios", "arm64", ".ipa")

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
            Path(directory, MAC).write_bytes(b"broken")
            with self.assertRaisesRegex(ValueError, "Missing release assets"):
                verify(directory)


class PackageSetTests(unittest.TestCase):
    def make_release(self, directory):
        import plistlib
        import zipfile
        from release import required
        root = Path(directory)
        for name in required():
            path = root / name
            if name.endswith('.exe'):
                path.write_bytes(b'MZ installer')
            elif name.endswith('.deb'):
                path.write_bytes(b'!<arch>\npackage')
            elif name.endswith('.zip'):
                with zipfile.ZipFile(path, 'w') as archive:
                    archive.writestr('DMXPulse 1.2.3+4.app/Contents/MacOS/DMXPulse', b'executable')
            elif name.endswith('.apk'):
                with zipfile.ZipFile(path, 'w') as archive:
                    for member in ('AndroidManifest.xml', 'classes.dex', 'lib/arm64-v8a/libDMXPulse.so'):
                        archive.writestr(member, b'content')
        with zipfile.ZipFile(root / IOS_UNSIGNED, 'w') as archive:
            archive.writestr('Payload/DMXPulse.app/Info.plist', plistlib.dumps({
                'CFBundleIdentifier': 'fr.pixelstereo.dmxpulse',
                'CFBundleExecutable': 'DMXPulse',
                'CFBundleSupportedPlatforms': ['iPhoneOS']}))
            archive.writestr('Payload/DMXPulse.app/DMXPulse', b'executable')
        return root

    def test_complete_zip_installer_and_ipa_set(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_release(directory)
            files = verify(root)
            self.assertEqual(len(files), 7)
            self.assertEqual(set(root.iterdir()), set(files))
            (root / 'internal-build-log.txt').write_text('CI log')
            self.assertEqual(files, verify(root))
            (root / IOS_UNSIGNED).rename(root / 'DMXPulse-ios-device-unsigned.zip')
            with self.assertRaisesRegex(ValueError, 'one iOS device IPA'):
                verify(root)

    def test_installer_signature_and_obsolete_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_release(directory)
            (root / 'DMXPulse-macos-arm64.dmg').write_bytes(b'old download')
            with self.assertRaisesRegex(ValueError, 'Obsolete'):
                verify(root)
            (root / 'DMXPulse-macos-arm64.dmg').unlink()
            (root / WIN).write_bytes(b'corrupt')
            with self.assertRaisesRegex(ValueError, 'Windows installer'):
                verify(root)

    def test_exactly_one_ipa_and_signing_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_release(directory)
            import shutil
            shutil.copy2(root / IOS_UNSIGNED, root / IOS_SIGNED)
            with self.assertRaisesRegex(ValueError, 'exactly one'):
                verify(root)
            (root / IOS_UNSIGNED).unlink()
            with self.assertRaisesRegex(ValueError, 'profile/signature'):
                verify(root)

    def test_prune_preserves_required_and_unrelated_downloads(self):
        from release import stale_assets
        with tempfile.TemporaryDirectory() as directory:
            self.make_release(directory)
            self.assertEqual(stale_assets([
                IOS_SIGNED, IOS_UNSIGNED,
                'DMXPulse-macos-arm64.zip', 'DMXPulse-macos-arm64.dmg',
                'DMXPulse-source.tar.gz', 'release.json', 'SHA256SUMS',
                'Android-NOTICES.md', 'DMXPulse-android-arm64.apk.idsig',
                'DMXPulse-windows-x86_64.zip', 'custom-user-file.zip'], directory),
                ['Android-NOTICES.md', 'DMXPulse-android-arm64.apk.idsig',
                 IOS_SIGNED, 'DMXPulse-macos-arm64.dmg', 'DMXPulse-macos-arm64.zip',
                 'DMXPulse-source.tar.gz', 'DMXPulse-windows-x86_64.zip', 'SHA256SUMS', 'release.json'])


if __name__ == "__main__":
    unittest.main()
