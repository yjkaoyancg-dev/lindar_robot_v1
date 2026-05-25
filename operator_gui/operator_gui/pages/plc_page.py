from __future__ import annotations

from PySide6.QtWidgets import QGridLayout, QGroupBox, QLabel, QPlainTextEdit, QVBoxLayout, QWidget

from ..models import AppState


class PlcPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        title = QLabel("PLC 通信（只读）")
        title.setObjectName("Title")
        layout.addWidget(title)

        self.dry_run = QLabel("DRY-RUN / 未允许输出 / PR1 不连接串口")
        self.dry_run.setObjectName("BadgeDryRun")
        layout.addWidget(self.dry_run)

        group = QGroupBox("PLC 配置")
        grid = QGridLayout(group)
        self.mode = QLabel("-")
        self.device = QLabel("-")
        self.slave = QLabel("-")
        self.topic = QLabel("-")
        for row, (name, widget) in enumerate([
            ("模式", self.mode),
            ("串口/地址", self.device),
            ("从站地址", self.slave),
            ("结果 topic", self.topic),
        ]):
            grid.addWidget(QLabel(name), row, 0)
            grid.addWidget(widget, row, 1)
        layout.addWidget(group)

        self.raw_json = QPlainTextEdit()
        self.raw_json.setReadOnly(True)
        layout.addWidget(self.raw_json)

    def update_state(self, state: AppState) -> None:
        cfg = state.runtime_config.plc_config
        mode = cfg.get("mode", "-")
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
        self.raw_json.setPlainText(str(cfg))

