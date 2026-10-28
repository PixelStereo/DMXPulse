#!/usr/bin/env bash
set -euo pipefail
# All credential material is confined to the ephemeral GitHub runner.
required=(IOS_CERTIFICATE_BASE64 IOS_CERTIFICATE_PASSWORD IOS_PROFILE_BASE64 APPLE_TEAM_ID)
for name in "${required[@]}"; do test -n "${!name:-}" || { echo "Missing signing secret: $name" >&2; exit 1; }; done
keychain="$RUNNER_TEMP/dmxpulse-signing.keychain-db"
password="$(openssl rand -hex 24)"
cleanup() { security delete-keychain "$keychain" >/dev/null 2>&1 || true; rm -f "$RUNNER_TEMP/dmxpulse.p12" "$RUNNER_TEMP/dmxpulse.mobileprovision"; }
trap cleanup EXIT
python3 - <<'PY'
import base64, os
from pathlib import Path
root=Path(os.environ['RUNNER_TEMP'])
for name,output in [('IOS_CERTIFICATE_BASE64','dmxpulse.p12'),('IOS_PROFILE_BASE64','dmxpulse.mobileprovision')]:
    (root/output).write_bytes(base64.b64decode(os.environ[name],validate=True))
PY
security create-keychain -p "$password" "$keychain"
security set-keychain-settings -lut 21600 "$keychain"
security unlock-keychain -p "$password" "$keychain"
security import "$RUNNER_TEMP/dmxpulse.p12" -k "$keychain" -P "$IOS_CERTIFICATE_PASSWORD" -T /usr/bin/codesign >/dev/null
security set-key-partition-list -S apple-tool:,apple: -k "$password" "$keychain" >/dev/null
security list-keychains -d user -s "$keychain" login.keychain-db
security cms -D -i "$RUNNER_TEMP/dmxpulse.mobileprovision" > "$RUNNER_TEMP/profile.plist"
profile_uuid="$(/usr/libexec/PlistBuddy -c 'Print UUID' "$RUNNER_TEMP/profile.plist")"
profile_name="$(/usr/libexec/PlistBuddy -c 'Print Name' "$RUNNER_TEMP/profile.plist")"
python3 - <<'PY'
import os,plistlib
with open(os.path.join(os.environ['RUNNER_TEMP'],'profile.plist'),'rb') as f:p=plistlib.load(f)
if not p.get('Entitlements',{}).get('com.apple.developer.networking.multicast'):
    raise SystemExit('Provisioning profile must include the approved multicast entitlement')
PY
mkdir -p "$HOME/Library/MobileDevice/Provisioning Profiles"
cp "$RUNNER_TEMP/dmxpulse.mobileprovision" "$HOME/Library/MobileDevice/Provisioning Profiles/$profile_uuid.mobileprovision"
qt-cmake -S . -B build-ios-signed -G Xcode -DBUILD_TESTING=OFF -DDMXPULSE_IOS_SIGNING=ON -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DDMXPULSE_VERSION="$APP_VERSION" -DDMXPULSE_BUILD_NUMBER="$APP_BUILD_NUMBER"
xcodebuild -project build-ios-signed/DMXPulse.xcodeproj -scheme DMXPulse -configuration Release -sdk iphoneos -archivePath "$PWD/build-ios-signed/DMXPulse.xcarchive" archive CODE_SIGN_STYLE=Manual DEVELOPMENT_TEAM="$APPLE_TEAM_ID" PROVISIONING_PROFILE_SPECIFIER="$profile_name" CODE_SIGN_IDENTITY="Apple Distribution"
export IOS_EXPORT_METHOD="${IOS_EXPORT_METHOD:-release-testing}"
export IOS_PROFILE_NAME="$profile_name"
python3 - <<'PY'
import os,plistlib
options={'method':os.environ['IOS_EXPORT_METHOD'],'teamID':os.environ['APPLE_TEAM_ID'],'signingStyle':'manual','provisioningProfiles':{'fr.pixelstereo.dmxpulse':os.environ['IOS_PROFILE_NAME']}}
with open('build-ios-signed/ExportOptions.plist','wb') as f:plistlib.dump(options,f)
PY
xcodebuild -exportArchive -archivePath build-ios-signed/DMXPulse.xcarchive -exportOptionsPlist build-ios-signed/ExportOptions.plist -exportPath build-ios-signed/export
cp build-ios-signed/export/DMXPulse.ipa dist/DMXPulse-ios.ipa

# Publish exactly one iOS device package after a successful signed export.
rm -f dist/DMXPulse-ios-unsigned.ipa
