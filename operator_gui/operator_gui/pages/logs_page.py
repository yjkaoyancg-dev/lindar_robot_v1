from __future__ import annotations

from PySide6.QtWidgets import QLabel, QPlainTextEdit, QTabWidget, QVBoxLayout, QWidget

from ..log_reader import LogReader


class LogsPage(QWidget):
    def __init__(self, log_reader: LogReader) -> None:
        super().__init__()
        self.log_reader = log_reader
        layout = QVBoxLayout(self)
        title = QLabel("日志与报警（只读）")
        title.setObjectName("Title")
        layout.addWidget(title)
        self.tabs = QTabWidget()
        self.editors: dict[str, QPlainTextEdit] = {}
        for name in ["lidar", "detector", "range_detector", "range_bridge", "plc"]:
            editor = QPlainTextEdit()
            editor.setReadOnly(True)
            self.editors[name] = editor
            self.tabs.addTab(editor, name)
        layout.addWidget(self.tabs)

    def refresh(self) -> None:
        logs = self.log_reader.tail_logs()
        for name, text in logs.items():
            if name in self.editors:
                self.editors[name].setPlainText(text)

