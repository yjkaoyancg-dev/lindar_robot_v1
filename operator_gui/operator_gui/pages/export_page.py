from __future__ import annotations

from pathlib import Path

from PySide6.QtWidgets import QGridLayout, QGroupBox, QLabel, QPushButton, QPlainTextEdit, QVBoxLayout, QWidget

from ..export_service import ExportService
from ..models import AppState


class ExportPage(QWidget):
    def __init__(self, export_service: ExportService, screenshot_target: QWidget) -> None:
        super().__init__()
        self.export_service = export_service
        self.screenshot_target = screenshot_target
        self.state = AppState()

        layout = QVBoxLayout(self)
        title = QLabel("数据导出")
        title.setObjectName("Title")
        layout.addWidget(title)

        safety = QLabel("只读导出：保存快照，不写 PLC 寄存器。")
        safety.setObjectName("BadgeDryRun")
        layout.addWidget(safety)

        group = QGroupBox("导出操作")
        grid = QGridLayout(group)
        self.result_button = QPushButton("导出识别结果 JSON")
        self.config_button = QPushButton("导出配置摘要")
        self.logs_button = QPushButton("导出日志快照")
        self.screenshot_button = QPushButton("保存界面截图")
        self.result_button.clicked.connect(self._export_detection)
        self.config_button.clicked.connect(self._export_config)
        self.logs_button.clicked.connect(self._export_logs)
        self.screenshot_button.clicked.connect(self._export_screenshot)
        grid.addWidget(self.result_button, 0, 0)
        grid.addWidget(self.config_button, 0, 1)
        grid.addWidget(self.logs_button, 1, 0)
        grid.addWidget(self.screenshot_button, 1, 1)
        layout.addWidget(group)

        self.export_dir_label = QLabel(str(self.export_service.export_root))
        layout.addWidget(QLabel("导出目录"))
        layout.addWidget(self.export_dir_label)

        self.output = QPlainTextEdit()
        self.output.setReadOnly(True)
        layout.addWidget(self.output, 1)

    def update_state(self, state: AppState) -> None:
        self.state = state

    def _export_detection(self) -> None:
        self._run_export(lambda: self.export_service.export_detection_result(self.state))

    def _export_config(self) -> None:
        self._run_export(lambda: self.export_service.export_config_summary(self.state))

    def _export_logs(self) -> None:
        self._run_export(self.export_service.export_log_snapshot)

    def _export_screenshot(self) -> None:
        self._run_export(lambda: self.export_service.export_screenshot(self.screenshot_target))

    def _run_export(self, fn) -> None:
        try:
            path: Path = fn()
            self.output.appendPlainText(f"OK: {path}")
        except Exception as exc:
            self.output.appendPlainText(f"ERROR: {exc}")
