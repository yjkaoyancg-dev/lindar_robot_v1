#!/usr/bin/env bash
pkill -9 -f "lidar_node" || true
pkill -9 -f "range_density_detector_node" || true
pkill -9 -f "range_plc_bridge_node" || true
pkill -9 -f "robot_plc_crontorl_node" || true
echo "已停止 Photonix 相关节点。"
