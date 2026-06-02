from __future__ import annotations

import json
import re
from typing import Any

from PySide6.QtWidgets import QGridLayout, QGroupBox, QLabel, QPlainTextEdit, QTabWidget, QVBoxLayout, QWidget

from ..models import AppState


class ConfigPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)

        title = QLabel("配置参数")
        title.setObjectName("Title")
        layout.addWidget(title)

        note = QLabel(
            "只读查看。修改 configs/runtime 里的 JSON 后，需要运行 deploy/install_runtime_assets.sh "
            "并重启程序。"
        )
        note.setObjectName("BadgeDryRun")
        layout.addWidget(note)

        field_group = QGroupBox("现场需要重点检查的参数")
        grid = QGridLayout(field_group)
        self.lidar_sn = QLabel("-")
        self.pointcloud_topic = QLabel("-")
        self.transform = QLabel("-")
        self.passthrough = QLabel("-")
        self.roi = QLabel("-")
        self.scan_axis = QLabel("-")
        self.bin_range = QLabel("-")
        self.thresholds = QLabel("-")
        self.plc_device = QLabel("-")
        self.output_gate = QLabel("-")
        rows = [
            ("雷达 SN", self.lidar_sn),
            ("点云话题", self.pointcloud_topic),
            ("坐标矩阵平移量", self.transform),
            ("点云粗过滤范围", self.passthrough),
            ("检测 ROI 区域", self.roi),
            ("扫描轴", self.scan_axis),
            ("分段范围", self.bin_range),
            ("密度阈值", self.thresholds),
            ("PLC 串口/地址", self.plc_device),
            ("PLC 输出开关", self.output_gate),
        ]
        for row, (name, widget) in enumerate(rows):
            grid.addWidget(QLabel(name), row, 0)
            widget.setWordWrap(True)
            grid.addWidget(widget, row, 1)
        layout.addWidget(field_group)

        self.tabs = QTabWidget()
        self.lidar_json = self._json_editor()
        self.range_json = self._json_editor()
        self.plc_json = self._json_editor()
        self.detect_json = self._json_editor()
        self.tabs.addTab(self.lidar_json, "雷达配置")
        self.tabs.addTab(self.range_json, "密度检测")
        self.tabs.addTab(self.plc_json, "PLC 配置")
        self.tabs.addTab(self.detect_json, "聚类检测")
        layout.addWidget(self.tabs, 1)

    def update_state(self, state: AppState) -> None:
        lidar = state.runtime_config.lidar_config
        range_cfg = state.runtime_config.range_config
        plc = state.runtime_config.plc_config
        detect = state.runtime_config.detect_config

        lidar_items = self._lidar_items(lidar)
        first_lidar = lidar_items[0] if lidar_items else {}
        sn = str(first_lidar.get("sn", "-"))
        filter_cfg = first_lidar.get("filter", {}) if isinstance(first_lidar, dict) else {}

        self.lidar_sn.setText(sn)
        fallback_topic = f"/pointcloud_{self._ros_safe_name(sn)}" if sn != "-" else "-"
        self.pointcloud_topic.setText(str(range_cfg.get("topic_name", fallback_topic)))
        self.transform.setText(self._translation_text(filter_cfg.get("transform_to_world")))
        self.passthrough.setText(self._passthrough_text(filter_cfg.get("passthrough")))
        self.roi.setText(
            f"x=[{range_cfg.get('roi_x_min', '-')}, {range_cfg.get('roi_x_max', '-')}], "
            f"y=[{range_cfg.get('roi_y_min', '-')}, {range_cfg.get('roi_y_max', '-')}], "
            f"z=[{range_cfg.get('roi_z_min', '-')}, {range_cfg.get('roi_z_max', '-')}]"
        )
        self.scan_axis.setText(str(range_cfg.get("scan_axis", "-")))
        self.bin_range.setText(
            f"[{range_cfg.get('bin_min', '-')}, {range_cfg.get('bin_max', '-')}], "
            f"size={range_cfg.get('bin_size', '-')}"
        )
        self.thresholds.setText(
            f"min_points_per_bin={range_cfg.get('min_points_per_bin', '-')}, "
            f"min_total_points={range_cfg.get('min_total_points', '-')}, "
            f"consecutive=[{range_cfg.get('min_consecutive_bins', '-')}, "
            f"{range_cfg.get('max_consecutive_bins', '-')}]"
        )

        mode = plc.get("mode", "-")
        if mode == "rtu":
            rtu = plc.get("rtu", {})
            self.plc_device.setText(
                f"{rtu.get('device', '-')}, baud={rtu.get('baud', '-')}, slave_id={rtu.get('slave_id', '-')}"
            )
        else:
            tcp = plc.get("tcp", {})
            self.plc_device.setText(f"{tcp.get('ip', '-')}:{tcp.get('port', '-')}")
        self.output_gate.setText("true" if bool(plc.get("output_enabled", False)) else "false")

        self.lidar_json.setPlainText(self._pretty(lidar))
        self.range_json.setPlainText(self._pretty(range_cfg))
        self.plc_json.setPlainText(self._pretty(plc))
        self.detect_json.setPlainText(self._pretty(detect))

    def _json_editor(self) -> QPlainTextEdit:
        editor = QPlainTextEdit()
        editor.setReadOnly(True)
        return editor

    def _pretty(self, data: Any) -> str:
        return json.dumps(data, indent=2, ensure_ascii=False)

    def _lidar_items(self, lidar_config: dict[str, Any]) -> list[dict[str, Any]]:
        items: list[dict[str, Any]] = []
        for kind in ("lidar80", "lidar180"):
            groups = lidar_config.get(kind, [])
            if not isinstance(groups, list):
                continue
            for group in groups:
                if isinstance(group, list):
                    items.extend(item for item in group if isinstance(item, dict))
        return items

    def _translation_text(self, matrix: Any) -> str:
        if (
            isinstance(matrix, list)
            and len(matrix) >= 3
            and all(isinstance(row, list) and len(row) >= 4 for row in matrix[:3])
        ):
            return f"tx={matrix[0][3]}, ty={matrix[1][3]}, tz={matrix[2][3]}"
        return "-"

    def _passthrough_text(self, passthrough: Any) -> str:
        if not isinstance(passthrough, list):
            return "-"
        parts: list[str] = []
        for item in passthrough:
            if isinstance(item, dict):
                parts.append(f"{item.get('field_name', '?')}={item.get('limits', '-')}")
        return "; ".join(parts) if parts else "-"

    def _ros_safe_name(self, value: str) -> str:
        safe = re.sub(r"[^A-Za-z0-9_]", "_", value)
        if not safe or safe[0].isdigit():
            safe = f"_{safe}"
        return safe
