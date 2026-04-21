## 🤖 Assistant

这是一个非常典型的ROS2工业机器人感知系统架构。根据你提供的目录结构和代码逻辑，我为你撰写了一份详细的 **README.md** 文档。

你可以将以下内容保存到项目根目录下的 `README.md` 文件中。

```markdown
# 机器人激光雷达感知与控制系统 (Robotic Lidar Perception & Control System)

## 1. 项目简介

本项目是一套基于ROS2的机器人感知与控制解决方案。系统集成了激光雷达数据采集、基于点云特征匹配的目标检测以及与PLC（可编程逻辑控制器）的工业通信。

**核心功能流程：**
1.  **数据采集**：`lidar_module` 通过UDP协议采集Livox系列激光雷达（Mid70, Tele, Avia）的原始点云数据。
2.  **目标识别**：`detect_module` 实时对点云进行处理（降采样、聚类），并与离线训练的FPFH特征模板进行匹配，识别物体并计算质心。
3.  **控制交互**：`robot_plc_crontorl` 订阅识别结果，响应PLC的Modbus指令，触发检测服务并将结果回传至PLC。

## 2. 系统架构与目录结构

```bash
.
├── service/ # 部署配置文件（systemd服务）
│ ├── detect_service/ # 检测模块服务配置
│ ├── lidar_service/ # 激光雷达服务配置
│ ├── plc_service/ # PLC控制服务配置
│ └── templates/ # 特征模板库
│ └── avg_fpfh_features # FPFH特征文件 (*.fpfh)
├── src/ # 源代码
│ ├── common_utils/ # 通用工具库（日志、配置、校验）
│ ├── lidar_module/ # 激光雷达驱动与发布节点
│ ├── detect_module/ # 点云检测与识别节点
│ ├── plc_module/ # Modbus通信底层库
│ ├── robot_plc_crontorl/ # PLC控制与机器人逻辑节点
│ └── template_creater/ # 离线工具：用于生成特征模板
└── template/ # 模板点云数据 (*.pcd) 及配置
```

## 3. 核心模块详解

### 3.1 lidar_module (激光雷达模块)

负责连接传感器并对外发布标准PointCloud2消息。

* **硬件支持**：Livox Mid70, Livox Tele, Livox Avia。
* **通信方式**：UDP Socket。
* **话题 (Topics)**：
  * **发布**：`/pointcloud_{sn}` (`sensor_msgs::msg::PointCloud2`)
    * `{sn}` 为激光雷达的序列号，用于区分多台设备。
* **服务 (Services)**：
  * **采集控制**：`/collect_{sn}` (`std_srvs::srv::Trigger`)
    * 用于远程启动/停止雷达的数据采集或录制（取决于具体配置）。
* **配置文件**：`src/lidar_module/config/lidar_config.json` (包含IP、端口、SN等)。

### 3.2 detect_module (检测模块)

负责将点云数据转化为结构化的物体信息（JSON）。

* **算法流程**：
    1. **下采样 (Downsampling)**：使用VoxelGrid对原始点云进行降采样，减少计算量。
    2. **聚类 (Clustering)**：应用基于区域生长（Region Growing）的聚类算法，分割出独立的物体点云簇。
    3. **特征匹配 (Feature Matching)**：计算每个簇的FPFH（Fast Point Feature Histograms）特征，并与 `templates/avg_fpfh_features` 目录下的预存模板进行匹配。
    4. **质心计算**：对匹配成功的点云簇计算几何质心。
* **话题 (Topics)**：
  * **订阅**：订阅 `/pointcloud_{sn}` (来自lidar_module)。
  * **发布**：`/robot_info` (`std_msgs::msg::String`)
    * 内容为JSON格式，包含识别到的物体ID、位置、姿态等。
    * **注意**：虽然计算持续进行，但只有收到PLC请求时，才将**最新一次**的计算结果发布到此话题。
* **服务 (Services)**：
  * **识别触发**：`/start_detection` (`std_srvs::srv::Trigger`)
    * 当PLC请求识别结果时，此服务被 `robot_plc_crontorl` 调用。
* **配置文件**：`src/detect_module/config/detect_config.json`。

### 3.3 robot_plc_crontorl (PLC控制模块)

连接上层检测模块与下层工业设备（PLC）的桥梁。

* **主要职责**：
    1. **接收指令**：通过Modbus TCP/RTU接收PLC的控制指令（如“开始识别”、“停止识别”）。
    2. **数据转发**：调用 `detect_module` 的 `/start_detection` 服务获取最新识别结果。
    3. **回传结果**：将识别结果（JSON解析后）写入Modbus寄存器，发送给PLC。
* **话题 (Topics)**：
  * **订阅**：订阅 `/robot_info` (来自detect_module)，用于实时更新本地缓存的机器人状态。
* **服务 (Services)**：
  * **提供**：`/start_detection` (代理转发给 detect_module)。
* **配置文件**：`src/robot_plc_crontorl/config/robot_plc_crontorl.json`。

### 3.4 辅助模块

* **plc_module**: 封装了Modbus TCP/RTU的通信协议实现，支持主机（Master）和从机（Slave）模式，供 `robot_plc_crontorl` 调用。
* **common_utils**: 提供统一的日志输出（基于spdlog）、配置文件解析（JSON/YAML）、时间处理等工具。
* **template_creater**: **离线工具**。用于将人工裁剪好的 `.pcd` 模板文件转换为 `.fpfh` 特征文件，以便检测模块进行匹配。

## 4. 配置说明

### 4.1 模板配置

位于 `template/ranges.json`。定义了检测区域的范围，只有在此范围内的点云才会参与识别计算。

### 4.2 特征模板

位于 `template/avg_fpfh_features/`。包含预先计算好的标准物体FPFH特征（`template_01.fpfh` ~ `template_09.fpfh`）。如果需要识别新物体，需使用 `template_creater` 工具生成新的 `.fpfh` 文件放入此目录。

## 5. 安装与运行

### 5.1 环境依赖

* **ROS2** (Humble 或 Foxy)
* **PCL** (Point Cloud Library)
* **Livox SDK** (如果是Livox非标准驱动)
* **Modbus** 库 (如 libmodbus)

### 5.2 编译

```bash
# 进入工作空间根目录
cd ~/ros2_ws
colcon build --packages-select common_utils plc_module lidar_module detect_module robot_plc_crontorl
source install/setup.bash
```

### 5.3 启动顺序

1. **启动雷达节点**：

    ```bash
    ros2 run lidar_module lidar_node
    ```

    *监听UDP，发布 `/pointcloud_{sn}`*

2. **启动检测节点**：

    ```bash
    ros2 run detect_module detect_module
    ```

    *订阅点云，持续计算，但默认不发布结果*

3. **启动PLC控制节点**：

    ```bash
    ros2 run robot_plc_crontorl robot_plc_crontorl_node
    ```

    *等待PLC触发*

### 5.4 部署 (Systemd)

项目提供了 `service/` 目录下的 `.service` 文件，可以使用 `install.sh` 脚本将其部署为Linux系统服务，从而实现开机自启和后台运行。

## 6. 常见问题排查

* **雷达无数据**：检查 `lidar_config.json` 中的IP和端口是否与雷达实际IP匹配；检查防火墙是否开放UDP端口。
* **识别不到物体**：
    1. 检查模板 `template_pcd` 与实际物体是否匹配。
    2. 调整 `detect_config.json` 中的聚类参数（搜索半径、最小簇点数）。
    3. 检查 `ranges.json` 区域范围是否正确。
* **PLC通信失败**：确认Modbus寄存器映射表（`register_layout.h`）与PLC编程中的寄存器地址一致。

## 7. 开发指南

如果需要添加新的识别模板：

1. 使用PCL查看器或CloudCompare手动裁剪目标物体的点云，保存为 `.pcd`。
2. 运行 `template_creater` 节点，输入该 `.pcd` 文件，生成对应的 `.fpfh` 文件。
3. 将生成的 `.fpfh` 放入 `template/avg_fpfh_features` 并在 `detect_config.json` 中注册。

```
