# Photonix 项目交付说明（现场联调版）

版本：2026-04-20  
适用对象：算法开发、上位机/中位机开发、下位机/PLC 开发、现场调试人员  
建议状态：**现场冻结版本（field release）**

---

## 1. 文档目的

本文档用于把当前已完成联调的 Photonix 项目整理成一份**可交付、可现场使用、可给下位机人员直接参考**的说明文件。  
文档覆盖以下内容：

- 项目组成与模块职责
- 当前推荐目录结构
- 核心配置文件
- 启动/停止/日志/主站模拟脚本
- 雷达模块、聚类检测模块、X 方向密度检测模块、PLC 模块说明
- PLC / Modbus RTU 寄存器映射、状态机、主站流程、解码方法
- 本轮联调中定位出的关键问题与最终结论
- 现场交付建议与仓库/离线包建议

---

## 2. 当前项目组成

当前工程包含以下主要包：

- `lidar_module`
  - Mid70 雷达扫描、点云发布、滤波、交替采集控制
- `detect_module`
  - 原聚类/模板检测逻辑
  - 新的 `range_density_detector_node`（X 方向密度检测）
  - 新增 `range_plc_bridge_node`（把新检测结果适配为 PLC 旧接口）
- `robot_plc_crontorl`
  - PLC/主站对接逻辑
  - Modbus RTU 从站
  - 状态机控制
- `plc_module`
  - Modbus 封装
- `common_utils`
  - 公共工具

---

## 3. 当前推荐目录结构（建议整理为仓库结构）

```text
Photonix/
├─ src/
│  ├─ lidar_module/
│  ├─ detect_module/
│  ├─ robot_plc_crontorl/
│  ├─ plc_module/
│  └─ common_utils/
│
├─ configs/
│  ├─ lidar/
│  │  └─ lidar_config.json
│  ├─ detect/
│  │  ├─ detect_config.json
│  │  └─ range_density_detect_config.json
│  └─ plc/
│     └─ robot_plc_crontorl.json
│
├─ templates/
│  ├─ cluster_templates/
│  ├─ avg_fpfh_features/
│  └─ ranges.json
│
├─ scripts/
│  ├─ start_photonix_stack.sh
│  ├─ stop_photonix_stack.sh
│  ├─ watch_photonix_logs.sh
│  ├─ trigger_plc_test.sh
│  ├─ fake_plc_master_flow.sh
│  └─ check_photonix_stack.sh
│
├─ docs/
│  ├─ 00_overview.md
│  ├─ 01_deployment.md
│  ├─ 02_lidar_module.md
│  ├─ 03_cluster_detector.md
│  ├─ 04_range_density_detector.md
│  ├─ 05_plc_module.md
│  ├─ 06_integration_flow.md
│  ├─ 07_field_debug_checklist.md
│  └─ 08_modbus_manual_for_plc.md
│
├─ README.md
├─ CHANGELOG.md
└─ VERSION.txt
```

---

## 4. 现场版本与仓库版本建议

建议同时保留两种交付形态：

### 4.1 开发仓库版
用途：
- 持续开发
- 参数迭代
- 算法升级
- 多人协作

特点：
- 完整源码
- 完整提交历史
- 可继续修改

### 4.2 现场冻结版（推荐）
用途：
- 出差现场调试
- 无网环境使用
- 提供给 PLC / 下位机人员配合联调

特点：
- 指定 commit
- 指定配置
- 指定模板
- 指定启动脚本
- **不依赖现场联网下载**

建议打包成：

```text
photonix_field_release_20260420.tar.gz
```

包含：
- `install/`
- `configs/`
- `templates/`
- `scripts/`
- `docs/`
- `VERSION.txt`

---

## 5. 模块说明

## 5.1 雷达模块 `lidar_module`

### 职责
- 扫描 Mid70 雷达
- 根据 SN 匹配配置
- 发布点云话题
- 支持交替采集（alternate scheduler）
- 支持 passthrough + voxel 滤波

### 当前输出
- 典型点云话题：
  - `/pointcloud_BFZ9IEC9`

### 关键说明
- 雷达处于保护期/交替采集 OFF 状态时，可能暂时不发有效新点云
- 若配置中的雷达 SN 与实际扫描到的不一致，则不会创建对应节点
- 当前现场调试时，需要保证 **实际 SN 与 `lidar_config.json` 中配置一致**

