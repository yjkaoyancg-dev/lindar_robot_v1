# Photonix GitHub 整理版（2026-04-21）

这是根据现场联调后的真实快照整理出的 **GitHub 友好版本**。

## 当前确认包含的关键内容
- 雷达模块 `lidar_module`
- 聚类/模板匹配检测 `detect_module`
- X 方向密度检测 `range_density_detector_node`
- PLC 兼容桥接 `range_plc_bridge_node`
- Modbus RTU PLC 模块 `robot_plc_crontorl`
- 运行时配置、模板、启动脚本、部署脚本和现场文档

## 检测模式
支持两种可切换检测模式：
- `cluster`：旧聚类/模板匹配方案
- `range_density`：X 方向密度检测方案（通过桥接输出 `/robot_info` 给 PLC）

切换命令：
```bash
./scripts/select_detector_mode.sh cluster
./scripts/select_detector_mode.sh range_density
```

默认模式：`range_density`

## 快速使用
### 1. 编译
```bash
source /opt/ros/iron/setup.bash
colcon build --symlink-install
source install/setup.bash
```

### 2. 安装运行时配置与模板
```bash
./deploy/install_runtime_assets.sh
```

### 3. 启动
```bash
./scripts/start_stack.sh
```

### 4. 查看日志
```bash
./scripts/watch_logs.sh
```

### 5. 停止
```bash
./scripts/stop_stack.sh
```

## PLC / Modbus RTU 当前联调结论
- 当前验证通过的从站设备：`/dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC016039-if00`
- 当前验证通过的主站测试设备：`/dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00`
- 主站流程：写 `cmd=0` → 写 `cmd=1` → 轮询 `state` → `state=2` 后读取结果

详见：
- `docs/08_modbus_manual_for_plc.md`
- `docs/09_new_computer_deployment_checklist.md`

## 目录说明
- `src/`：ROS2 源码
- `service/`：原始 service 与模板目录（已去除二进制可执行文件）
- `configs/runtime/`：当前现场验证通过的运行时配置
- `configs/examples/`：原始 service 配置样例
- `templates/runtime_current/`：当前运行时模板
- `templates/cluster_default/`：聚类/模板匹配默认模板
- `scripts/`：启动、停止、日志、模式切换与测试脚本
- `deploy/`：运行时配置安装与 systemd 服务文件
- `docs/`：现场交付与部署文档
- `archive/environment_snapshot/`：设备、串口、USB、网络等环境快照（只保留诊断用文本）

## GitHub 上传建议
建议上传本目录内容，不要上传：
- `build/ install/ log/`
- 二进制可执行文件
- 现场大日志和旧备份

本仓库已移除：
- `build/ install/ log/`
- `misc/` 大体积旧备份
- service 下的二进制执行文件
