#!/usr/bin/env bash
set -e
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
sudo mkdir -p /opt/zwkj/configs /opt/zwkj/templates
sudo cp "$ROOT_DIR/configs/runtime/"*.json /opt/zwkj/configs/
# 默认安装当前运行时模板；如需聚类模板，可手动从 templates/cluster_default 复制覆盖
sudo rm -rf /opt/zwkj/templates/*
sudo cp -r "$ROOT_DIR/templates/runtime_current/." /opt/zwkj/templates/
echo '已复制运行时 configs 和 templates 到 /opt/zwkj。'
