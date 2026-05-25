#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if [ -f /opt/ros/iron/setup.bash ]; then
  source /opt/ros/iron/setup.bash
fi

if [ -f install/setup.bash ]; then
  source install/setup.bash
fi

python3 -m operator_gui.operator_gui.main

