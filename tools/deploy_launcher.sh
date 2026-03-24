#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-out/build/x64-Release}"
DIST_DIR="${DIST_DIR:-dist/deploy-launcher}"
BASE_URL="${BASE_URL:-https://your-patch-server.com}"
STAGE_DIR="${STAGE_DIR:-dist/deploy-stage}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_PATH="$ROOT_DIR/$BUILD_DIR"
DIST_PATH="$ROOT_DIR/$DIST_DIR"
STAGE_PATH="$ROOT_DIR/$STAGE_DIR"

if [[ ! -f "$BUILD_PATH/launcher.exe" ]]; then
  echo "launcher.exe not found in $BUILD_PATH" >&2
  exit 1
fi

pwsh -ExecutionPolicy Bypass -File "$ROOT_DIR/tools/package_release.ps1" -BuildDir "$BUILD_DIR" -OutputDir "$DIST_DIR" -Zip

VERSION="$(strings "$BUILD_PATH/launcher.exe" | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1 || true)"
if [[ -z "$VERSION" ]]; then
  VERSION="0.0.0"
fi

rm -rf "$STAGE_PATH"
mkdir -p "$STAGE_PATH/launcher"

cp "$DIST_PATH/launcher.exe" "$STAGE_PATH/launcher/launcher.exe"
pwsh -NoProfile -ExecutionPolicy Bypass -Command "\$zipPath = Join-Path '$STAGE_PATH' 'launcher/launcher.zip'; if (Test-Path \$zipPath) { Remove-Item \$zipPath -Force }; Compress-Archive -Path (Join-Path '$DIST_PATH' '*') -DestinationPath \$zipPath -Force -Exclude 'helper\\autoupdate.exe'"

cat > "$STAGE_PATH/launcher/version.json" <<EOF
{
  "version": "$VERSION",
  "launcher_url": "$BASE_URL/launcher/launcher.zip"
}
EOF

if [[ -f "$DIST_PATH/helper/autoupdate.exe" ]]; then
  mkdir -p "$STAGE_PATH/launcher/helper"
  cp "$DIST_PATH/helper/autoupdate.exe" "$STAGE_PATH/launcher/helper/autoupdate.exe"
fi

echo "Prepared launcher deployment stage in $STAGE_PATH"
echo "Upload:"
echo "  $STAGE_PATH/launcher/launcher.exe"
echo "  $STAGE_PATH/launcher/launcher.zip"
echo "  $STAGE_PATH/launcher/version.json"
if [[ -f "$STAGE_PATH/launcher/helper/autoupdate.exe" ]]; then
  echo "  $STAGE_PATH/launcher/helper/autoupdate.exe"
fi