### 常见检查
```bash
ros2 topic list | grep pointcloud
ros2 topic echo /pointcloud_BFZ9IEC9 --once
```

---

## 5.2 原聚类检测模块 `detect_module`
（旧方案）

### 特点
- 基于模板/聚类/特征匹配
- 原工程中与 `/start_detection` 和 `/robot_info` 旧接口契合

### 已知问题
- 当前现场场景中，Mid70 拖影明显
- 聚类数量最多只有 2~3 个，不能稳定对应目标
- 模板匹配在当前场景不够稳

### 建议
- 作为保留方案，不建议作为当前主链路方案

---

## 5.3 X 方向密度检测模块 `range_density_detector_node`
（当前主方案）

### 设计思路
在给定 ROI 内，沿 X 方向做距离 bin 统计，寻找从近到远的高密度距离段，用于替代拖影场景下的聚类检测。

### 当前输出
- `/robot_info_range` (`geometry_msgs/msg/PoseStamped`)
- `/range_detector/detected` (`std_msgs/msg/Bool`)
- `/range_detector/confidence` (`std_msgs/msg/Float32`)

### 当前特点
- 更适合“拖影严重，聚类无法稳定分离”的场景
- 可以稳定得到目标位置
- 当前桥接方案已把其结果转给 PLC 逻辑

### 核心参数
配置文件：
- `/opt/zwkj/configs/range_density_detect_config.json`

重点参数：
- `roi_x_min / roi_x_max`
- `roi_y_min / roi_y_max`
- `roi_z_min / roi_z_max`
- `bin_min / bin_max`
- `bin_size`
- `min_points_per_bin`
- `min_consecutive_bins`
- `max_consecutive_bins`
- `confidence_ref_points`

### 建议
- 现场以此模块为主
- 聚类模块只作为备选

---

## 5.4 PLC 桥接模块 `range_plc_bridge_node`

### 作用
把新的 `range_density_detector_node` 输出，适配到旧 PLC 控制链路需要的接口：

- 提供 `/start_detection` 服务
- 发布 `/robot_info` (`std_msgs/msg/String`)

### 当前桥接输出格式
桥接最终应发布如下 JSON 结构：

```json
{
  "position": {
    "x": 0.75,
    "y": -0.05,
    "z": 0.21
  },
  "attitude": {
    "roll": 0.0,
    "pitch": 0.0,
    "yaw": 0.0
  },
  "confidence": 1.0
}
```

### 注意
当前桥接模块是为了**兼容旧 PLC 状态机接口**，不建议直接跳过。

---

## 5.5 PLC 模块 `robot_plc_crontorl`

### 职责
- 提供 Modbus RTU 从站
- 接收主站写入 `cmd`
- 管理 `state`
- 调用 `/start_detection`
- 订阅 `/robot_info`
- 在合法条件下把位置/姿态/置信度编码到寄存器

### 当前通讯方式
- **Modbus RTU**
- **big-endian float**

---

## 6. 当前关键配置

## 6.1 PLC 配置文件

当前配置文件：

```text
/opt/zwkj/configs/robot_plc_crontorl.json
```

当前已验证可用的端口角色：

- **PLC 从站程序**（robot_plc_crontorl）：
  - `/dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC016039-if00`
- **主站调试口**（mbpoll 假主站）：
  - `/dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00`

### 当前 RTU 参数
- 波特率：`115200`
- 校验：`N`
- 数据位：`8`
- 停止位：`1`
- 从站地址：`1`

### 当前寄存器映射
- `cmd = 0`
- `state = 1`
- `position = 2`
- `attitude = 8`

说明：
- 主站工具按 1-based 显示时：
  - `[1]` 对应 `cmd`
  - `[2]` 对应 `state`
  - `[3]-[8]` 对应 `x/y/z`
  - `[9]-[14]` 对应 `roll/pitch/yaw`
  - `[15]-[16]` 对应 `confidence`

---

## 7. PLC / 主站流程（给下位机人员）

这部分是当前已经验证通过的主流程。

## 7.1 状态定义
- `state = 0`：未启动 / 空闲
- `state = 1`：正在启动 / 等待结果
- `state = 2`：启动完成 / 结果有效

## 7.2 主站标准流程
### Step 1：主站先写 `cmd = 0`
保证从上一轮复位。

### Step 2：主站写 `cmd = 1`
形成 `0 -> 1` 上升沿，触发新一轮检测。

