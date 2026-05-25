from __future__ import annotations

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
        self.table.setHorizontalHeaderLabels(["设备", "状态", "最近数据", "备注"])
        self.table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.table)

    def update_state(self, state: AppState) -> None:
        rows: list[tuple[str, str, str, str]] = []
        for group in state.runtime_config.lidar_config.get("lidar80", []):
            for item in group:
                rows.append((f"雷达 {item.get('sn', '-')}", "配置存在", "-", "lidar80"))
        for group in state.runtime_config.lidar_config.get("lidar180", []):
            for item in group:
                rows.append((f"雷达 {item.get('sn', '-')}", "配置存在", "-", "lidar180"))
        for topic in state.topics.values():
            last_seen = topic.last_seen.strftime("%H:%M:%S") if topic.last_seen else "-"
            rows.append((topic.name, "在线", last_seen, f"count={topic.message_count}"))
        if not rows:
            rows.append(("系统", "离线", "-", "未读取到设备或 topic"))

        self.table.setRowCount(len(rows))
        for row, values in enumerate(rows):
            for col, value in enumerate(values):
                self.table.setItem(row, col, QTableWidgetItem(value))

