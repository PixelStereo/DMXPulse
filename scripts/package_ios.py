#!/usr/bin/env python3
"""Wrap an unsigned iPhoneOS application in a resignment-ready IPA."""
import argparse
import os
from pathlib import Path
import plistlib
import stat
import zipfile


def validate_ipa(archive):
    """Validate the device bundle and Payload layout, without claiming signing."""
    names = archive.namelist()
    root = "Payload/DMXPulse.app/"
    info_path = root + "Info.plist"
    if info_path not in names:
        raise ValueError("IPA must contain Payload/DMXPulse.app/Info.plist")
    if any(name.startswith("Payload/") and not name.startswith(root)
           and name != "Payload/" for name in names):
        raise ValueError("IPA must contain only the DMXPulse application in Payload")
    if any(".." in Path(name).parts or name.startswith("/") for name in names):
        raise ValueError("Unsafe archive path in IPA")
    info = plistlib.loads(archive.read(info_path))
    executable = info.get("CFBundleExecutable", "")
    if not executable or Path(executable).name != executable or executable in (".", ".."):
        raise ValueError("Invalid iOS application executable")
    if root + executable not in names or archive.getinfo(root + executable).file_size == 0:
        raise ValueError("IPA application executable is missing or empty")
    if info.get("CFBundleIdentifier") != "fr.pixelstereo.dmxpulse":
        raise ValueError("Unexpected iOS bundle identifier")
    platforms = info.get("CFBundleSupportedPlatforms", [])
    platform = info.get("DTPlatformName", "")
    if "iPhoneSimulator" in platforms or platform == "iphonesimulator":
        raise ValueError("An iOS simulator application cannot be packaged for a device")
    if "iPhoneOS" not in platforms and platform != "iphoneos":
        raise ValueError("IPA must contain an iPhoneOS device application")


def package(app, output):
    app, output = Path(app).resolve(), Path(output).resolve()
    if app.name != "DMXPulse.app" or not app.is_dir():
        raise ValueError("Expected a DMXPulse.app device bundle")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    try:
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED) as archive:
            for directory, dirs, files in os.walk(app, followlinks=False):
                for name in sorted(dirs + files):
                    path = Path(directory, name)
                    relative = "Payload/DMXPulse.app/" + path.relative_to(app).as_posix()
                    mode = path.lstat().st_mode
                    if stat.S_ISLNK(mode):
                        entry = zipfile.ZipInfo(relative)
                        entry.create_system = 3
                        entry.external_attr = mode << 16
                        archive.writestr(entry, os.readlink(path).encode("utf-8"))
                    elif path.is_file():
                        archive.write(path, relative)
        with zipfile.ZipFile(temporary) as archive:
            if archive.testzip() is not None:
                raise ValueError("Corrupt IPA archive")
            validate_ipa(archive)
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"Created unsigned IPA: {output}")
    print("Apple signing/provisioning is required before device installation.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", help="Path to a device DMXPulse.app")
    parser.add_argument("--output", default="dist/DMXPulse-ios-unsigned.ipa")
    args = parser.parse_args()
    package(args.app, args.output)


if __name__ == "__main__":
    main()