### Step 3：轮询 `state`
- 若 `state = 1`：
  - 说明还在等待结果
  - 此时数据区应全 0
- 若 `state = 2`：
  - 说明本轮完成
  - 此时数据区应为本轮结果

### Step 4：读取位置/姿态/置信度
当 `state = 2` 时读取。

### Step 5：下一轮前必须先把 `cmd` 写回 0
否则第二轮不会再被视为新的启动。

---

## 8. 主站示例命令（mbpoll 模拟 PLC）

## 8.1 手动触发一轮
使用主站口：

```text
/dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00
```

### 复位
```bash
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 /dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00 0
```

### 启动
```bash
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 /dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00 1
```

### 读取 16 个寄存器
```bash
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 -c 16 /dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00
```

---

## 8.2 假下位机完整流程脚本
已整理的脚本：

```text
~/fake_plc_master_flow.sh
```

作用：
- 等点云
- 写 `cmd=0`
- 写 `cmd=1`
- 轮询 `state`
- 识别完成后退出

---

## 9. 当前已经验证通过的联调结论

## 9.1 RTU 通讯层
- 两只 USB 转 485 均可被系统识别
- 当前可用角色分配：
  - `016039` 作为从站
  - `004110` 作为主站
- 当前角色分配下，`mbpoll` 可以正常读写寄存器

## 9.2 PLC 状态机流程
已经验证：
- 写 `cmd=1` 可触发等待态
- 主站可读到 `state=1`
- 在有效时序下，主站可读到 `state=2`

## 9.3 当前已验证的一组有效寄存器结果
在一轮成功测试中，主站读取到：

- `[1] = 1`
- `[2] = 2`
- `[3]~[8]` 为非零，表示 `x/y/z`
- `[9]~[14]` 为 0，表示 `roll/pitch/yaw=0`
- `[15]~[16]` 对应 `confidence=1.0`

解码结果约为：

- `x ≈ 0.7515`
- `y ≈ -0.0539`
- `z ≈ 0.2116`
- `roll = 0`
- `pitch = 0`
- `yaw = 0`
- `confidence = 1.0`

这与当前检测节点输出位置基本一致。

---

## 10. 32-bit 浮点解码说明（big-endian）

每个 float 使用两个 16-bit Holding Register。

例如：
- `x` 使用 `[3],[4]`
- `y` 使用 `[5],[6]`
- `z` 使用 `[7],[8]`

姿态：
- `roll` 用 `[9],[10]`
- `pitch` 用 `[11],[12]`
- `yaw` 用 `[13],[14]`

置信度：
- `confidence` 用 `[15],[16]`

### Python 解码示例
```python
import struct

def f32_be(reg_hi, reg_lo):
    data = struct.pack(">HH", reg_hi & 0xFFFF, reg_lo & 0xFFFF)
    return struct.unpack(">f", data)[0]
```

### 说明
单个寄存器显示为负数（例如 `48476 (-17060)`）并不代表数据错误。  
因为它只是 float 的一半，必须两个寄存器合并后再解释。

---

## 11. 启动/停止/日志脚本

## 11.1 启动整套
```bash
~/start_photonix_stack.sh
```

当前启动内容：
1. `lidar_node`
2. `range_density_detector_node`
3. `range_plc_bridge_node`
4. `robot_plc_crontorl_node`

## 11.2 停止整套
```bash
~/stop_photonix_stack.sh
```

## 11.3 查看日志
```bash
~/watch_photonix_logs.sh
```

日志目录：
```text
~/photonix_logs/
```

典型日志文件：
- `lidar.log`
- `range_detector.log`
- `range_bridge.log`
- `plc.log`

---

## 12. 当前建议的现场联调方式

## 12.1 推荐流程
1. 启动整套：
   ```bash
   ~/start_photonix_stack.sh
   ```
2. 检查点云：
   ```bash
   ros2 topic echo /pointcloud_BFZ9IEC9 --once
   ```
3. 检查新检测结果：
   ```bash
   ros2 topic echo /robot_info_range --once
   ros2 topic echo /range_detector/detected --once
   ros2 topic echo /range_detector/confidence --once
   ```
4. 启动假 PLC 主站：
   ```bash
   ~/fake_plc_master_flow.sh
   ```
5. 若要看底层寄存器：
   ```bash
   mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 -c 16 /dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00
   ```

---

## 13. 本轮联调中定位出的关键问题与结论

