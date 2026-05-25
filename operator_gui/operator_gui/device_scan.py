from __future__ import annotations

import glob
import os
import re
import socket
import time
from datetime import datetime

from .models import DeviceScanResult, LidarCandidate, RuntimeConfig, SerialCandidate


class DeviceScanner:
    """Read-only discovery. PR2 never writes configs or opens serial ports."""

    def scan(self, runtime_config: RuntimeConfig, lidar_seconds: float = 2.0, lidar_port: int = 55000) -> DeviceScanResult:
        result = DeviceScanResult(scanned_at=datetime.now())
        result.serial_candidates = self.scan_serial(runtime_config, result.errors)
        result.lidar_candidates = self.scan_lidar_passive(lidar_seconds, lidar_port, result.errors)
        return result

    def configured_lidars(self, runtime_config: RuntimeConfig) -> list[dict[str, str]]:
        lidars: list[dict[str, str]] = []
        for kind in ("lidar80", "lidar180"):
            groups = runtime_config.lidar_config.get(kind, [])
            if not isinstance(groups, list):
                continue
            for group_index, group in enumerate(groups):
                if not isinstance(group, list):
                    continue
                for item in group:
                    if isinstance(item, dict):
                        lidars.append(
                            {
                                "type": kind,
                                "group": str(group_index),
                                "sn": str(item.get("sn", "")),
                                "note": "config only, read-only",
                            }
                        )
        return lidars

    def scan_serial(self, runtime_config: RuntimeConfig, errors: list[str]) -> list[SerialCandidate]:
        candidates: list[SerialCandidate] = []
        seen: set[str] = set()
        rtu_cfg = runtime_config.plc_config.get("rtu", {})
        configured_device = rtu_cfg.get("device", "") if isinstance(rtu_cfg, dict) else ""

        paths: list[tuple[str, str]] = []
        paths.extend((path, "by-id") for path in sorted(glob.glob("/dev/serial/by-id/*")))
        paths.extend((path, "ttyUSB") for path in sorted(glob.glob("/dev/ttyUSB*")))
        paths.extend((path, "ttyACM") for path in sorted(glob.glob("/dev/ttyACM*")))

        for path, source in paths:
            if path in seen:
                continue
            seen.add(path)
            real_path = self._realpath(path, errors)
            role_hint = "master/slave candidate"
            note = "read-only; serial port not opened"
            if configured_device and (path == configured_device or real_path == self._realpath(configured_device, errors)):
                role_hint = "current PLC slave config"
                note = "from robot_plc_crontorl.json; read-only"
            elif source == "by-id":
                role_hint = "preferred candidate"

            candidates.append(
                SerialCandidate(path=path, real_path=real_path, role_hint=role_hint, source=source, note=note)
            )

        configured_real = self._realpath(configured_device, errors) if configured_device else ""
        if configured_device and not any(c.path == configured_device or c.real_path == configured_real for c in candidates):
            candidates.insert(
                0,
                SerialCandidate(
                    path=configured_device,
                    real_path=configured_real,
                    role_hint="current PLC slave config",
                    source="config",
                    note="configured path was not present in scanned /dev results",
                ),
            )
        return candidates

    def scan_lidar_passive(self, seconds: float, port: int, errors: list[str]) -> list[LidarCandidate]:
        found: dict[tuple[str, int], LidarCandidate] = {}
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.settimeout(0.2)
            sock.bind(("", port))
            deadline = time.monotonic() + max(0.1, seconds)
            while time.monotonic() < deadline:
                try:
                    data, addr = sock.recvfrom(2048)
                except socket.timeout:
                    continue
                except OSError as exc:
                    errors.append(f"lidar UDP passive scan failed: {exc}")
                    break

                source_ip, source_port = addr[0], int(addr[1])
                key = (source_ip, source_port)
                now = datetime.now()
                candidate = found.get(key)
                if candidate is None:
                    candidate = LidarCandidate(
                        source_ip=source_ip,
                        port=source_port,
                        first_seen=now,
                        sn=self._extract_possible_sn(data),
                        note="passive UDP listen only; no command sent",
                    )
                    found[key] = candidate
                candidate.last_seen = now
                candidate.packet_count += 1
        except PermissionError as exc:
            errors.append(f"no permission to listen on UDP {port}: {exc}")
        except OSError as exc:
            errors.append(f"cannot listen on UDP {port}: {exc}")
        finally:
            sock.close()
        return list(found.values())

    def _realpath(self, path: str, errors: list[str]) -> str:
        if not path:
            return ""
        try:
            return os.path.realpath(path)
        except OSError as exc:
            errors.append(f"cannot resolve path {path}: {exc}")
            return ""

    def _extract_possible_sn(self, data: bytes) -> str:
        text = data.decode("ascii", errors="ignore")
        matches = re.findall(r"[A-Z0-9]{6,20}", text)
        return matches[0] if matches else ""
