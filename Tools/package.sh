#!/usr/bin/env bash
# Build the watch app and pack the community-app-store upload:
#   Output/SkiDay_<version>_store.zip = .uapp + app-manifest.json + icon.png + previews/
# Usage: UNA_SDK=/path/to/una-sdk Tools/package.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
: "${UNA_SDK:?Set UNA_SDK to the una-sdk checkout}"
CMAKE_DIR="$ROOT/Software/Apps/SkiDay-CMake"
BUILD="$ROOT/build/watch"

cmake -G "Unix Makefiles" -S "$CMAKE_DIR" -B "$BUILD"
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 4)"

UAPP=$(ls -t "$BUILD"/SkiDay_*.uapp | head -1)
NAME=$(basename "$UAPP")
VERSION=${NAME#SkiDay_}; VERSION=${VERSION%.uapp}; VERSION=${VERSION%%-*}
APP_ID=$(sed -n 's/^set(APP_ID "\([0-9A-F]*\)").*/\1/p' "$CMAKE_DIR/CMakeLists.txt")

STAGE=$(mktemp -d)
cp "$UAPP" "$STAGE/"
cp "$ROOT/Resources/icon.png" "$STAGE/icon.png"
mkdir -p "$STAGE/previews"
cp "$ROOT"/Docs/screenshots/store_*.png "$STAGE/previews/"

# The flags below describe what the FIT file holds: a lap per run and per
# lift ride, distance, the GPS track, elevation and speed (no heart rate or
# steps).
cat > "$STAGE/app-manifest.json" <<JSON
{
  "manifest_version": 1,
  "type": ["activity"],
  "name": "Ski Day",
  "icon": "icon.png",
  "binary": "$NAME",
  "previews": "previews/",
  "appVersion": "$VERSION",
  "minKernelVersion": "1.4.0",
  "requiredHardware": ["GPS"],
  "description": "A ski and snowboard day tracker. Just go ski: runs count themselves. The barometer tells lift rides from runs (even in a gondola with no GPS), and each run gets its vertical drop, distance, top and average speed and time, shown as a recap while you ride the next lift. The day adds up runs, total vertical, top speed and time skiing vs on lifts, with an altitude profile and a runs list at the end. Storm watch buzzes when the air pressure falls fast (3 hPa in 3 h by default), comparing only readings taken while you stand at the same spot, such as a lift queue, so skiing downhill is never mistaken for a storm. Records a FIT file (alpine skiing or snowboarding) with a lap per run and per lift. Buttons only, glove-friendly. Includes a demo day you can play at 1x, 10x or 60x.",
  "stravaExport": true,
  "id": "$APP_ID",
  "supportsLaps": true,
  "supportsDistance": true,
  "supportsTrack": true,
  "supportsHeartbeat": false,
  "supportsElevation": true,
  "supportsStep": false,
  "supportsSpeed": "speed",
  "customMeasures": []
}
JSON
python3 "$UNA_SDK/Utilities/Scripts/app_packer/min_kernel_version.py" --stamp "$STAGE/app-manifest.json"

mkdir -p "$ROOT/Release"
ZIP="$ROOT/Release/SkiDay_${VERSION}_store.zip"
rm -f "$ZIP"
(cd "$STAGE" && zip -qr "$ZIP" .)
cp "$UAPP" "$ROOT/Release/"
rm -rf "$STAGE"
echo "Store package: $ZIP"
unzip -l "$ZIP"
