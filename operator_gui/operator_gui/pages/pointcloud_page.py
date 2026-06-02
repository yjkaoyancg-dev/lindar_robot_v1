from __future__ import annotations

import pyqtgraph as pg
from PySide6.QtWidgets import QComboBox, QGridLayout, QGroupBox, QLabel, QVBoxLayout, QWidget

from ..models import AppState, PointCloudFrame


class PointCloudPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        title = QLabel("点云监控")
        title.setObjectName("Title")
        layout.addWidget(title)

        safety = QLabel("只读显示：订阅 ROS2 点云话题，不控制设备。")
        safety.setObjectName("BadgeDryRun")
        layout.addWidget(safety)

        group = QGroupBox("点云状态")
        grid = QGridLayout(group)
        self.raw_status = QLabel("-")
        self.target_status = QLabel("-")
        self.centroid = QLabel("-")
        self.detected = QLabel("-")
        self.projection = QComboBox()
        self.projection.addItems(["XY 投影", "XZ 投影", "YZ 投影"])
        rows = [
            ("原始点云", self.raw_status),
            ("目标点云", self.target_status),
            ("当前目标", self.centroid),
            ("检测状态", self.detected),
            ("显示视图", self.projection),
        ]
        for row, (name, widget) in enumerate(rows):
            grid.addWidget(QLabel(name), row, 0)
            grid.addWidget(widget, row, 1)
        layout.addWidget(group)

        self.plot = pg.PlotWidget()
        self.plot.setBackground("#020617")
        self.plot.addLegend()
        self.raw_item = self.plot.plot([], [], pen=None, symbol="o", symbolSize=3, symbolBrush="#64748b", name="raw")
        self.target_item = self.plot.plot([], [], pen=None, symbol="o", symbolSize=6, symbolBrush="#ef4444", name="target")
        self.centroid_item = self.plot.plot([], [], pen=None, symbol="+", symbolSize=16, symbolBrush="#facc15", name="target")
        layout.addWidget(self.plot, 1)

    def update_state(self, state: AppState) -> None:
        raw = self._select_raw_cloud(state)
        target = self._select_target_cloud(state)
        self.raw_status.setText(self._format_cloud(raw))
        self.target_status.setText(self._format_cloud(target))

        pose = state.robot_info if state.robot_info.updated_at else state.robot_info_range
        self.centroid.setText(f"x={pose.x:.4f}, y={pose.y:.4f}, z={pose.z:.4f}")
        if state.range_detected is None:
            self.detected.setText("未知")
        else:
            self.detected.setText("已检测到" if state.range_detected else "未检测到")

        self._set_axis_labels()
        if raw:
            self.raw_item.setData(*self._project(raw.sampled_points))
        else:
            self.raw_item.setData([], [])
        if target:
            self.target_item.setData(*self._project(target.sampled_points))
        else:
            self.target_item.setData([], [])

        if pose.updated_at:
            self.centroid_item.setData(*self._project([(pose.x, pose.y, pose.z)]))
        else:
            self.centroid_item.setData([], [])

    def _select_raw_cloud(self, state: AppState) -> PointCloudFrame | None:
        raw_topics = [name for name in state.pointclouds if name.startswith("/pointcloud_")]
        if not raw_topics:
            return None
        return state.pointclouds[sorted(raw_topics)[0]]

    def _select_target_cloud(self, state: AppState) -> PointCloudFrame | None:
        for topic in ("/range_detector/target_cloud", "/robot_pointcloud", "/cluster_pointcloud"):
            if topic in state.pointclouds:
                return state.pointclouds[topic]
        return None

    def _format_cloud(self, cloud: PointCloudFrame | None) -> str:
        if cloud is None:
            return "等待点云"
        updated = cloud.updated_at.strftime("%H:%M:%S") if cloud.updated_at else "-"
        return f"{cloud.topic}，点数={cloud.point_count}，{cloud.note}，{updated}"

    def _project(self, points: list[tuple[float, float, float]]) -> tuple[list[float], list[float]]:
        mode = self.projection.currentText()
        if mode.startswith("XZ"):
            return [p[0] for p in points], [p[2] for p in points]
        if mode.startswith("YZ"):
            return [p[1] for p in points], [p[2] for p in points]
        return [p[0] for p in points], [p[1] for p in points]

    def _set_axis_labels(self) -> None:
        mode = self.projection.currentText()
        if mode.startswith("XZ"):
            self.plot.setLabel("bottom", "x")
            self.plot.setLabel("left", "z")
        elif mode.startswith("YZ"):
            self.plot.setLabel("bottom", "y")
            self.plot.setLabel("left", "z")
        else:
            self.plot.setLabel("bottom", "x")
            self.plot.setLabel("left", "y")
