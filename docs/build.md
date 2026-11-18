# Build and release DMXPulse

For installation and everyday use, see the [user guide](guide.md). This document is for developers and release maintainers.

## Requirements

- CMake 3.21+, a C++17 compiler, and Qt 6.4+ Core/Gui/Widgets/Network.
- Qt Test for desktop regression tests; use `-DBUILD_TESTING=OFF` for application-only builds.
- Python 3.10+ for packaging and release checks.

The [build workflow](../.github/workflows/release.yml) is the executable specification for CI dependencies and commands. It follows a build → test → package → smoke-test → publish sequence. A failed required job prevents release publication.

## Desktop

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/platform
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --timeout 90
python3 -m unittest discover -s scripts -p 'test_*.py'
```

### macOS

For a local build, run this single command from the repository root:

```sh
./build.sh
```

The executable script installs Homebrew if missing using its [official installer](https://brew.sh), installs missing CMake and Qt Base dependencies, configures a native Release build in `build-macos`, compiles the application, and opens the freshly built copy. Existing dependencies and build objects are reused. It also works when invoked using its absolute path from another directory. The application uses the locally installed Homebrew Qt runtime; use the CI packages or the packager below for distribution.

If Apple Command Line Tools are missing, the script opens Apple's installer. Finish that installation and run `./build.sh` again. Homebrew's first installation may ask for your administrator password. Subsequent builds need only the same command.

Use `./build.sh --no-open` to compile without launching, or `DMXPULSE_BUILD_JOBS=8 ./build.sh` to change the default four parallel build jobs. By default the script launches a new instance of the freshly built application.

For manual builds and distribution, use the matching Qt macOS kit and architecture (`arm64` or `x86_64`). The executable is `build/DMXPulse.app`.

```sh
python3 scripts/package_desktop.py macos --arch arm64
```

The packager runs `macdeployqt`, rejects non-system absolute runtime dependencies, adds notices, applies and verifies an ad-hoc signature, launches a temporary-settings smoke test, and creates one zip (made with `ditto`, then extracted, signature-checked and launched) for the selected architecture. It does not notarize or apply a Developer ID signature. The official Qt SDK is used in CI to avoid relying on unrelated package-manager runtimes.

### Windows

Use an MSVC 2022 developer environment and the `win64_msvc2022_64` Qt 6.8.3 kit. Configure with `-G Ninja -DCMAKE_BUILD_TYPE=Release`. `windeployqt` and Python must be on PATH; `VCToolsRedistDir` must identify the matching redistributable directory.

```powershell
python scripts/package_desktop.py windows
```

The portable ZIP contains Qt, platform plugins and MSVC CRT DLLs. The staged executable must pass a smoke test before packaging succeeds.

### Linux and Raspberry Pi

CI builds natively in Debian Bookworm containers, on x86-64 and ARM64 runners. This keeps package dependencies aligned with Debian 12 / 64-bit Raspberry Pi OS Bookworm. A graphical desktop is required; 32-bit ARM is not currently packaged.

```sh
sudo apt-get install build-essential cmake ninja-build qt6-base-dev \
  libqt6opengl6-dev libgl1-mesa-dev dpkg-dev python3
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
(cd build && cpack -G DEB)
sudo apt install ./build/dmxpulse_*.deb
DMXPulse --smoke-test -platform offscreen
```

CPack computes shared-library dependencies using `dpkg-shlibdeps`. DEB packages rely on the distribution's Qt runtime; they are not self-contained AppImages.

## Tag-driven releases

Every pushed tag triggers all builds. Pushes to `main`, pull requests and manual dispatch also validate/package, without publishing a release unless the ref is a tag.

```sh
git tag v1.1.0
git push origin v1.1.0
```

Do not move published tags. Numeric tags (`v1.2`, `1.2.3`, `v1.2.3-rc.1`) determine the embedded numeric application version. Other tag names use the checked-in `VERSION`; the exact tag still identifies the GitHub release.

Required release assets:

- One macOS ARM64 zip and one macOS x86-64 zip, and one Windows installer.
- Windows x86-64 portable ZIP.
- Linux AMD64 and Raspberry Pi ARM64 DEBs.

Only these packages are attached to releases. Duplicate macOS ZIPs, the mobile packages DMXPulse no longer builds, custom source archives and metadata files are removed when updating an existing release. GitHub provides its own source downloads. License notices remain embedded in the application.

The release job downloads package artifacts, validates the complete required set and archive integrity, creates a draft release, uploads only the validated packages, and publishes it. Missing packages or a failed desktop or sanitizer job block publication.

## Checks and diagnostics

The workflow retains failed CTest logs. Tests use isolated ports and temporary settings; loopback traffic must be permitted. The physical-adapter regression is skipped if no non-loopback IPv4 adapter exists. Exact duplicate-unicast port sharing is exercised on Unix; Windows tests isolate the receiver because its documented `SO_REUSEADDR` delivery between duplicate binds is indeterminate.

See [VALIDATION.md](../VALIDATION.md) for additional local sanitizer commands and [THIRD-PARTY-NOTICES.md](../THIRD-PARTY-NOTICES.md) for runtime distribution information. Build reproducibility depends on the pinned Qt/toolchain versions and the package distribution's repositories; this is not a bit-for-bit reproducible-build guarantee.
