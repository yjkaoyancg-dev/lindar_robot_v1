from __future__ import annotations

from PySide6.QtWidgets import QLabel, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget

from ..models import AppState


class DeviceStatusPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        title = QLabel("Device Status")
        title.setObjectName("Title")
        layout.addWidget(title)

        self.table = QTableWidget(0, 4)
        self.table.setHorizontalHeaderLabels(["Device/topic", "Status", "Last data", "Note"])
        self.table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.table)

    def update_state(self, state: AppState) -> None:
        rows: list[tuple[str, str, str, str]] = []
        lidar_config = state.runtime_config.lidar_config
        for kind in ("lidar80", "lidar180"):
            groups = lidar_config.get(kind, [])
            if not isinstance(groups, list):
                continue
            for group_index, group in enumerate(groups):
                if not isinstance(group, list):
                    continue
                for item in group:
                    if isinstance(item, dict):
                        rows.append((f"lidar {item.get('sn', '-')}", "configured", "-", f"{kind} group {group_index}"))

        for topic in state.topics.values():
            last_seen = topic.last_seen.strftime("%H:%M:%S") if topic.last_seen else "-"
            rows.append((topic.name, "online", last_seen, f"count={topic.message_count} {topic.last_value}"))

        if not rows:
            rows.append(("system", "waiting", "-", "no configured device or ROS topic yet"))

        self.table.setRowCount(len(rows))
        for row, values in enumerate(rows):
            for col, value in enumerate(values):
                self.table.setItem(row, col, QTableWidgetItem(value))
