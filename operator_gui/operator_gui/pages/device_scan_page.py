from __future__ import annotations

from PySide6.QtCore import QThread, Signal
from PySide6.QtWidgets import (
    QHBoxLayout,
    QLabel,
    QPushButton,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from ..device_scan import DeviceScanner
from ..models import AppState, DeviceScanResult, RuntimeConfig


class ScanThread(QThread):
    completed = Signal(object)

    def __init__(self, runtime_config: RuntimeConfig) -> None:
        super().__init__()
        self.runtime_config = runtime_config

    def run(self) -> None:
        self.completed.emit(DeviceScanner().scan(self.runtime_config))


class DeviceScanPage(QWidget):
    scan_requested = Signal()
    scan_completed = Signal(object)

    def __init__(self) -> None:
        super().__init__()
        self._scan_thread: ScanThread | None = None
        layout = QVBoxLayout(self)

        header = QHBoxLayout()
        title = QLabel("设备扫描（只读）")
        title.setObjectName("Title")
        header.addWidget(title)
        header.addStretch(1)
        self.scan_button = QPushButton("刷新扫描")
        self.scan_button.clicked.connect(self.scan_requested.emit)
        header.addWidget(self.scan_button)
        layout.addLayout(header)

        safety = QLabel("PR2 只显示候选设备，不写配置、不打开串口、不发送雷达控制命令")
        safety.setObjectName("BadgeDryRun")
        layout.addWidget(safety)

        layout.addWidget(QLabel("当前配置中的雷达"))
        self.configured_lidar_table = QTableWidget(0, 4)
        self.configured_lidar_table.setHorizontalHeaderLabels(["类型", "组", "SN", "备注"])
        self.configured_lidar_table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.configured_lidar_table)

        layout.addWidget(QLabel("被动监听到的雷达候选"))
        self.lidar_table = QTableWidget(0, 5)
        self.lidar_table.setHorizontalHeaderLabels(["来源 IP", "端口", "可能 SN", "包数", "备注"])
        self.lidar_table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.lidar_table)

        layout.addWidget(QLabel("串口候选"))
        self.serial_table = QTableWidget(0, 5)
        self.serial_table.setHorizontalHeaderLabels(["设备路径", "真实路径", "候选身份", "来源", "备注"])
        self.serial_table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.serial_table)

        self.errors = QLabel("")
        self.errors.setWordWrap(True)
        layout.addWidget(self.errors)

    def start_scan(self, runtime_config: RuntimeConfig) -> None:
        if self._scan_thread and self._scan_thread.isRunning():
            return
        self.scan_button.setEnabled(False)
        self.scan_button.setText("扫描中...")
        self._scan_thread = ScanThread(runtime_config)
        self._scan_thread.completed.connect(self._on_scan_completed)
        self._scan_thread.start()

    def render_state(self, state: AppState) -> None:
        configured = DeviceScanner().configured_lidars(state.runtime_config)
        self.configured_lidar_table.setRowCount(len(configured))
        for row, item in enumerate(configured):
            values = [item.get("type", ""), item.get("group", ""), item.get("sn", ""), item.get("note", "")]
            for col, value in enumerate(values):
                self.configured_lidar_table.setItem(row, col, QTableWidgetItem(value))
        self.render_scan_result(state.device_scan)

    def render_scan_result(self, result: DeviceScanResult) -> None:
        self.lidar_table.setRowCount(len(result.lidar_candidates))
        for row, item in enumerate(result.lidar_candidates):
            values = [item.source_ip, str(item.port), item.sn or "-", str(item.packet_count), item.note]
            for col, value in enumerate(values):
                self.lidar_table.setItem(row, col, QTableWidgetItem(value))

        self.serial_table.setRowCount(len(result.serial_candidates))
        for row, item in enumerate(result.serial_candidates):
            values = [item.path, item.real_path, item.role_hint, item.source, item.note]
            for col, value in enumerate(values):
                self.serial_table.setItem(row, col, QTableWidgetItem(value))

        self.errors.setText("\n".join(result.errors))

    def _on_scan_completed(self, result: DeviceScanResult) -> None:
        self.scan_button.setEnabled(True)
        self.scan_button.setText("刷新扫描")
        self.render_scan_result(result)
        self.scan_completed.emit(result)
