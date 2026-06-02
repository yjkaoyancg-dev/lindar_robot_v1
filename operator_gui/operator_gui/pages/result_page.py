from __future__ import annotations

import pyqtgraph as pg
from PySide6.QtWidgets import QGridLayout, QGroupBox, QLabel, QVBoxLayout, QWidget

from ..models import AppState


class ResultPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self._history: list[tuple[float, float, float, float]] = []

        layout = QVBoxLayout(self)
        title = QLabel("Detection Results")
        title.setObjectName("Title")
        layout.addWidget(title)

        group = QGroupBox("Current Result")
        grid = QGridLayout(group)
        self.raw = QLabel("-")
        self.output = QLabel("-")
        self.detected = QLabel("-")
        self.conf = QLabel("-")
        self.updated = QLabel("-")
        rows = [
            ("Range pose", self.raw),
            ("PLC payload pose", self.output),
            ("Detected", self.detected),
            ("Confidence", self.conf),
            ("Last update", self.updated),
        ]
        for row, (name, widget) in enumerate(rows):
            grid.addWidget(QLabel(name), row, 0)
            grid.addWidget(widget, row, 1)
        layout.addWidget(group)

        self.plot = pg.PlotWidget()
        self.plot.setBackground("#020617")
        self.plot.addLegend()
        self.x_curve = self.plot.plot(pen=pg.mkPen("#38bdf8", width=2), name="x")
        self.y_curve = self.plot.plot(pen=pg.mkPen("#22c55e", width=2), name="y")
        self.z_curve = self.plot.plot(pen=pg.mkPen("#f97316", width=2), name="z")
        self.c_curve = self.plot.plot(pen=pg.mkPen("#e879f9", width=2), name="confidence")
        layout.addWidget(self.plot, 1)

    def update_state(self, state: AppState) -> None:
        pose = state.robot_info if state.robot_info.updated_at else state.robot_info_range
        confidence = state.range_confidence if state.range_confidence is not None else pose.confidence
        self.raw.setText(f"x={pose.x:.4f}, y={pose.y:.4f}, z={pose.z:.4f}")
        self.output.setText(f"x={pose.x:.4f}, y={pose.y:.4f}, z={pose.z:.4f}")
        if state.range_detected is None:
            self.detected.setText("unknown")
        else:
            self.detected.setText("true" if state.range_detected else "false")
        self.conf.setText(f"{confidence:.3f}")
        self.updated.setText(pose.updated_at.strftime("%Y-%m-%d %H:%M:%S") if pose.updated_at else "-")

        if pose.updated_at:
            point = (pose.x, pose.y, pose.z, confidence)
            if not self._history or self._history[-1] != point:
                self._history.append(point)
                self._history = self._history[-120:]
            xs = list(range(len(self._history)))
            self.x_curve.setData(xs, [p[0] for p in self._history])
            self.y_curve.setData(xs, [p[1] for p in self._history])
            self.z_curve.setData(xs, [p[2] for p in self._history])
            self.c_curve.setData(xs, [p[3] for p in self._history])
