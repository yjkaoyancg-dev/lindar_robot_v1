# 检测模块可切换说明

## cluster 模式
链路：
- `lidar_node`
- `detect_module`
- `robot_plc_crontorl_node`

特点：
- 直接由旧检测模块提供 `/start_detection` 和 `/robot_info`
- 适用于模板匹配、聚类可分场景

## range_density 模式
链路：
- `lidar_node`
- `range_density_detector_node`
- `range_plc_bridge_node`
- `robot_plc_crontorl_node`

特点：
- 适用于拖影严重、聚类不可靠场景
- 由 `range_plc_bridge_node` 提供 `/start_detection` 和 `/robot_info`
- PLC 层无需改协议
