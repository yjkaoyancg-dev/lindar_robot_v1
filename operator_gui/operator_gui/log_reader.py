from __future__ import annotations

import os
from pathlib import Path


LOG_NAMES = {
    "lidar": "lidar.log",
    "detector": "detector.log",
    "range_detector": "range_detector.log",
    "range_bridge": "range_bridge.log",
    "plc": "plc.log",
}


class LogReader:
    def __init__(self) -> None:
        home = Path(os.environ.get("HOME", str(Path.home())))
        self.log_dirs = [
            home / "deploy_v1" / "runtime" / "opt_zwkj" / "logs",
            Path("/opt/zwkj/logs"),
            home / "photonix_logs",
        ]

    def tail_logs(self, max_lines: int = 200) -> dict[str, str]:
        return {name: self._tail_named_log(filename, max_lines) for name, filename in LOG_NAMES.items()}

    def _tail_named_log(self, filename: str, max_lines: int) -> str:
        checked = []
        for log_dir in self.log_dirs:
            path = log_dir / filename
            checked.append(str(path))
            if path.exists():
                return self._tail_file(path, max_lines)
        return "Log file not found. Checked:\n" + "\n".join(checked)

    def _tail_file(self, path: Path, max_lines: int) -> str:
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except FileNotFoundError:
            return f"Log file not found: {path}"
        except OSError as exc:
            return f"Cannot read log file {path}: {exc}"
        return "\n".join(lines[-max_lines:])
