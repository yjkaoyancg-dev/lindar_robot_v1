#!/usr/bin/env bash
set -e

LOGDIR="$HOME/photonix_logs"
mkdir -p "$LOGDIR"

cd "$HOME/work/Photonix"
source /opt/ros/iron/setup.bash
source install/setup.bash

# 清理旧进程
pkill -9 -f "lidar_node" || true
pkill -9 -f "range_density_detector_node" || true
pkill -9 -f "range_plc_bridge_node" || true
pkill -9 -f "robot_plc_crontorl_node" || true

sleep 2

echo "[1/4] 启动 lidar_node ..."
nohup stdbuf -oL -eL ros2 run lidar_module lidar_node \
  > "$LOGDIR/lidar.log" 2>&1 &

sleep 2

echo "[2/4] 启动 range_density_detector_node ..."
nohup stdbuf -oL -eL ros2 run detect_module range_density_detector_node \
  --ros-args -p config_path:=/opt/zwkj/configs/range_density_detect_config.json \
  > "$LOGDIR/range_detector.log" 2>&1 &

sleep 2

echo "[3/4] 启动 range_plc_bridge_node ..."
nohup stdbuf -oL -eL ros2 run detect_module range_plc_bridge_node \
  --ros-args \
  -p timeout_ms:=8000 \
  -p freshness_ms:=8000 \
  -p use_latest_if_fresh:=false \
  -p zero_on_timeout:=false \
  > "$LOGDIR/range_bridge.log" 2>&1 &

sleep 2

echo "[4/4] 启动 robot_plc_crontorl_node ..."
nohup stdbuf -oL -eL ros2 run robot_plc_crontorl robot_plc_crontorl_node \
  > "$LOGDIR/plc.log" 2>&1 &

sleep 3

echo
echo "全部启动完成。日志目录: $LOGDIR"
echo
ps -ef | grep -E "lidar_node|range_density_detector_node|range_plc_bridge_node|robot_plc_crontorl_node" | grep -v grep
