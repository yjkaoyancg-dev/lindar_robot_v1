#!/usr/bin/env bash
set -e

ZIP_PATH="${1:-operator_gui_pr4_visualization.zip}"
TARGET_ROOT="${HOME}/deploy_v1"

if [ ! -f "$ZIP_PATH" ]; then
  echo "Zip not found: $ZIP_PATH" >&2
  exit 1
fi

mkdir -p "$TARGET_ROOT"
unzip -o "$ZIP_PATH" -d "$TARGET_ROOT"

REQ="${TARGET_ROOT}/operator_gui/requirements.txt"
if [ -f "$REQ" ]; then
  python3 -m pip install -r "$REQ"
fi

echo "Installed operator GUI PR4 to ${TARGET_ROOT}/operator_gui"

