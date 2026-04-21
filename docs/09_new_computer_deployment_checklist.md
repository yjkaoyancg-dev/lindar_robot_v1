# 新电脑部署 Checklist（Photonix 现场版）

适用对象：到现场后，需要把当前这套 Photonix 系统快速部署到另一台 Ubuntu 22.04 + ROS 2 Iron 电脑上的人员。

---

## 0. 部署目标

在新电脑上完成以下能力：

- 雷达节点能启动并发布点云
- 检测节点能输出目标位置/置信度
- PLC 节点能作为 Modbus RTU 从站响应主站读写
- 主站可通过 485 读取 `state`、位置、姿态、置信度寄存器

---

## 1. 准备物料

出发前确认以下内容都已带齐：

- `photonix_field_release_2026-04-20.tar.gz`
- 两个 USB 转 485 模块
- 雷达设备与网线
- 现场电脑电源、鼠标、键盘
- 串口/网口转接头（如现场电脑接口不足）
- 现场调试记录本或截图工具

建议再额外带：

- 一个 U 盘备份交付包
- 一份纸质或离线 PDF 的 PLC 寄存器说明

---

## 2. 新电脑系统检查

### 2.1 确认系统版本

```bash
lsb_release -a
uname -a
```

期望：

- Ubuntu 22.04.x
- 已安装 ROS 2 Iron

### 2.2 确认 ROS 环境

```bash
echo $ROS_DISTRO
source /opt/ros/iron/setup.bash
ros2 --help | head
```

期望：

- `ROS_DISTRO=iron`
- `ros2` 命令可用

### 2.3 确认基础工具

```bash
python3 --version
g++ --version
cmake --version
colcon --version
```

---

## 3. 解压交付包

选择一个干净目录，例如：

```bash
mkdir -p ~/deploy
cd ~/deploy
tar -xzf /path/to/photonix_field_release_2026-04-20.tar.gz
cd photonix_field_release_2026-04-20
```

检查目录结构：

```bash
find . -maxdepth 2 -type d | sort
```

重点确认这些目录存在：

- `source_bundle/`
- `configs/`
- `scripts/`
- `docs/`
- `templates/`

---

## 4. 从交付包恢复工程目录

如果交付包里带的是源码包而不是现成工作区，建议统一恢复到：

```bash
~/work/Photonix
```

例如：

```bash
mkdir -p ~/work
cp -r source_bundle/Photonix ~/work/Photonix
```

如果提示目录已存在，先确认是否要覆盖旧版本：

```bash
mv ~/work/Photonix ~/work/Photonix_old_$(date +%Y%m%d_%H%M%S)
cp -r source_bundle/Photonix ~/work/Photonix
```

---

## 5. 配置文件部署

### 5.1 创建运行目录

```bash
sudo mkdir -p /opt/zwkj/configs
sudo mkdir -p /opt/zwkj/templates
```

### 5.2 拷贝配置

```bash
sudo cp configs/lidar/lidar_config.json /opt/zwkj/configs/
sudo cp configs/detect/detect_config.json /opt/zwkj/configs/
sudo cp configs/detect/range_density_detect_config.json /opt/zwkj/configs/
sudo cp configs/plc/robot_plc_crontorl.json /opt/zwkj/configs/
```

### 5.3 拷贝模板

```bash
sudo cp -r templates/* /opt/zwkj/templates/
```

### 5.4 检查

```bash
ls -l /opt/zwkj/configs
ls -l /opt/zwkj/templates | head
```

---

## 6. 编译工程

进入工程目录：

```bash
cd ~/work/Photonix
source /opt/ros/iron/setup.bash
colcon build --symlink-install
```

如果只想先编译关键包：

```bash
colcon build --symlink-install --packages-select lidar_module detect_module robot_plc_crontorl
```

编译成功后：

```bash
source ~/work/Photonix/install/setup.bash
ros2 pkg executables detect_module
ros2 pkg executables robot_plc_crontorl
```

---

## 7. 串口与 485 角色确认

插入两个 USB 转 485 后执行：

```bash
ls -l /dev/serial/by-id/
```

记录两只设备的稳定路径。

### 当前已验证推荐分配

- **PLC 从站程序使用**：`...016039-if00`
- **主站调试/假下位机使用**：`...004110-if00`

如果现场电脑枚举顺序不同，优先以 `by-id` 路径为准，不要依赖 `/dev/ttyACM0`、`/dev/ttyACM1`。

### 检查 PLC 配置

```bash
cat /opt/zwkj/configs/robot_plc_crontorl.json
```

确认 `rtu.device` 指向的是**从站**那只 485。

---

## 8. 网络与雷达检查

### 8.1 查看网卡

```bash
ip addr
```

### 8.2 启动雷达节点

```bash
cd ~/work/Photonix
source /opt/ros/iron/setup.bash
source install/setup.bash
ros2 run lidar_module lidar_node
```

### 8.3 检查点云

另开一个终端：

```bash
source /opt/ros/iron/setup.bash
source ~/work/Photonix/install/setup.bash
ros2 topic list | grep pointcloud
ros2 topic echo /pointcloud_BFZ9IEC9 --once
```

期望：

- 出现 `/pointcloud_BFZ9IEC9`
- 能收到一帧 `PointCloud2`

---

## 9. 检测模块检查

### 9.1 启动 X 方向密度检测模块

```bash
cd ~/work/Photonix
source /opt/ros/iron/setup.bash
source install/setup.bash
ros2 run detect_module range_density_detector_node --ros-args -p config_path:=/opt/zwkj/configs/range_density_detect_config.json
```

### 9.2 检查检测输出

