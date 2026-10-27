#!/usr/bin/env python3
"""Require the on-device test marker; process startup alone is not a pass."""
import argparse
import json
from pathlib import Path
import subprocess
import time

PASS = "DMXPULSE_MOBILE_TEST_PASS"
FAIL = "DMXPULSE_MOBILE_TEST_FAIL"
BUNDLE = "fr.pixelstereo.dmxpulse.test"


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, **kwargs)


def find_android_test_apk(build):
    # Qt 6.8 uses android-build-debug.apk even for a differently named CMake target.
    candidates = list(Path(build).rglob("*-debug.apk"))
    if len(candidates) != 1:
        raise RuntimeError(f"Expected one mobile test APK: {candidates}")
    return candidates[0]


def android(build):
    run("adb", "install", "-r", str(find_android_test_apk(build)), timeout=90)
    run("adb", "logcat", "-c")
    run("adb", "shell", "am", "start", "-n", BUNDLE + "/org.qtproject.qt.android.bindings.QtActivity", timeout=30)
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        log = subprocess.check_output(["adb", "logcat", "-d"], text=True, timeout=15)
        if FAIL in log:
            raise RuntimeError(log)
        if PASS in log:
            print(log)
            return
        time.sleep(1)
    raise RuntimeError("Android test did not report completion:\n" + log)


def ios(build):
    candidates = list(Path(build).rglob("DMXPulseMobileTest.app"))
    if len(candidates) != 1:
        raise RuntimeError(f"Expected one iOS test app: {candidates}")
    devices = json.loads(subprocess.check_output(["xcrun", "simctl", "list", "devices", "available", "-j"], text=True, timeout=60))["devices"]
    available = [d for runtime, group in devices.items() if "iOS" in runtime for d in group if d["isAvailable"]]
    if not available:
        raise RuntimeError("No installed iOS simulator runtime")
    device = available[0]["udid"]
    if available[0]["state"] != "Booted":
        run("xcrun", "simctl", "boot", device, timeout=90)
    run("xcrun", "simctl", "bootstatus", device, "-b", timeout=180)
    run("xcrun", "simctl", "install", device, str(candidates[0]), timeout=90)
    result = run("xcrun", "simctl", "launch", "--console", device, BUNDLE, capture_output=True, timeout=90)
    log = result.stdout + result.stderr
    print(log)
    if PASS not in log or FAIL in log:
        raise RuntimeError("iOS test did not pass")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", choices=("android", "ios"))
    parser.add_argument("build")
    args = parser.parse_args()
    (android if args.platform == "android" else ios)(args.build)

if __name__ == "__main__":
    main()
