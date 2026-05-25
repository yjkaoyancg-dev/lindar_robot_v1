#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${ROOT_DIR}/operator_gui_pr1.zip"

cd "$ROOT_DIR"
rm -f "$OUT"

zip -r "$OUT" operator_gui \
  -x "operator_gui/**/__pycache__/*" \
  -x "operator_gui/**/*.pyc"

echo "Created: $OUT"

