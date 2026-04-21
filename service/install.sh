#!/bin/bash

# 部署脚本
# 注意：此脚本需要以root权限运行，以便将服务文件复制到系统目录。

set -e  # 出错时自动退出，避免部分失败导致后续错误

# 定义源目录（假设脚本在当前目录运行）
SOURCE_DIR="."

# 定义目标目录
SERVICE_TARGET="/etc/systemd/system/"
CONFIG_TARGET="/opt/zwkj/configs/"
NODES_BASE="/home/zwkj01/nodes/"
ENV_TARGET="/home/zwkj01/"
TEMPLATES_TARGET="/opt/zwkj/"

# 检查是否以root权限运行
if [ "$EUID" -ne 0 ]; then
    echo "错误：请使用 'sudo' 运行此脚本，因为需要将文件复制到系统目录。"
    exit 1
fi

# 创建所有必要的目标目录（如果不存在）
echo "创建目标目录..."
mkdir -p "$CONFIG_TARGET"
mkdir -p "$NODES_BASE/lidar"
mkdir -p "$NODES_BASE/plc"
mkdir -p "$NODES_BASE/detect"
mkdir -p "$ENV_TARGET"
mkdir -p "$TEMPLATES_TARGET"

# 1. 复制服务文件到 /etc/systemd/system/
echo "复制服务文件到 $SERVICE_TARGET ..."
cp "$SOURCE_DIR/detect_service/zwkjDetect.service" "$SERVICE_TARGET"
cp "$SOURCE_DIR/lidar_service/zwkjLidar.service" "$SERVICE_TARGET"
cp "$SOURCE_DIR/plc_service/zwkjPlc.service" "$SERVICE_TARGET"

# 2. 复制JSON配置文件到 /opt/zwkj/configs/
echo "复制配置文件到 $CONFIG_TARGET ..."
cp "$SOURCE_DIR/detect_service/detect_config.json" "$CONFIG_TARGET"
cp "$SOURCE_DIR/lidar_service/lidar_config.json" "$CONFIG_TARGET"
cp "$SOURCE_DIR/plc_service/robot_plc_crontorl.json" "$CONFIG_TARGET"

# 3. 移动可执行文件到节点目录，并赋予执行权限
echo "移动可执行文件到节点目录并赋权..."

# 处理detect可执行文件
DETECT_SRC="$SOURCE_DIR/detect_service/detect_module"
DETECT_DEST="$NODES_BASE/detect/detect_module"
if [ -f "$DETECT_SRC" ]; then
    mkdir -p "$NODES_BASE/detect"
    mv "$DETECT_SRC" "$DETECT_DEST"
    chmod +x "$DETECT_DEST"
    echo "已移动并赋权 detect_module 到 $DETECT_DEST"
else
    echo "警告: $DETECT_SRC 不存在，跳过。"
fi

# 处理lidar可执行文件
LIDAR_SRC="$SOURCE_DIR/lidar_service/lidar_node"
LIDAR_DEST="$NODES_BASE/lidar/lidar_node"
if [ -f "$LIDAR_SRC" ]; then
    mkdir -p "$NODES_BASE/lidar"
    mv "$LIDAR_SRC" "$LIDAR_DEST"
    chmod +x "$LIDAR_DEST"
    echo "已移动并赋权 lidar_node 到 $LIDAR_DEST"
else
    echo "警告: $LIDAR_SRC 不存在，跳过。"
fi

# 处理plc可执行文件
PLC_SRC="$SOURCE_DIR/plc_service/robot_plc_crontorl_node"
PLC_DEST="$NODES_BASE/plc/robot_plc_crontorl_node"
if [ -f "$PLC_SRC" ]; then
    mkdir -p "$NODES_BASE/plc"
    mv "$PLC_SRC" "$PLC_DEST"
    chmod +x "$PLC_DEST"
    echo "已移动并赋权 robot_plc_crontorl_node 到 $PLC_DEST"
else
    echo "警告: $PLC_SRC 不存在，跳过。"
fi

# 4. 移动env目录到 /home/zwkj01/
echo "移动env目录到 $ENV_TARGET ..."
if [ -d "$SOURCE_DIR/env" ]; then
    mv "$SOURCE_DIR/env" "$ENV_TARGET"
    echo "已移动 env 目录到 $ENV_TARGET"
else
    echo "警告: $SOURCE_DIR/env 不存在，跳过。"
fi

# 5. 移动templates目录到 /opt/zwkj/
echo "移动templates目录到 $TEMPLATES_TARGET ..."
if [ -d "$SOURCE_DIR/templates" ]; then
    mv "$SOURCE_DIR/templates" "$TEMPLATES_TARGET"
    echo "已移动 templates 目录到 $TEMPLATES_TARGET"
else
    echo "警告: $SOURCE_DIR/templates 不存在，跳过。"
fi

echo "部署完成。"
echo "提示：服务文件已安装，可以使用 systemctl 管理服务（如 systemctl start zwkjDetect）。"