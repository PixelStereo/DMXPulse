#!/bin/bash
# Local macOS build. Compatible with the Bash version shipped by Apple.
set -Eeuo pipefail

fail() { printf '\nError: %s\n' "$*" >&2; exit 1; }
trap 'printf "\nBuild stopped at line %s. See the error above.\n" "$LINENO" >&2' ERR

launch_app=1
for argument in "$@"; do
  case "$argument" in
    --no-open) launch_app=0 ;;
    -h|--help)
      printf 'Usage: ./build.sh [--no-open]\n\nInstall missing macOS dependencies, configure CMake, build and open DMXPulse.\nUse --no-open to build without launching the app.\n'
      exit 0 ;;
    *) fail "Unknown option: $argument. Use --help for usage." ;;
  esac
done

[[ "$(/usr/bin/uname -s)" == Darwin ]] || fail 'This script requires macOS.'
[[ "$EUID" -ne 0 ]] || fail 'Run ./build.sh as your normal user, without sudo.'
repo_dir="$(cd -P -- "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"

# Avoid mixing Intel Homebrew libraries and ARM binaries in a Rosetta terminal.
if [[ "$(/usr/sbin/sysctl -in sysctl.proc_translated 2>/dev/null || true)" == 1 ]]; then
  exec /usr/bin/arch -arm64 /bin/bash "$repo_dir/build.sh" "$@"
fi

build_jobs="${DMXPULSE_BUILD_JOBS:-4}"
[[ "$build_jobs" =~ ^[1-9][0-9]*$ ]] || fail 'DMXPULSE_BUILD_JOBS must be a positive integer.'
build_dir="$repo_dir/build-macos"

printf '\nDMXPulse · macOS build\n'
if ! /usr/bin/xcode-select -p >/dev/null 2>&1 ||
   ! /usr/bin/xcrun --sdk macosx --find clang++ >/dev/null 2>&1 ||
   ! /usr/bin/xcrun --sdk macosx --show-sdk-path >/dev/null 2>&1; then
  printf '\nOpening Apple Command Line Tools installer…\n'
  /usr/bin/xcode-select --install || true
  fail 'Finish installing the Apple Command Line Tools, then run ./build.sh again.'
fi

find_brew() {
  if [[ "$(/usr/bin/uname -m)" == arm64 && -x /opt/homebrew/bin/brew ]]; then
    printf '%s\n' /opt/homebrew/bin/brew
  elif [[ "$(/usr/bin/uname -m)" == x86_64 && -x /usr/local/bin/brew ]]; then
    printf '%s\n' /usr/local/bin/brew
  else
    command -v brew || true
  fi
}

brew_bin="$(find_brew)"
if [[ -z "$brew_bin" ]]; then
  printf '\nInstalling Homebrew using its official installer…\n'
  brew_installer="$(/usr/bin/mktemp "${TMPDIR:-/tmp}/dmxpulse-homebrew.XXXXXX")"
  trap '/bin/rm -f "$brew_installer"' EXIT
  /usr/bin/curl --fail --location --show-error --silent \
    https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh \
    --output "$brew_installer"
  /bin/bash "$brew_installer"
  /bin/rm -f "$brew_installer"
  trap - EXIT
  brew_bin="$(find_brew)"
  [[ -n "$brew_bin" ]] || fail 'Homebrew installation did not finish successfully.'
fi

brew_prefix="$("$brew_bin" --prefix)"
export PATH="$brew_prefix/bin:$brew_prefix/sbin:$PATH"
missing=()
for formula in cmake qtbase; do
  if [[ -z "$("$brew_bin" list --versions "$formula" 2>/dev/null || true)" ]]; then
    missing+=("$formula")
  fi
done
if [[ "${#missing[@]}" -gt 0 ]]; then
  printf '\nInstalling missing dependencies: %s\n' "${missing[*]}"
  "$brew_bin" install "${missing[@]}"
else
  printf 'CMake and Qt are already installed.\n'
fi

cmake_bin="$("$brew_bin" --prefix cmake)/bin/cmake"
qt_prefix="$("$brew_bin" --prefix qtbase)"
macos_sdk="$(/usr/bin/xcrun --sdk macosx --show-sdk-path)"
cxx_compiler="$(/usr/bin/xcrun --sdk macosx --find clang++)"
app_version="$(/usr/bin/tr -d '[:space:]' < "$repo_dir/VERSION")"

printf '\nConfiguring Release build…\n'
"$cmake_bin" -S "$repo_dir" -B "$build_dir" -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$qt_prefix" \
  -DQt6_DIR="$qt_prefix/lib/cmake/Qt6" \
  -DCMAKE_CXX_COMPILER="$cxx_compiler" \
  -DCMAKE_OSX_SYSROOT="$macos_sdk" \
  -DCMAKE_OSX_ARCHITECTURES="$(/usr/bin/uname -m)" \
  -DDMXPULSE_VERSION="$app_version" \
  -DBUILD_TESTING=OFF \
  -DDMXPULSE_BUILD_MOBILE_TESTS=OFF

printf '\nBuilding DMXPulse (%s jobs)…\n' "$build_jobs"
"$cmake_bin" --build "$build_dir" --config Release --target DMXPulse --parallel "$build_jobs"

app_bundle="$build_dir/DMXPulse.app"
[[ -x "$app_bundle/Contents/MacOS/DMXPulse" ]] || fail "Application not found: $app_bundle"
printf '\nBuild complete: %s\n' "$app_bundle"
if [[ "$launch_app" == 1 ]]; then
  printf 'Opening the newly built application…\n'
  /usr/bin/open -n "$app_bundle"
fi
