from __future__ import annotations

import json
from dataclasses import asdict, is_dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

from PySide6.QtWidgets import QWidget

from .log_reader import LogReader
from .models import AppState


class ExportService:
    def __init__(self, export_root: Path | None = None) -> None:
        self.export_root = export_root or (Path.home() / "deploy_v1" / "operator_gui_exports")
        self.export_root.mkdir(parents=True, exist_ok=True)
        self.log_reader = LogReader()

    def export_detection_result(self, state: AppState) -> Path:
        payload = {
            "exported_at": self._now_text(),
            "dry_run": state.dry_run,
            "output_allowed": state.output_allowed,
            "range_detected": state.range_detected,
            "range_confidence": state.range_confidence,
            "robot_info": self._dataclass_to_dict(state.robot_info),
            "robot_info_range": self._dataclass_to_dict(state.robot_info_range),
            "pointcloud_topics": {
                topic: {
                    "frame_id": frame.frame_id,
                    "point_count": frame.point_count,
                    "sampled_count": len(frame.sampled_points),
                    "updated_at": self._datetime_to_text(frame.updated_at),
                    "note": frame.note,
                }
                for topic, frame in state.pointclouds.items()
            },
            "note": "PR5 read-only export; no PLC write, no serial access, no device control.",
        }
        return self._write_json("detection_result", payload)

    def export_config_summary(self, state: AppState) -> Path:
        cfg = state.runtime_config
        payload = {
            "exported_at": self._now_text(),
            "detector_mode": cfg.detector_mode,
            "lidar_config": cfg.lidar_config,
            "detect_config": cfg.detect_config,
            "range_config": cfg.range_config,
            "plc_config": cfg.plc_config,
            "config_errors": cfg.errors,
            "note": "Configuration summary is copied read-only from runtime files.",
        }
        return self._write_json("config_summary", payload)

    def export_log_snapshot(self) -> Path:
        payload = {
            "exported_at": self._now_text(),
            "logs": self.log_reader.tail_logs(max_lines=500),
            "note": "Log snapshot is read-only.",
        }
        return self._write_json("log_snapshot", payload)

    def export_screenshot(self, widget: QWidget) -> Path:
        filename = self._filename("screenshot", "png")
        path = self.export_root / filename
        pixmap = widget.grab()
        if not pixmap.save(str(path), "PNG"):
            raise RuntimeError(f"failed to save screenshot: {path}")
        return path

    def _write_json(self, prefix: str, payload: dict[str, Any]) -> Path:
        path = self.export_root / self._filename(prefix, "json")
        path.write_text(json.dumps(payload, ensure_ascii=False, indent=2, default=str), encoding="utf-8")
        return path

    def _filename(self, prefix: str, suffix: str) -> str:
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        return f"{prefix}_{stamp}.{suffix}"

    def _now_text(self) -> str:
        return datetime.now().isoformat(timespec="seconds")

    def _datetime_to_text(self, value: datetime | None) -> str | None:
        return value.isoformat(timespec="seconds") if value else None

    def _dataclass_to_dict(self, value: Any) -> Any:
        if is_dataclass(value):
            data = asdict(value)
            for key, item in list(data.items()):
                if isinstance(item, datetime):
                    data[key] = self._datetime_to_text(item)
            return data
        return value
