from __future__ import annotations

import json
from datetime import datetime

from PySide6.QtCore import QObject, QThread, Signal

from .models import RobotPose, TopicStatus
from .pointcloud_decoder import decode_pointcloud2


class RosWorker(QObject):
    status_changed = Signal(bool, str)
    topic_changed = Signal(object)
    robot_info_changed = Signal(object)
    robot_info_range_changed = Signal(object)
    range_detected_changed = Signal(object)
    range_confidence_changed = Signal(object)
    pointcloud_changed = Signal(object)

    def __init__(self, pointcloud_topics: list[str] | None = None) -> None:
        super().__init__()
        self.pointcloud_topics = pointcloud_topics or []
        self._running = False
        self._rclpy = None
        self._node = None
        self._topic_counts: dict[str, int] = {}

    def run(self) -> None:
        self._running = True
        try:
            import rclpy
            from geometry_msgs.msg import PoseStamped
            from sensor_msgs.msg import PointCloud2
            from std_msgs.msg import Bool, Float32, String

            self._rclpy = rclpy
            if not rclpy.ok():
                rclpy.init(args=None)
            self._node = rclpy.create_node("operator_gui_observer")
            self._node.create_subscription(String, "/robot_info", self._on_robot_info, 10)
            self._node.create_subscription(PoseStamped, "/robot_info_range", self._on_robot_info_range, 10)
            self._node.create_subscription(Bool, "/range_detector/detected", self._on_range_detected, 10)
            self._node.create_subscription(Float32, "/range_detector/confidence", self._on_range_confidence, 10)
            for topic in self.pointcloud_topics:
                self._node.create_subscription(
                    PointCloud2,
                    topic,
                    lambda msg, topic_name=topic: self._on_pointcloud(topic_name, msg),
                    10,
                )
            self.status_changed.emit(True, "ROS2 observer online")

            while self._running:
                rclpy.spin_once(self._node, timeout_sec=0.1)
        except Exception as exc:
            self.status_changed.emit(False, f"ROS2 unavailable: {exc}")
            while self._running:
                QThread.msleep(500)
        finally:
            self._shutdown_ros()

    def stop(self) -> None:
        self._running = False

    def _shutdown_ros(self) -> None:
        try:
            if self._node is not None:
                self._node.destroy_node()
            if self._rclpy is not None and self._rclpy.ok():
                self._rclpy.shutdown()
        except Exception:
            pass

    def _touch_topic(self, topic: str, value: str = "") -> None:
        count = self._topic_counts.get(topic, 0) + 1
        self._topic_counts[topic] = count
        self.topic_changed.emit(
            TopicStatus(name=topic, last_seen=datetime.now(), message_count=count, last_value=value)
        )

    def _on_robot_info(self, msg) -> None:
        self._touch_topic("/robot_info", msg.data[:160])
        pose = self._parse_robot_info_json(msg.data, "/robot_info")
        self.robot_info_changed.emit(pose)

    def _on_robot_info_range(self, msg) -> None:
        self._touch_topic("/robot_info_range", "PoseStamped")
        pose = RobotPose(
            x=float(msg.pose.position.x),
            y=float(msg.pose.position.y),
            z=float(msg.pose.position.z),
            confidence=0.0,
            source="/robot_info_range",
            updated_at=datetime.now(),
        )
        self.robot_info_range_changed.emit(pose)

    def _on_range_detected(self, msg) -> None:
        self._touch_topic("/range_detector/detected", str(bool(msg.data)))
        self.range_detected_changed.emit(bool(msg.data))

    def _on_range_confidence(self, msg) -> None:
        value = float(msg.data)
        self._touch_topic("/range_detector/confidence", f"{value:.3f}")
        self.range_confidence_changed.emit(value)

    def _on_pointcloud(self, topic: str, msg) -> None:
        frame = decode_pointcloud2(msg, topic)
        self._touch_topic(topic, frame.note)
        self.pointcloud_changed.emit(frame)

    def _parse_robot_info_json(self, text: str, source: str) -> RobotPose:
        try:
            payload = json.loads(text)
            position = payload.get("position", {})
            attitude = payload.get("attitude", {})
            return RobotPose(
                x=float(position.get("x", 0.0)),
                y=float(position.get("y", 0.0)),
                z=float(position.get("z", 0.0)),
                roll=float(attitude.get("roll", 0.0)),
                pitch=float(attitude.get("pitch", 0.0)),
                yaw=float(attitude.get("yaw", 0.0)),
                confidence=float(payload.get("confidence", 0.0)),
                source=source,
                updated_at=datetime.now(),
            )
        except Exception:
            return RobotPose(source=source, updated_at=datetime.now())
