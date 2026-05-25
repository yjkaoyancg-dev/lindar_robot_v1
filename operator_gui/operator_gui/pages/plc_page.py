from __future__ import annotations

from PySide6.QtWidgets import QGridLayout, QGroupBox, QLabel, QPlainTextEdit, QVBoxLayout, QWidget

from ..models import AppState


class PlcPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        title = QLabel("PLC Communication (Read Only)")
        title.setObjectName("Title")
        layout.addWidget(title)

        self.gate_badge = QLabel("DRY-RUN / output gate disabled / no PLC writes from GUI")
        self.gate_badge.setObjectName("BadgeDryRun")
        layout.addWidget(self.gate_badge)

        group = QGroupBox("PLC Configuration")
        grid = QGridLayout(group)
        self.mode = QLabel("-")
        self.device = QLabel("-")
        self.slave = QLabel("-")
        self.topic = QLabel("-")
        self.output_enabled = QLabel("-")
        self.effective_state = QLabel("-")
        rows = [
            ("Mode", self.mode),
            ("Serial/address", self.device),
            ("Slave ID", self.slave),
            ("Result topic", self.topic),
            ("Output gate", self.output_enabled),
            ("Effective safety state", self.effective_state),
        ]
        for row, (name, widget) in enumerate(rows):
            grid.addWidget(QLabel(name), row, 0)
            grid.addWidget(widget, row, 1)
        layout.addWidget(group)

        self.raw_json = QPlainTextEdit()
        self.raw_json.setReadOnly(True)
        layout.addWidget(self.raw_json)

    def update_state(self, state: AppState) -> None:
        cfg = state.runtime_config.plc_config
        mode = cfg.get("mode", "-")
        enabled = bool(cfg.get("output_enabled", False))
        self.mode.setText(str(mode))
        if mode == "rtu":
            rtu = cfg.get("rtu", {})
            self.device.setText(str(rtu.get("device", "-")))
            self.slave.setText(str(rtu.get("slave_id", "-")))
        else:
            tcp = cfg.get("tcp", {})
            self.device.setText(f"{tcp.get('ip', '-')}:{tcp.get('port', '-')}")
            self.slave.setText("-")
        self.topic.setText(str(cfg.get("topic_name", "-")))
        self.output_enabled.setText("true" if enabled else "false")

        if enabled:
            self.gate_badge.setText("OUTPUT ENABLED by configuration / GUI remains read-only")
            self.gate_badge.setObjectName("BadgeWarn")
            self.effective_state.setText("enabled after PLC node restart")
        else:
            self.gate_badge.setText("DRY-RUN / output gate disabled / PLC result registers blocked")
            self.gate_badge.setObjectName("BadgeDryRun")
            self.effective_state.setText("safe: detection results are not written to output registers")
        self.gate_badge.style().unpolish(self.gate_badge)
        self.gate_badge.style().polish(self.gate_badge)
        self.raw_json.setPlainText(str(cfg))
