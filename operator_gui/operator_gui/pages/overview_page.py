from __future__ import annotations

from PySide6.QtWidgets import QGridLayout, QGroupBox, QLabel, QVBoxLayout, QWidget

from ..models import AppState, RobotPose


class OverviewPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)

        title = QLabel("系统总览")
        title.setObjectName("Title")
        layout.addWidget(title)

        self.safety_badge = QLabel("PLC 输出开关：正在读取配置")
        self.safety_badge.setObjectName("BadgeWarn")
        layout.addWidget(self.safety_badge)

        status_group = QGroupBox("运行状态")
        status_grid = QGridLayout(status_group)
        self.mode = QLabel("-")
        self.ros = QLabel("-")
        self.lidar = QLabel("-")
        self.pointcloud = QLabel("-")
        self.detected = QLabel("-")
        self.plc = QLabel("-")
        rows = [
            ("检测模式", self.mode),
            ("ROS 观察状态", self.ros),
            ("已配置雷达", self.lidar),
            ("点云状态", self.pointcloud),
            ("检测状态", self.detected),
            ("PLC 模式", self.plc),
        ]
        for row, (name, widget) in enumerate(rows):
            status_grid.addWidget(QLabel(name), row, 0)
            status_grid.addWidget(widget, row, 1)
        layout.addWidget(status_group)

        pose_group = QGroupBox("当前目标")
        pose_grid = QGridLayout(pose_group)
        self.pose_source = QLabel("-")
        self.pose_value = QLabel("-")
        self.attitude_value = QLabel("-")
        self.confidence = QLabel("-")
        self.updated_at = QLabel("-")
        rows = [
            ("数据来源", self.pose_source),
            ("位置 x/y/z", self.pose_value),
            ("姿态 roll/pitch/yaw", self.attitude_value),
            ("置信度", self.confidence),
            ("更新时间", self.updated_at),
        ]
        for row, (name, widget) in enumerate(rows):
            pose_grid.addWidget(QLabel(name), row, 0)
            pose_grid.addWidget(widget, row, 1)
        layout.addWidget(pose_group)

        hint_group = QGroupBox("现场操作顺序")
        hint = QLabel(
            "1. 确认雷达 SN 和点云话题。  "
            "2. 设置 transform_to_world 坐标矩阵和 ROI 检测区域。  "
            "3. 检查点云和目标识别结果。  "
            "4. 现场确认无误后再打开 PLC 输出。"
        )
        hint.setWordWrap(True)
        hint_layout = QVBoxLayout(hint_group)
        hint_layout.addWidget(hint)
        layout.addWidget(hint_group)
        layout.addStretch(1)

    def update_state(self, state: AppState) -> None:
        cfg = state.runtime_config
        plc_cfg = cfg.plc_config
        output_enabled = bool(plc_cfg.get("output_enabled", False))
        if output_enabled:
            self.safety_badge.setText("PLC 输出开关：已在配置中启用")
            self.safety_badge.setObjectName("BadgeWarn")
        else:
            self.safety_badge.setText("PLC 输出开关：关闭，当前为安全调试状态")
            self.safety_badge.setObjectName("BadgeDryRun")
        self.safety_badge.style().unpolish(self.safety_badge)
        self.safety_badge.style().polish(self.safety_badge)

        self.mode.setText(cfg.detector_mode)
        self.ros.setText("在线" if state.ros_available else "离线或未加载 rclpy")
        self.lidar.setText(self._lidar_summary(state))
        self.pointcloud.setText(self._pointcloud_summary(state))
        self.detected.setText(self._detected_summary(state))
        self.plc.setText(str(plc_cfg.get("mode", "unknown")))

        pose = self._current_pose(state)
        self.pose_source.setText(pose.source or "-")
        self.pose_value.setText(f"x={pose.x:.4f}, y={pose.y:.4f}, z={pose.z:.4f}")
        self.attitude_value.setText(
            f"roll={pose.roll:.4f}, pitch={pose.pitch:.4f}, yaw={pose.yaw:.4f}"
        )
        confidence = state.range_confidence if state.range_confidence is not None else pose.confidence
        self.confidence.setText(f"{confidence:.3f}")
        self.updated_at.setText(pose.updated_at.strftime("%Y-%m-%d %H:%M:%S") if pose.updated_at else "-")

    def _current_pose(self, state: AppState) -> RobotPose:
        if state.robot_info.updated_at:
            return state.robot_info
        return state.robot_info_range

    def _lidar_summary(self, state: AppState) -> str:
        sns: list[str] = []
        lidar_config = state.runtime_config.lidar_config
        for kind in ("lidar80", "lidar180"):
            groups = lidar_config.get(kind, [])
            if not isinstance(groups, list):
                continue
            for group in groups:
                if not isinstance(group, list):
                    continue
                for item in group:
                    if isinstance(item, dict) and item.get("sn"):
                        sns.append(str(item["sn"]))
        return ", ".join(sns) if sns else "no lidar configured"

    def _pointcloud_summary(self, state: AppState) -> str:
        clouds = [frame for name, frame in state.pointclouds.items() if name.startswith("/pointcloud_")]
        if not clouds:
            return "等待点云"
        frame = sorted(clouds, key=lambda item: item.topic)[0]
        updated = frame.updated_at.strftime("%H:%M:%S") if frame.updated_at else "-"
        return f"{frame.topic}，点数={frame.point_count}，{updated}"

    def _detected_summary(self, state: AppState) -> str:
        if state.range_detected is None:
            return "未知"
        return "已检测到" if state.range_detected else "未检测到"
