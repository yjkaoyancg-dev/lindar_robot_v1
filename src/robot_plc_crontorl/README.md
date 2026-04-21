# robot_plc_crontorl

ROS2 Modbus TCP 从站模块。

当前支持：

- Modbus TCP 从站
- Modbus RTU 从站

职责：

- 接收 PLC 写入的启动命令寄存器
- 在命令上升沿后清零位置/姿态寄存器并进入等待状态
- 在主站写复位命令 `0` 且当前状态为 `2` 时，回到未启动并清空全部数据寄存器
- 调用检测模块的 `std_srvs/srv/Trigger` 服务启动计算
- 订阅 `std_msgs/msg/String` JSON 结果话题
- 仅在收到本轮新结果后写入位置/姿态寄存器，并切换到启动完成

状态寄存器：

- `0` 未启动
- `1` 正在启动
- `2` 启动完成

固定寄存器布局：

- `0`: `cmd_reg`
- `1`: `state_reg`
- `2-7`: `position.x/y/z`
- `8-13`: `attitude.roll/pitch/yaw`
- `14-15`: `confidence`

结果 JSON：

```json
{
  "position": { "x": 1.0, "y": 2.0, "z": 3.0 },
  "attitude": { "roll": 4.0, "pitch": 5.0, "yaw": 6.0 },
  "confidence": 0.85
}
```

配置文件默认路径：

- `/opt/zwkj/configs/robot_plc_crontorl.json`

当前支持配置项：

- `mode`
- `tcp.ip`
- `tcp.port`
- `rtu.device`
- `rtu.baud`
- `rtu.parity`
- `rtu.data_bit`
- `rtu.stop_bit`
- `rtu.slave_id`
- `byte_order`
- `trigger_service_name`
- `topic_name`

示例配置：

```json
{
  "mode": "tcp",
  "tcp": {
    "ip": "0.0.0.0",
    "port": 1502
  },
  "rtu": {
    "device": "/dev/ttyUSB0",
    "baud": 115200,
    "parity": "N",
    "data_bit": 8,
    "stop_bit": 1,
    "slave_id": 1
  },
  "byte_order": "big",
  "trigger_service_name": "robot_pose_start",
  "topic_name": "robot_pose_json"
}
```

`modpoll` 测试方案：

1. TCP 基础读写
   - 启动节点，配置 `mode=tcp`
   - 读状态寄存器：
   - `modpoll -m tcp -p 1502 -1 -t 4 -r 2 -c 1 <ip>`
   - 预期初始值为 `0`
   - 读位置姿态区：
   - `modpoll -m tcp -p 1502 -1 -t 4 -r 3 -c 12 <ip>`
   - 预期全部为 `0`

2. TCP 启动握手
   - 写启动命令寄存器：
   - `modpoll -m tcp -p 1502 -1 -t 4 -r 1 <ip> 1`
   - 再读状态寄存器，应为 `1`
   - 再读数据区，应仍全部为 `0`
   - 这一步同时观察节点日志，应看到触发检测模块 `Trigger`

3. TCP 结果回填
   - 让检测模块服务返回 `success=true`
   - 发布一帧结果 JSON 到 `topic_name`
   - 再读状态寄存器，应为 `2`
   - 再读 12 个数据寄存器，应看到本轮位置和姿态编码值

4. TCP 主站复位验证
   - 在 `state=2` 后由主站写 `cmd_reg=0`
   - 立即读状态寄存器，应回到 `0`
   - 立即读数据区，应重新全为 `0`
   - 下一轮测试时，再写一次 `cmd_reg=1` 形成新的上升沿

5. RTU 测试
   - 切换配置为 `mode=rtu`
   - 准备串口或 USB-RS485，保证 `rtu.*` 参数与主站一致
   - 读状态寄存器：
   - `modpoll -m rtu -a <slave_id> -b <baud> -p none -d 8 -s 1 -1 -t 4 -r 2 -c 1 <device>`
   - 启动写命令：
   - `modpoll -m rtu -a <slave_id> -b <baud> -p none -d 8 -s 1 -1 -t 4 -r 1 <device> 1`
   - 后续状态和数据验证步骤与 TCP 相同

建议检查点：

- `state=0` 时数据区全零
- `state=1` 时数据区全零
- 检测模块 `Trigger` 未成功前，结果 JSON 不得推进到 `state=2`
- `state=2` 时只允许出现本轮新数据
- 主站执行 `2 -> 0` 复位时，旧数据必须立刻清空
