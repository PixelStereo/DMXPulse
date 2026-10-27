#!/usr/bin/env python3
"""Release metadata, version normalization and complete-package validation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import zipfile
import tarfile

REQUIRED = (
    "DMXPulse-macos-arm64.zip", "DMXPulse-macos-x86_64.zip",
    "DMXPulse-windows-x86_64.zip", "DMXPulse-linux-amd64.deb",
    "DMXPulse-raspberry-arm64.deb", "DMXPulse-android-arm64.apk",
    "DMXPulse-ios-device-unsigned.zip", "DMXPulse-ios-simulator-x86_64.zip",
    "DMXPulse-source.tar.gz",
)

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
    missing = [name for name in REQUIRED if not (directory / name).is_file()]
    if missing:
        raise ValueError("Missing release assets: " + ", ".join(missing))
    for name in REQUIRED:
        path = directory / name
        if path.stat().st_size == 0:
            raise ValueError("Empty asset: " + name)
        if path.suffix in (".zip", ".apk"):
            with zipfile.ZipFile(path) as archive:
                if archive.testzip() is not None:
                    raise ValueError("Corrupt archive: " + name)
                names = archive.namelist()
                if path.suffix == ".apk" and not {"AndroidManifest.xml", "classes.dex"}.issubset(names):
                    raise ValueError("APK is missing its Android runtime")
                if path.suffix == ".zip":
                    expected = "DMXPulse/DMXPulse.exe" if "windows" in name else "DMXPulse.app/Contents/MacOS/DMXPulse" if "macos" in name else "DMXPulse.app/DMXPulse"
                    if expected not in names:
                        raise ValueError("Missing application executable in " + name)
                    if "windows" in name and "DMXPulse/platforms/qwindows.dll" not in names:
                        raise ValueError("Missing Windows Qt platform plugin")
                if path.suffix == ".apk" and not any(n.startswith("lib/arm64-v8a/") and "DMXPulse" in n for n in names):
                    raise ValueError("Missing ARM64 Android application library")
        elif name.endswith(".tar.gz"):
            with tarfile.open(path) as archive:
                if "DMXPulse/CMakeLists.txt" not in archive.getnames():
                    raise ValueError("Source archive is missing the build project")
        elif path.suffix == ".deb" and path.read_bytes()[:8] != b"!<arch>\n":
            raise ValueError("Invalid Debian package: " + name)
    return [p for p in sorted(directory.iterdir()) if p.is_file() and p.name not in ("release.json", "SHA256SUMS")]

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
            raise ValueError("Invalid mobile build number")
        values = {"version": version, "build_number": build}
        if os.environ.get("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
                for key, value in values.items():
                    output.write(f"{key}={value}\n")
        print(json.dumps(values))
        return
    directory = Path(args.directory)
    files = verify(directory)
    entries = [{"name": p.name, "bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in files]
    manifest = {"application": "DMXPulse", "version": version, "tag": tag,
                "commit": os.environ.get("GITHUB_SHA", ""), "assets": entries,
                "ios_device_package": "Unsigned; signing/provisioning is required. A signed IPA is included only when Apple signing secrets are configured."}
    (directory / "release.json").write_text(json.dumps(manifest, indent=2) + "\n")
    with (directory / "SHA256SUMS").open("w") as output:
        for path in sorted(directory.iterdir()):
            if path.is_file() and path.name != "SHA256SUMS":
                output.write(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n")

if __name__ == "__main__":
    main()
