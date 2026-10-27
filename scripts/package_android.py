#!/usr/bin/env python3
import base64
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


def main():
    build = Path(os.environ.get("ANDROID_BUILD_DIR", "build-android"))
    candidates = list(build.rglob("*-release-unsigned.apk"))
    if len(candidates) != 1:
        raise RuntimeError(f"Expected one unsigned release APK, found {candidates}")
    tools = Path(os.environ["ANDROID_SDK_ROOT"]) / "build-tools/35.0.0"
    output = Path("dist")
    output.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        keystore = directory / "release.keystore"
        if os.environ.get("ANDROID_KEYSTORE_BASE64"):
            keystore.write_bytes(base64.b64decode(os.environ["ANDROID_KEYSTORE_BASE64"], validate=True))
            alias = os.environ["ANDROID_KEY_ALIAS"]
        else:
            alias = "androiddebugkey"
            os.environ["ANDROID_KEYSTORE_PASSWORD"] = "android"
            os.environ["ANDROID_KEY_PASSWORD"] = "android"
            run("keytool", "-genkeypair", "-keystore", keystore, "-alias", alias,
                "-storepass", "android", "-keypass", "android", "-dname", "CN=DMXPulse CI test key",
                "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000")
        aligned = directory / "aligned.apk"
        run(tools / "zipalign", "-f", "-p", "4", candidates[0], aligned)
        apk = output / "DMXPulse-android-arm64.apk"
        run(tools / "apksigner", "sign", "--ks", keystore, "--ks-key-alias", alias,
            "--ks-pass", "env:ANDROID_KEYSTORE_PASSWORD", "--key-pass", "env:ANDROID_KEY_PASSWORD",
            "--out", apk, aligned)
        run(tools / "apksigner", "verify", "--verbose", apk)
        shutil.copy2("THIRD-PARTY-NOTICES.md", output / "Android-NOTICES.md")

if __name__ == "__main__":
    main()
