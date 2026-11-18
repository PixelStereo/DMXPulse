#!/usr/bin/env python3
"""Release metadata, version normalization and complete-package validation."""
import argparse
import json
import os
from pathlib import Path
import re
import zipfile
import names

# <App>-<os>-<arch>-<tag>: the same rule in every Pixel Stereo application (see scripts/names.py)
def required():
    return (names.name("macos", "arm64", ".zip"), names.name("macos", "x64", ".zip"),
            names.name("windows", "x64", "-Setup.exe"), names.name("linux", "x64", ".deb"),
            names.name("linux", "arm64", ".deb"))

# Names of earlier formats: never published together with the current ones
OBSOLETE = ("DMXPulse-macos-arm64.dmg", "DMXPulse-macos-x86_64.dmg", "DMXPulse-windows-x86_64.zip",
            "DMXPulse-macos-arm64.zip", "DMXPulse-macos-x86_64.zip", "DMXPulse-windows-x86_64-Setup.exe",
            "DMXPulse-linux-amd64.deb", "DMXPulse-raspberry-arm64.deb", "DMXPulse-android-arm64.apk",
            "DMXPulse-ios-unsigned.ipa", "DMXPulse-ios.ipa",
            "DMXPulse-ios-device-unsigned.zip", "DMXPulse-ios-simulator-x86_64.zip",
            "DMXPulse-source.tar.gz", "release.json", "SHA256SUMS",
            "Android-NOTICES.md", "DMXPulse-android-arm64.apk.idsig")

def obsolete():
    # DMXPulse is a desktop application: the mobile packages it once published are pruned from a release being updated
    return set(OBSOLETE) | {names.name("android", "arm64", ".apk"), names.name("ios", "arm64", "-unsigned.ipa"),
                            names.name("ios", "arm64", ".ipa")}

def stale_assets(names, directory):
    return sorted(set(names) & obsolete())


def numeric_version(tag, fallback):
    match = re.fullmatch(r"v?(\d+)\.(\d+)(?:\.(\d+))?(?:[-+][0-9A-Za-z.-]+)?", tag)
    version = ".".join(str(int(x or 0)) for x in match.groups()) if match else fallback.strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("VERSION must contain major.minor.patch")
    if any(int(x) > 65535 for x in version.split(".")):
        raise ValueError("Version components exceed platform limits")
    return version

def verify(directory):
    directory = Path(directory)
    missing = [name for name in required() if not (directory / name).is_file()]
    if missing:
        raise ValueError("Missing release assets: " + ", ".join(missing))
    stale = [name for name in sorted(obsolete()) if (directory / name).exists()]
    if stale:
        raise ValueError("Obsolete release assets: " + ", ".join(stale))
    for name in required():
        path = directory / name
        if path.stat().st_size == 0:
            raise ValueError("Empty asset: " + name)
        if path.suffix == ".zip":
            with zipfile.ZipFile(path) as archive:
                if archive.testzip() is not None:
                    raise ValueError("Corrupt archive: " + name)
                names = archive.namelist()
                if not any(n.endswith(".app/Contents/MacOS/DMXPulse") for n in names):
                    raise ValueError("Missing application executable in " + name)
        elif path.suffix == ".exe" and path.read_bytes()[:2] != b"MZ":
            raise ValueError("Invalid Windows installer: " + name)
        elif path.suffix == ".deb" and path.read_bytes()[:8] != b"!<arch>\n":
            raise ValueError("Invalid Debian package: " + name)
    return [directory / name for name in sorted(required())]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("metadata", "verify"))
    parser.add_argument("--directory", default="dist")
    args = parser.parse_args()
    tag = os.environ.get("RELEASE_TAG", "")
    version = numeric_version(tag, Path("VERSION").read_text())
    if args.command == "metadata":
        build = os.environ.get("GITHUB_RUN_NUMBER", "1")
        if not build.isdigit() or not 1 <= int(build) <= 2100000000:
            raise ValueError("Invalid build number")
        values = {"version": version, "build_number": build}
        if os.environ.get("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
                for key, value in values.items():
                    output.write(f"{key}={value}\n")
        print(json.dumps(values))
        return
    directory = Path(args.directory)
    files = verify(directory)
    print("Verified release packages: " + ", ".join(path.name for path in files))

if __name__ == "__main__":
    main()
