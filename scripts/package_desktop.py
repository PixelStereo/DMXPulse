#!/usr/bin/env python3
"""Stage Qt runtime dependencies, smoke-test, and package desktop downloads: a zip on macOS, an installer on Windows."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import stat
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "third_party/pxs/scripts"))
from build_id import display_version  # noqa: E402  the version the application carries in its name


def app_name():
    """"DMXPulse 0.2.1" for a release, "DMXPulse 0.2.1+3" three commits after it, "DMXPulse 0.2.1+3+" with changes."""
    fallback = (ROOT / "VERSION").read_text().strip()
    return f"DMXPulse {display_version(ROOT, os.environ.get('RELEASE_TAG') or None, fallback)}"

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
    name = app_name()
    app = stage / f"{name}.app"  # the executable inside keeps its name; the bundle carries the version
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
    # ditto, not Python's zipfile: it keeps the symlinks inside the frameworks, which the signature depends on
    archive = output / f"DMXPulse-macos-{arch}.zip"
    archive.unlink(missing_ok=True)
    run("ditto", "-c", "-k", "--keepParent", app, archive)
    # What users get is the zip, so the checks run on its extracted content
    with tempfile.TemporaryDirectory() as check:
        run("ditto", "-x", "-k", archive, check)
        extracted = Path(check) / f"{name}.app"
        run("codesign", "--verify", "--deep", "--strict", extracted)
        smoke(extracted / "Contents/MacOS/DMXPulse")
    shutil.rmtree(stage)

def windows(build, output):
    stage = output / "DMXPulse"
    stage.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / "DMXPulse.exe", stage)
    run("windeployqt", "--release", "--no-translations", "--no-compiler-runtime", stage / "DMXPulse.exe")
    name = app_name()
    (stage / "DMXPulse.exe").rename(stage / f"{name}.exe")  # the version is in the name, as on macOS
    redist = os.environ.get("VCToolsRedistDir")
    if not redist:
        raise RuntimeError("VCToolsRedistDir is unavailable; run in the MSVC developer environment")
    crt_dirs = sorted((Path(redist) / "x64").glob("Microsoft.VC*.CRT"))
    if not crt_dirs:
        raise RuntimeError("No MSVC x64 runtime found")
    for dll in crt_dirs[-1].glob("*.dll"):
        shutil.copy2(dll, stage)
    notices(stage)
    smoke(stage / f"{name}.exe")
    version = os.environ.get("APP_VERSION", "0.0.0")
    installer = output / "DMXPulse-windows-x86_64-Setup.exe"
    makensis = shutil.which("makensis") or str(Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "NSIS/makensis.exe")
    run(makensis, "/DPRODUCT=DMXPulse", f"/DAPP_VERSION={name.removeprefix('DMXPulse ')}", f"/DAPP_EXE={name}.exe",
        f"/DPAYLOAD={stage}", f"/DOUTPUT={installer}", ROOT / "third_party/pxs/packaging/windows.nsi")
    # What users get is the installer: install it silently somewhere clean and start what it installed
    with tempfile.TemporaryDirectory() as clean:
        run(installer, "/S", "/CurrentUser", f"/D={clean}")
        smoke(Path(clean) / "app" / f"{name}.exe")
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
