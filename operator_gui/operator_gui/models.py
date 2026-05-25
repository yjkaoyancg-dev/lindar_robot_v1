from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime
from typing import Any


@dataclass
class RobotPose:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0
    roll: float = 0.0
    pitch: float = 0.0
    yaw: float = 0.0
    confidence: float = 0.0
    source: str = ""
    updated_at: datetime | None = None


@dataclass
class TopicStatus:
    name: str
    last_seen: datetime | None = None
    message_count: int = 0
    last_value: str = ""


@dataclass
class RuntimeConfig:
    detector_mode: str = "range_density"
    lidar_config: dict[str, Any] = field(default_factory=dict)
    detect_config: dict[str, Any] = field(default_factory=dict)
    range_config: dict[str, Any] = field(default_factory=dict)
    plc_config: dict[str, Any] = field(default_factory=dict)
    errors: list[str] = field(default_factory=list)


@dataclass
class LidarCandidate:
    source_ip: str = ""
    local_ip: str = ""
    port: int = 0
    sn: str = ""
    first_seen: datetime | None = None
    last_seen: datetime | None = None
    packet_count: int = 0
    note: str = ""


@dataclass
class SerialCandidate:
    path: str = ""
    real_path: str = ""
    role_hint: str = ""
    source: str = ""
    note: str = ""


@dataclass
class DeviceScanResult:
    scanned_at: datetime | None = None
    lidar_candidates: list[LidarCandidate] = field(default_factory=list)
    serial_candidates: list[SerialCandidate] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)


@dataclass
class AppState:
    dry_run: bool = True
    output_allowed: bool = False
    ros_available: bool = False
    runtime_config: RuntimeConfig = field(default_factory=RuntimeConfig)
    robot_info: RobotPose = field(default_factory=RobotPose)
    robot_info_range: RobotPose = field(default_factory=RobotPose)
    range_detected: bool | None = None
    range_confidence: float | None = None
    topics: dict[str, TopicStatus] = field(default_factory=dict)
    device_scan: DeviceScanResult = field(default_factory=DeviceScanResult)
