from __future__ import annotations

from PySide6.QtWidgets import QGridLayout, QGroupBox, QLabel, QVBoxLayout, QWidget

from ..models import AppState


class OverviewPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        title = QLabel("首页总览")
        title.setObjectName("Title")
        layout.addWidget(title)

        self.dry_run = QLabel("DRY-RUN / 未允许输出")
        self.dry_run.setObjectName("BadgeDryRun")
        layout.addWidget(self.dry_run)

        group = QGroupBox("系统状态")
        grid = QGridLayout(group)
        self.mode = QLabel("-")
        self.ros = QLabel("-")
        self.radar = QLabel("-")
        self.detect = QLabel("-")
        self.plc = QLabel("-")
        self.config = QLabel("-")
        rows = [
            ("当前检测模式", self.mode),
            ("ROS2 观察状态", self.ros),
            ("雷达状态", self.radar),
            ("检测状态", self.detect),
            ("PLC 状态", self.plc),
            ("当前配置", self.config),
        ]
        for row, (name, widget) in enumerate(rows):
            grid.addWidget(QLabel(name), row, 0)
            grid.addWidget(widget, row, 1)
        layout.addWidget(group)

        result_group = QGroupBox("当前目标坐标")
        result_grid = QGridLayout(result_group)
        self.raw_pose = QLabel("-")
        self.plc_pose = QLabel("-")
        self.confidence = QLabel("-")
        result_grid.addWidget(QLabel("原始 detect 坐标"), 0, 0)
        result_grid.addWidget(self.raw_pose, 0, 1)
        result_grid.addWidget(QLabel("PLC 输出坐标"), 1, 0)
        result_grid.addWidget(self.plc_pose, 1, 1)
        result_grid.addWidget(QLabel("置信度"), 2, 0)
        result_grid.addWidget(self.confidence, 2, 1)
        layout.addWidget(result_group)
        layout.addStretch(1)

    def update_state(self, state: AppState) -> None:
        self.mode.setText(state.runtime_config.detector_mode)
        self.ros.setText("在线" if state.ros_available else "离线或未加载 rclpy")
        lidar_count = len(state.runtime_config.lidar_config.get("lidar80", [])) + len(
            state.runtime_config.lidar_config.get("lidar180", [])
        )
        self.radar.setText(f"配置组数: {lidar_count}")
        self.detect.setText("收到检测数据" if state.robot_info.updated_at or state.robot_info_range.updated_at else "等待数据")
        plc_mode = state.runtime_config.plc_config.get("mode", "unknown")
        self.plc.setText(f"{plc_mode} / 只读")
        self.config.setText("runtime")
        pose = state.robot_info if state.robot_info.updated_at else state.robot_info_range
        self.raw_pose.setText(f"x={pose.x:.4f}, y={pose.y:.4f}, z={pose.z:.4f}")
        self.plc_pose.setText(f"x={pose.x:.4f}, y={pose.y:.4f}, z={pose.z:.4f} (identity)")
        conf = state.range_confidence if state.range_confidence is not None else pose.confidence
        self.confidence.setText(f"{conf:.3f}")

