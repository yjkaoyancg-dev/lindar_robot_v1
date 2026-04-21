#!/usr/bin/env bash
set -e
MODE="${1:-}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE_FILE="$ROOT_DIR/scripts/.detector_mode"
case "$MODE" in
  cluster|range_density)
    echo "$MODE" > "$MODE_FILE"
    echo "当前检测模式已设置为: $MODE"
    ;;
  *)
    echo "用法: $0 {cluster|range_density}" >&2
    exit 1
    ;;
esac
