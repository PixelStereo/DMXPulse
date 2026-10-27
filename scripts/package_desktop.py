#!/usr/bin/env python3
"""Stage Qt runtime dependencies, smoke-test, and archive desktop packages."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import zipfile
import stat

ROOT = Path(__file__).resolve().parents[1]

def run(*args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)

def notices(destination):
    for name in ("LICENSE", "THIRD-PARTY-NOTICES.md"):
        shutil.copy2(ROOT / name, destination)
    if (ROOT / "licenses").exists():
        shutil.copytree(ROOT / "licenses", destination / "licenses", dirs_exist_ok=True)

def smoke(executable):
    # Exercise the shipped native platform plugin, with SDK overrides removed.
    env = os.environ.copy()
    for name in ("QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH", "QT_QPA_PLATFORM", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH"):
        env.pop(name, None)
    deploy = shutil.which("windeployqt") or shutil.which("macdeployqt")
    if deploy:
        sdk_bin = Path(deploy).parent.resolve()
        env["PATH"] = os.pathsep.join(p for p in env.get("PATH", "").split(os.pathsep) if p and Path(p).resolve() != sdk_bin)
    run(executable, "--smoke-test", timeout=30, env=env)

def macos(build, output, arch):
    stage = output / "stage-macos"
    stage.mkdir(parents=True, exist_ok=True)
    app = stage / "DMXPulse.app"
    shutil.copytree(build / "DMXPulse.app", app, symlinks=True, dirs_exist_ok=True)
    result = subprocess.run(["macdeployqt", str(app), "-always-overwrite"], text=True, capture_output=True)
    print(result.stdout)
    print(result.stderr)
    if result.returncode or "ERROR:" in result.stdout + result.stderr:
        raise RuntimeError("macdeployqt reported an error")
    notices(app / "Contents/Resources")
    for path in (app / "Contents").rglob("*"):
        if path.is_file() and not path.is_symlink():
            kind = subprocess.check_output(["file", "-b", str(path)], text=True)
            if "Mach-O" in kind:
                dependencies = subprocess.check_output(["otool", "-L", str(path)], text=True)
                for line in dependencies.splitlines():
                    # Universal binaries print one non-indented header per architecture.
                    if not line.startswith(("\t", " ")):
                        continue
                    dependency = line.strip().split(" (", 1)[0]
                    if dependency.startswith(str(app / "Contents/Frameworks") + "/"):
                        # Some SDK framework IDs are absolute *inside* the staged app.
                        # Rewrite both LC_ID_DYLIB and references before moving the bundle.
                        relative = Path(dependency).relative_to(app / "Contents/Frameworks")
                        path.chmod(path.stat().st_mode | stat.S_IWUSR)
                        if Path(dependency).resolve() == path.resolve():
                            run("install_name_tool", "-id", "@rpath/" + relative.as_posix(), path)
                        else:
                            run("install_name_tool", "-change", dependency, "@executable_path/../Frameworks/" + relative.as_posix(), path)
                    elif dependency.startswith("/") and not dependency.startswith(("/System/Library/", "/usr/lib/")):
                        raise RuntimeError(f"External runtime dependency in {path}: {dependency}")
    run("codesign", "--force", "--deep", "--sign", "-", app)
    run("codesign", "--verify", "--deep", "--strict", app)
    smoke(app / "Contents/MacOS/DMXPulse")
    run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", app, output / f"DMXPulse-macos-{arch}.zip")
    (stage / "Applications").symlink_to("/Applications")
    run("hdiutil", "create", "-volname", "DMXPulse", "-srcfolder", stage, "-ov", "-format", "UDZO", output / f"DMXPulse-macos-{arch}.dmg")
    shutil.rmtree(stage)

def windows(build, output):
    stage = output / "DMXPulse"
    stage.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / "DMXPulse.exe", stage)
    run("windeployqt", "--release", "--no-translations", "--compiler-runtime", stage / "DMXPulse.exe")
    redist = os.environ.get("VCToolsRedistDir")
    if not redist:
        raise RuntimeError("VCToolsRedistDir is unavailable; run in the MSVC developer environment")
    crt_dirs = sorted((Path(redist) / "x64").glob("Microsoft.VC*.CRT"))
    if not crt_dirs:
        raise RuntimeError("No MSVC x64 runtime found")
    for dll in crt_dirs[-1].glob("*.dll"):
        shutil.copy2(dll, stage)
    notices(stage)
    smoke(stage / "DMXPulse.exe")
    with zipfile.ZipFile(output / "DMXPulse-windows-x86_64.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(output))
    shutil.rmtree(stage)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", choices=("macos", "windows"))
    parser.add_argument("--build", default="build")
    parser.add_argument("--output", default="dist")
    parser.add_argument("--arch", default="arm64")
    args = parser.parse_args()
    build, output = Path(args.build).resolve(), Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    if args.platform == "macos":
        macos(build, output, args.arch)
    else:
        windows(build, output)

if __name__ == "__main__":
    main()
