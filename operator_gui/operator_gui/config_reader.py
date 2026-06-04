from __future__ import annotations

import json
import os
from pathlib import Path

from .models import RuntimeConfig


class ConfigReader:
    def __init__(self, repo_root: Path) -> None:
        self.repo_root = repo_root
        self.config_dirs = [
            Path("/opt/zwkj/configs"),
            Path.home() / "deploy_v1" / "runtime" / "opt_zwkj" / "configs",
            self.repo_root / "configs" / "runtime",
        ]

    def read_runtime_config(self) -> RuntimeConfig:
        cfg = RuntimeConfig()
        cfg.detector_mode = self._read_detector_mode()
        cfg.lidar_config = self._read_json("configs/runtime/lidar_config.json", cfg.errors)
        cfg.detect_config = self._read_json("configs/runtime/detect_config.json", cfg.errors)
        cfg.range_config = self._read_json("configs/runtime/range_density_detect_config.json", cfg.errors)
        cfg.plc_config = self._read_json("configs/runtime/robot_plc_crontorl.json", cfg.errors)
        return cfg

    def _read_detector_mode(self) -> str:
        candidates = [
            Path.home() / "deploy_v1" / "scripts" / ".detector_mode",
            self.repo_root / "scripts" / ".detector_mode",
        ]
        env_mode = os.environ.get("ZWKJ_DETECTOR_MODE")
        if env_mode:
            return env_mode.strip() or "range_density"
        for mode_file in candidates:
            try:
                if mode_file.exists():
                    mode = mode_file.read_text(encoding="utf-8").strip()
                    return mode or "range_density"
            except OSError:
                pass
        return "range_density"

    def _read_json(self, relative_path: str, errors: list[str]) -> dict:
        filename = Path(relative_path).name
        path = self._resolve_config_file(filename)
        if path is None:
            errors.append(f"Missing config: {filename}")
            return {}
        try:
            with path.open("r", encoding="utf-8") as stream:
                return json.load(stream)
        except FileNotFoundError:
            errors.append(f"Missing config: {path}")
        except json.JSONDecodeError as exc:
            errors.append(f"Invalid JSON {path}: {exc}")
        except OSError as exc:
            errors.append(f"Cannot read {path}: {exc}")
        return {}

    def _resolve_config_file(self, filename: str) -> Path | None:
        for directory in self.config_dirs:
            candidate = directory / filename
            if candidate.exists():
                return candidate
        return None
