from __future__ import annotations

from PySide6.QtWidgets import QGridLayout, QGroupBox, QLabel, QPlainTextEdit, QVBoxLayout, QWidget

from ..models import AppState


class PlcPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        title = QLabel("PLC 通讯（只读）")
        title.setObjectName("Title")
        layout.addWidget(title)

        self.gate_badge = QLabel("安全调试 / 输出开关关闭 / GUI 不写 PLC")
        self.gate_badge.setObjectName("BadgeDryRun")
        layout.addWidget(self.gate_badge)

        group = QGroupBox("PLC 配置")
        grid = QGridLayout(group)
        self.mode = QLabel("-")
        self.device = QLabel("-")
        self.slave = QLabel("-")
        self.topic = QLabel("-")
        self.output_enabled = QLabel("-")
        self.effective_state = QLabel("-")
        rows = [
            ("通讯模式", self.mode),
            ("串口/地址", self.device),
            ("从站 ID", self.slave),
            ("结果话题", self.topic),
            ("输出开关", self.output_enabled),
            ("当前安全状态", self.effective_state),
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
            self.gate_badge.setText("配置中已启用 PLC 输出 / GUI 仍保持只读")
            self.gate_badge.setObjectName("BadgeWarn")
            self.effective_state.setText("PLC 节点重启后生效")
        else:
            self.gate_badge.setText("安全调试 / 输出开关关闭 / PLC 结果寄存器不写入")
            self.gate_badge.setObjectName("BadgeDryRun")
            self.effective_state.setText("安全：检测结果不会写入输出寄存器")
        self.gate_badge.style().unpolish(self.gate_badge)
        self.gate_badge.style().polish(self.gate_badge)
        self.raw_json.setPlainText(str(cfg))