```bash
source /opt/ros/iron/setup.bash
source ~/work/Photonix/install/setup.bash
ros2 topic echo /robot_info_range --once
ros2 topic echo /range_detector/detected --once
ros2 topic echo /range_detector/confidence --once
```

期望：

- `/robot_info_range` 返回位置
- `/range_detector/detected` 为 `true`
- `/range_detector/confidence` 为 `1.0` 或其他合理值

---

## 10. 桥接节点检查

### 10.1 启动桥接节点

推荐手动启动，并使用已验证参数：

```bash
cd ~/work/Photonix
source /opt/ros/iron/setup.bash
source install/setup.bash
ros2 run detect_module range_plc_bridge_node \
  --ros-args \
  -p timeout_ms:=8000 \
  -p freshness_ms:=8000 \
  -p use_latest_if_fresh:=false \
  -p zero_on_timeout:=false
```

### 10.2 检查接口

```bash
source /opt/ros/iron/setup.bash
source ~/work/Photonix/install/setup.bash
ros2 service list | grep start_detection
ros2 topic list | grep robot_info
```

期望：

- 有 `/start_detection`
- 有 `/robot_info`
- 有 `/robot_info_range`

---

## 11. PLC 从站检查

### 11.1 启动 PLC 节点

```bash
cd ~/work/Photonix
source /opt/ros/iron/setup.bash
source install/setup.bash
ros2 run robot_plc_crontorl robot_plc_crontorl_node
```

期望日志中出现：

- 配置加载完成
- `Modbus RTU 从站已启动`
- 串口路径正确

### 11.2 检查端口占用

```bash
lsof /dev/serial/by-id/你的从站485路径
```

期望：

- 只有一个 `robot_plc` 进程占用该设备

---

## 12. 假下位机/主站联调

### 12.1 使用主站口读取寄存器

```bash
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 -c 4 /dev/serial/by-id/你的主站485路径
```

期望：

- 能读到寄存器，不超时

### 12.2 启动一次完整流程

主站标准流程：

1. 先写 `cmd=0`
2. 再写 `cmd=1`
3. 轮询 `state`
4. 若 `state=2`，读取结果寄存器

示例：

```bash
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 /dev/serial/by-id/你的主站485路径 0
sleep 1
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 /dev/serial/by-id/你的主站485路径 1
sleep 1
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 -c 16 /dev/serial/by-id/你的主站485路径
```

### 12.3 已验证的寄存器含义

- `[1]` → `cmd`
- `[2]` → `state`
- `[3]-[8]` → `x/y/z`
- `[9]-[14]` → `roll/pitch/yaw`
- `[15]-[16]` → `confidence`

状态定义：

- `0` 未启动
- `1` 正在启动/等待结果
- `2` 启动完成

---

## 13. 32 位浮点解码规则

每个 `float` 占两个 16-bit Holding Register，当前使用 **big-endian**。

示例：

- `x` = 寄存器 `[3],[4]`
- `y` = 寄存器 `[5],[6]`
- `z` = 寄存器 `[7],[8]`
- `confidence` = 寄存器 `[15],[16]`

注意：

- 单个寄存器显示成负数是正常的
- 不能单独解释某个寄存器
- 必须两个寄存器组合后按 float 解码

---

## 14. 现场快速判断标准

### 成功标准

满足以下 5 条即可认为部署成功：

1. 雷达能发布 `/pointcloud_BFZ9IEC9`
2. 检测节点能输出 `/robot_info_range`
3. 桥接节点能提供 `/start_detection`
4. PLC 节点能作为 RTU 从站被 `mbpoll` 读取
5. 主站写 `0->1` 后，`state` 能从 `1` 变成 `2`，并读到非零位置寄存器

### 当前已验证示例结果

曾成功读到：

- `state = 2`
- `x ≈ 0.7515`
- `y ≈ -0.0539`
- `z ≈ 0.2116`
- `confidence = 1.0`

---

## 15. 常见问题排查

### 15.1 读寄存器超时

检查顺序：

1. 是否只有一个 `robot_plc_crontorl_node` 在占用从站串口
2. 主站/从站 485 角色是否分配正确
3. 是否使用了错误的 `/dev/ttyACM*`
4. 是否改回了错误的 `rtu.device`

### 15.2 `state=1` 一直不变

说明：

- PLC 已触发启动
- 但桥接节点没有在等待时间内给出有效本轮结果

检查：

- `/robot_info_range`
- `/range_detector/detected`
- `/range_detector/confidence`
- 桥接节点参数是否正确

### 15.3 `state=2` 但数据全 0

这是不期望的组合。通常意味着：

- 过早发布了全零 JSON
- 或桥接 timeout 后错误地推进了完成态

优先检查桥接参数：

- `use_latest_if_fresh:=false`
- `zero_on_timeout:=false`

### 15.4 第二次启动无效

必须遵守：

- 先把 `cmd` 写回 `0`
- 再写 `1`

即必须形成新的 `0->1` 上升沿。

---

## 16. 建议的现场操作顺序

现场最推荐的顺序：

1. 插入两个 USB485
2. 确认 `by-id` 路径
3. 部署配置到 `/opt/zwkj/configs`
4. 启动雷达
5. 确认点云
6. 启动检测节点
7. 确认检测结果话题
8. 启动桥接节点
9. 启动 PLC 节点
10. 使用 `mbpoll` 模拟主站联调
11. 最后再接真实下位机/PLC

---

## 17. 交付建议

建议同时交付两份内容：

### 1. 仓库版
用于持续开发、参数更新、代码维护。

### 2. 现场冻结版
用于现场联调，要求：

- 不依赖联网下载
- 配置固定
- 485 角色固定
- 启动脚本固定
- 文档固定

