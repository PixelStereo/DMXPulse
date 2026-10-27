# Build and release DMXPulse

For installation and everyday use, see the [user guide](guide.md). This document is for developers and release maintainers.

## Requirements

- CMake 3.21+, a C++17 compiler, and Qt 6.4+ Core/Gui/Widgets/Network.
- Qt Test for desktop regression tests; use `-DBUILD_TESTING=OFF` for application-only builds.
- Python 3.10+ for packaging and release checks.
- Mobile builds use the Qt 6.8.3 SDKs in CI. Match SDK/NDK/JDK and compiler versions to the Qt kit.

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

Use the matching Qt macOS kit and architecture (`arm64` or `x86_64`). The executable is `build/DMXPulse.app`.

```sh
python3 scripts/package_desktop.py macos --arch arm64
```

The packager runs `macdeployqt`, rejects non-system absolute runtime dependencies, adds notices, applies and verifies an ad-hoc signature, launches a temporary-settings smoke test, and creates ZIP and DMG packages. It does not notarize or apply a Developer ID signature. The official Qt SDK is used in CI to avoid relying on unrelated package-manager runtimes.

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

## Android

CI builds an ARM64 release APK and a separate x86-64 emulator test application. Use JDK 17, Android SDK platform/build tools 35, NDK `26.1.10909125`, and Qt 6.8.3 for the selected ABI, plus the matching desktop host tools. The Qt installer action uses `--autodesktop` to fetch host tools.

```sh
/path/to/Qt/6.8.3/android_arm64_v8a/bin/qt-cmake -S . -B build-android \
  -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
  -DANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/26.1.10909125" \
  -DDMXPULSE_BUILD_NUMBER=1
cmake --build build-android --target apk --parallel 3
python3 scripts/package_android.py
```

The manifest declares Internet, network state, Wi-Fi state and multicast-lock permissions. Listening acquires a non-reference-counted Wi-Fi multicast lock and releases it when stopped. The mobile UI uses Monitor / Inspect / Output tabs and a scrollable eight-column grid. Network behavior still needs physical-device testing on the intended Wi-Fi infrastructure.

### Android signing secrets

Configure repository Actions secrets:

| Secret | Value |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | Base64-encoded persistent signing keystore |
| `ANDROID_KEYSTORE_PASSWORD` | Store password |
| `ANDROID_KEY_ALIAS` | Signing alias |
| `ANDROID_KEY_PASSWORD` | Key password |

If no keystore is configured, CI signs an installable test APK using an ephemeral key. Such APKs are for evaluation: a later build may require uninstalling the previous installation. A persistent key is required for reliable upgrades. Keystore files and passwords are not added to the repository or release assets.

## iOS / iPadOS

Use macOS, Xcode, the Qt 6.8.3 iOS kit and CMake's Xcode generator. CI builds device ARM64 and Intel simulator variants. The simulator variant uses x86-64 because that is the simulator architecture included by the selected Qt kit; use a matching Intel simulator environment.

```sh
/path/to/Qt/6.8.3/ios/bin/qt-cmake -S . -B build-ios -G Xcode \
  -DBUILD_TESTING=OFF -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build build-ios --config Release --parallel 3
```

Unsigned device builds demonstrate compilation and are provided for signing/rebuilding; they are not installable IPAs. The custom plist includes the local-network usage description. Broadcast/multicast on physical iOS devices requires the approved `com.apple.developer.networking.multicast` entitlement in the provisioning profile. Unicast is available without that entitlement, subject to local-network permission.

### Optional signed IPA

Configure these Actions secrets to add `DMXPulse-ios.ipa` to a release:

| Secret | Value |
|---|---|
| `IOS_CERTIFICATE_BASE64` | Base64 Apple Distribution `.p12` |
| `IOS_CERTIFICATE_PASSWORD` | Password for that certificate |
| `IOS_PROFILE_BASE64` | Base64 provisioning profile for `fr.pixelstereo.dmxpulse`, with approved multicast entitlement |
| `APPLE_TEAM_ID` | Apple developer team identifier |

Optional repository variable `IOS_EXPORT_METHOD` defaults to `release-testing` (Xcode 16 terminology). Use a method matching the certificate/profile and your distribution plan. The workflow creates a temporary keychain, imports existing signing material, checks the multicast entitlement, archives and exports with manual signing, then removes the temporary keychain. It does not create Apple certificates/profiles, register devices, upload to App Store Connect, or bypass provisioning requirements.

## Mobile tests

`-DDMXPULSE_BUILD_MOBILE_TESTS=ON` builds `DMXPulseMobileTest`, with an isolated application identity and temporary workspace. It verifies both packet encoders/decoders and that editing an output channel cannot populate the received grid.

- Android CI boots a hardware-accelerated x86-64 emulator and requires the explicit `DMXPULSE_MOBILE_TEST_PASS` log marker.
- iOS CI installs the test on an available simulator and requires the same marker.
- Merely building or launching an application does not count as a test pass.

## Tag-driven releases

Every pushed tag triggers all builds. Pushes to `main`, pull requests and manual dispatch also validate/package, without publishing a release unless the ref is a tag.

```sh
git tag v1.1.0
git push origin v1.1.0
```

Do not move published tags. Numeric tags (`v1.2`, `1.2.3`, `v1.2.3-rc.1`) determine the embedded numeric application version. Other tag names use the checked-in `VERSION`; the exact tag still identifies the GitHub release. `github.run_number` supplies the increasing mobile build number.

Required release assets:

- macOS ARM64 and x86-64 ZIPs (plus DMGs).
- Windows x86-64 portable ZIP.
- Linux AMD64 and Raspberry Pi ARM64 DEBs.
- Android ARM64 signed APK.
- iOS unsigned ARM64 device and x86-64 simulator ZIPs.
- A source archive, `release.json` and `SHA256SUMS`.
- A signed iOS IPA when Apple signing secrets are configured.

The release job downloads only package artifacts, validates the complete required set and archive integrity, computes checksums, creates a draft release, uploads assets, and publishes it. Missing packages or a failed desktop/mobile/sanitizer job block publication. Signing state is documented rather than presenting unsigned iOS artifacts as installable apps.

## Checks and diagnostics

The workflow retains failed CTest logs. Tests use isolated ports and temporary settings; loopback traffic must be permitted. The physical-adapter regression is skipped if no non-loopback IPv4 adapter exists.

See [VALIDATION.md](../VALIDATION.md) for additional local sanitizer commands and [THIRD-PARTY-NOTICES.md](../THIRD-PARTY-NOTICES.md) for runtime distribution information. Build reproducibility depends on the pinned Qt/toolchain versions and the package distribution's repositories; this is not a bit-for-bit reproducible-build guarantee.
