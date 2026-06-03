from __future__ import annotations

import re

from PySide6.QtWidgets import QLabel, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget

from ..models import AppState


class DeviceStatusPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        title = QLabel("设备状态")
        title.setObjectName("Title")
        layout.addWidget(title)

        self.table = QTableWidget(0, 4)
        self.table.setHorizontalHeaderLabels(["设备", "状态", "最近数据", "工人提示"])
        self.table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.table)

    def update_state(self, state: AppState) -> None:
        rows: list[tuple[str, str, str, str]] = []
        lidar_config = state.runtime_config.lidar_config

        configured_lidars = self._configured_lidars(lidar_config)
        for sn, kind, group_index in configured_lidars:
            topic = f"/pointcloud_{self._ros_safe_name(sn)}"
            cloud = state.pointclouds.get(topic)
            topic_status = state.topics.get(topic)
            last_seen = "-"
            if cloud and cloud.updated_at:
                last_seen = cloud.updated_at.strftime("%H:%M:%S")
            elif topic_status and topic_status.last_seen:
                last_seen = topic_status.last_seen.strftime("%H:%M:%S")

            if cloud or topic_status:
                point_count = cloud.point_count if cloud else 0
                rows.append(
                    (
                        f"雷达 {sn}",
                        "已连接",
                        last_seen,
                        f"雷达点云正常进入系统，话题 {topic}，当前点数 {point_count}。",
                    )
                )
            else:
                rows.append(
                    (
                        f"雷达 {sn}",
                        "未连接",
                        "-",
                        f"没有收到雷达点云。请检查雷达电源、网线、IP/网卡和配置 SN。类型 {kind}，第 {group_index} 组。",
                    )
                )

        for topic in state.topics.values():
            if topic.name.startswith("/pointcloud_") or topic.name == "ros_status":
                continue
            last_seen = topic.last_seen.strftime("%H:%M:%S") if topic.last_seen else "-"
            note = self._topic_note(topic.name, topic.last_value, topic.message_count)
            rows.append((topic.name, "在线", last_seen, note))

        ros_status = state.topics.get("ros_status")
        ros_note = ros_status.last_value if ros_status else ""
        rows.insert(
            0,
            (
                "ROS2 观察器",
                "在线" if state.ros_available else "离线",
                "-",
                "界面正在监听 ROS2 数据。" if state.ros_available else f"界面没有连上 ROS2：{ros_note}",
            ),
        )

        if not rows:
            rows.append(("系统", "等待", "-", "暂未读取到设备或 ROS 话题。"))

        self.table.setRowCount(len(rows))
        for row, values in enumerate(rows):
            for col, value in enumerate(values):
                self.table.setItem(row, col, QTableWidgetItem(value))
        self.table.resizeRowsToContents()

    def _configured_lidars(self, lidar_config: dict) -> list[tuple[str, str, int]]:
        result: list[tuple[str, str, int]] = []
        for kind in ("lidar80", "lidar180"):
            groups = lidar_config.get(kind, [])
            if not isinstance(groups, list):
                continue
            for group_index, group in enumerate(groups):
                if not isinstance(group, list):
                    continue
                for item in group:
                    if isinstance(item, dict) and item.get("sn"):
                        result.append((str(item["sn"]), kind, group_index))
        return result

    def _topic_note(self, name: str, last_value: str, count: int) -> str:
        if name == "/robot_info_range":
            return f"检测节点输出了机器人位置，收到 {count} 次。"
        if name == "/range_detector/detected":
            return f"目标检测状态：{last_value}。true 表示检测到机器人。"
        if name == "/range_detector/confidence":
            return f"识别置信度：{last_value}。数值越大越可信。"
        if name == "/robot_info":
            return "PLC 桥接结果已经生成，PLC 节点可读取该结果。"
        return f"系统话题在线，收到 {count} 次数据。"

    def _ros_safe_name(self, value: str) -> str:
        safe = re.sub(r"[^A-Za-z0-9_]", "_", value)
        if not safe or safe[0].isdigit():
            safe = f"_{safe}"
        return safe
