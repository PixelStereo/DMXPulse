#!/usr/bin/env python3
"""Release file names: <App>-<os>-<arch>-<tag><suffix>, the same rule in every Pixel Stereo application.

    names.py <os> <arch> <suffix>      e.g. names.py macos arm64 .zip  ->  DMXPulse-macos-arm64-v0.3.0.zip

The tag is RELEASE_TAG (the tag being built); a build that is not a release says "dev".
"""
import os
import sys

APP = "DMXPulse"
ARCHITECTURES = {"x86_64": "x64", "amd64": "x64", "aarch64": "arm64"}


def tag():
    return os.environ.get("RELEASE_TAG") or "dev"


def name(system, arch, suffix, release_tag=None):
    arch = ARCHITECTURES.get(arch, arch)
    return f"{APP}-{system}-{arch}-{release_tag or tag()}{suffix}"


if __name__ == "__main__":
    print(name(*sys.argv[1:4]))
