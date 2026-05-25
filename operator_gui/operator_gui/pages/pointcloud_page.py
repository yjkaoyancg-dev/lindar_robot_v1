from __future__ import annotations

from PySide6.QtWidgets import QGridLayout, QGroupBox, QLabel, QVBoxLayout, QWidget
import pyqtgraph as pg

from ..models import AppState, PointCloudFrame


class PointCloudPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        title = QLabel("点云监控（只读）")
        title.setObjectName("Title")
        layout.addWidget(title)

        safety = QLabel("PR4 只订阅和显示点云，不控制雷达、不保存配置、不写 PLC")
        safety.setObjectName("BadgeDryRun")
        layout.addWidget(safety)

        group = QGroupBox("当前点云状态")
        grid = QGridLayout(group)
        self.raw_status = QLabel("-")
        self.target_status = QLabel("-")
        self.centroid = QLabel("-")
        self.detected = QLabel("-")
        for row, (name, widget) in enumerate(
            [
                ("原始点云", self.raw_status),
                ("目标点云", self.target_status),
                ("当前 centroid", self.centroid),
                ("检测状态", self.detected),
            ]
        ):
            grid.addWidget(QLabel(name), row, 0)
            grid.addWidget(widget, row, 1)
        layout.addWidget(group)

        self.plot = pg.PlotWidget()
        self.plot.setBackground("#020617")
        self.plot.setLabel("bottom", "x")
        self.plot.setLabel("left", "y")
        self.plot.addLegend()
        self.raw_item = self.plot.plot([], [], pen=None, symbol="o", symbolSize=3, symbolBrush="#64748b", name="raw")
        self.target_item = self.plot.plot([], [], pen=None, symbol="o", symbolSize=6, symbolBrush="#ef4444", name="target")
        self.centroid_item = self.plot.plot([], [], pen=None, symbol="+", symbolSize=16, symbolBrush="#facc15", name="centroid")
        layout.addWidget(self.plot, 1)

    def update_state(self, state: AppState) -> None:
        raw = self._select_raw_cloud(state)
        target = self._select_target_cloud(state)
        self.raw_status.setText(self._format_cloud(raw))
        self.target_status.setText(self._format_cloud(target))

        pose = state.robot_info if state.robot_info.updated_at else state.robot_info_range
        self.centroid.setText(f"x={pose.x:.4f}, y={pose.y:.4f}, z={pose.z:.4f}")
        if state.range_detected is None:
            self.detected.setText("unknown")
        else:
            self.detected.setText("detected" if state.range_detected else "not detected")

        if raw:
            self.raw_item.setData([p[0] for p in raw.sampled_points], [p[1] for p in raw.sampled_points])
        else:
            self.raw_item.setData([], [])
        if target:
            self.target_item.setData([p[0] for p in target.sampled_points], [p[1] for p in target.sampled_points])
        else:
            self.target_item.setData([], [])

        if pose.updated_at:
            self.centroid_item.setData([pose.x], [pose.y])
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
            return "waiting"
        updated = cloud.updated_at.strftime("%H:%M:%S") if cloud.updated_at else "-"
        return f"{cloud.topic}, points={cloud.point_count}, {cloud.note}, {updated}"
