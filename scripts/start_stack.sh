#!/usr/bin/env bash
set -e
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE_FILE="$ROOT_DIR/scripts/.detector_mode"
MODE="range_density"
[ -f "$MODE_FILE" ] && MODE="$(cat "$MODE_FILE")"
LOGDIR="$HOME/photonix_logs"
mkdir -p "$LOGDIR"
cd "$ROOT_DIR"
source /opt/ros/iron/setup.bash
source install/setup.bash
"$ROOT_DIR/scripts/stop_stack.sh" || true
sleep 1
nohup stdbuf -oL -eL ros2 run lidar_module lidar_node > "$LOGDIR/lidar.log" 2>&1 &
sleep 2
if [ "$MODE" = "cluster" ]; then
  nohup stdbuf -oL -eL ros2 run detect_module detect_module > "$LOGDIR/detector.log" 2>&1 &
else
  nohup stdbuf -oL -eL ros2 run detect_module range_density_detector_node \
    --ros-args -p config_path:=/opt/zwkj/configs/range_density_detect_config.json \
    > "$LOGDIR/range_detector.log" 2>&1 &
  sleep 1
  nohup stdbuf -oL -eL ros2 run detect_module range_plc_bridge_node \
    --ros-args -p timeout_ms:=8000 -p freshness_ms:=8000 \
    -p use_latest_if_fresh:=false -p zero_on_timeout:=false \
    > "$LOGDIR/range_bridge.log" 2>&1 &
fi
sleep 2
nohup stdbuf -oL -eL ros2 run robot_plc_crontorl robot_plc_crontorl_node > "$LOGDIR/plc.log" 2>&1 &
echo "启动完成，当前模式: $MODE"
ps -ef | grep -E 'lidar_node|detect_module|range_density_detector_node|range_plc_bridge_node|robot_plc_crontorl_node' | grep -v grep || true