### 13.1 问题 1：RTU 读写 timeout
原因：
- 两只 USB 转 485 的主从软件角色分配一度反了
- 以及调试中出现过多个 `robot_plc` 实例同时占串口

结论：
- 当前固定角色如下：
  - `016039` → 从站
  - `004110` → 主站

### 13.2 问题 2：桥接节点 JSON 格式不兼容
原因：
- 旧 PLC 控制器期待：
  ```json
  {
    "position": {"x":...,"y":...,"z":...},
    "attitude": {"roll":...,"pitch":...,"yaw":...},
    "confidence": ...
  }
  ```
- 早期桥接版本发的是数组/额外字段，导致 `applyTopicPayload()` 不接受

结论：
- 已修正桥接 JSON 结构

### 13.3 问题 3：`state=2` 但数据全 0
原因：
- 早期桥接 timeout 后仍然发布了一个全 0 的合法 JSON
- PLC 控制器会把它当成一轮合法结果

结论：
- 这属于时序与桥接策略问题
- 目前已通过参数与流程调整，使主流程能得到真实位姿结果

### 13.4 问题 4：立即复用缓存导致时序竞争
原因：
- `/start_detection` 刚收到时桥接节点立即发布最新缓存
- PLC 控制器可能尚未把 `trigger_ready_` 置为 `true`

结论：
- 现场应避免过快复用旧缓存
- 建议最终桥接逻辑以“只接受本轮触发后的新结果”为目标继续完善

---

## 14. 仍建议后续优化的点

当前系统已经可以交付联调，但仍建议后续继续优化以下点：

1. `range_plc_bridge_node`
   - 不要复用上一轮缓存
   - 只接受 `trigger_time` 之后的新结果
   - timeout 时不要发布 0 值合法结果

2. `fake_plc_master_flow.sh`
   - 当前等的是“点云到达”
   - 更合理的是等“检测结果话题 ready”

3. 现场版本管理
   - 建议保留 `VERSION.txt`
   - 记录 commit、配置版本、模板版本、485 角色映射

---

## 15. 建议给下位机开发人员的使用说明（简版）

下位机侧只需要遵守以下约定：

1. 串口参数：
   - 115200
   - 8N1
   - slave id = 1

2. 启动流程：
   - 先写 `cmd=0`
   - 再写 `cmd=1`
   - 轮询 `state`

3. 状态解释：
   - `0`：空闲
   - `1`：处理中，数据区无效
   - `2`：处理完成，数据区有效

4. 只有 `state=2` 时才读取并使用位置/姿态/置信度

5. 下一轮前必须先把 `cmd` 拉回 0，再写 1

---

## 16. 当前交付建议

建议把本文件与以下内容一起交付：

- `configs/`
- `templates/`
- `scripts/`
- `install/`（若现场无需重新编译）
- `docs/08_modbus_manual_for_plc.md`
- `VERSION.txt`

建议打包为：

```text
photonix_field_release_20260420.tar.gz
```

---

## 17. 最终结论

截至当前版本，以下结论成立：

- 雷达模块可正常发布点云
- X 方向密度检测模块可稳定输出目标位置
- 新桥接模块可把新检测结果适配到 PLC 旧接口
- Modbus RTU 主从链路已打通
- 假主站脚本可完成：
  - 触发
  - 轮询状态
  - 读取最终结果
- 下位机/PLC 开发人员已经可以按当前协议进行开发与联调

当前最推荐的现场联调方案是：

- 以 `range_density_detector_node` 为主检测模块
- 以 `range_plc_bridge_node` 兼容 PLC 旧接口
- 以 `robot_plc_crontorl` 保持 RTU 状态机和寄存器映射稳定

---

## 18. 附：已验证的关键命令

### 启动整套
```bash
~/start_photonix_stack.sh
```

### 停止整套
```bash
~/stop_photonix_stack.sh
```

### 观察日志
```bash
~/watch_photonix_logs.sh
```

### 假主站流程
```bash
~/fake_plc_master_flow.sh
```

### 单次读 16 个寄存器
```bash
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 -c 16 /dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00
```

### 手动复位 + 启动
```bash
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 /dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00 0
sleep 1
mbpoll -1 -m rtu -a 1 -b 115200 -P none -t 4 -r 1 /dev/serial/by-id/usb-1a86_USB_Single_Serial_5ACC004110-if00 1
```
