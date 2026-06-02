from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QThread, QTimer
from PySide6.QtWidgets import (
    QApplication,
    QFrame,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPushButton,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)

from .config_reader import ConfigReader
from .export_service import ExportService
from .log_reader import LogReader
from .models import AppState, RobotPose, TopicStatus
from .pages.config_page import ConfigPage
from .pages.device_scan_page import DeviceScanPage
from .pages.device_status_page import DeviceStatusPage
from .pages.export_page import ExportPage
from .pages.logs_page import LogsPage
from .pages.overview_page import OverviewPage
from .pages.plc_page import PlcPage
from .pages.pointcloud_page import PointCloudPage
from .pages.result_page import ResultPage
from .ros_worker import RosWorker
from .style import APP_STYLE


class MainWindow(QMainWindow):
    def __init__(self, repo_root: Path) -> None:
        super().__init__()
        self.repo_root = repo_root
        self.state = AppState()
        self.config_reader = ConfigReader(repo_root)
        self.log_reader = LogReader()
        self.export_service = ExportService()
        self.setWindowTitle("Lidar Robot Operator GUI")
        self.resize(1280, 820)
        QApplication.instance().setStyleSheet(APP_STYLE)

        self._build_ui()
        self._load_config()
        self._start_ros_worker()

        self.refresh_timer = QTimer(self)
        self.refresh_timer.timeout.connect(self._refresh_pages)
        self.refresh_timer.start(1000)

        self.log_timer = QTimer(self)
        self.log_timer.timeout.connect(self.logs_page.refresh)
        self.log_timer.start(3000)
        self.logs_page.refresh()

        self.device_scan_page.scan_requested.connect(self._start_device_scan)
        self.device_scan_page.scan_completed.connect(self._on_device_scan_completed)

    def closeEvent(self, event) -> None:
        self.worker.stop()
        self.thread.quit()
        self.thread.wait(1500)
        super().closeEvent(event)

    def _build_ui(self) -> None:
        central = QWidget()
        root = QHBoxLayout(central)
        root.setContentsMargins(0, 0, 0, 0)
        self.setCentralWidget(central)

        nav = QFrame()
        nav.setObjectName("NavFrame")
        nav.setFixedWidth(220)
        nav_layout = QVBoxLayout(nav)
        title = QLabel("Lidar Robot\nOperator")
        title.setObjectName("Title")
        nav_layout.addWidget(title)

        self.stack = QStackedWidget()
        self.overview_page = OverviewPage()
        self.device_page = DeviceStatusPage()
        self.device_scan_page = DeviceScanPage()
        self.pointcloud_page = PointCloudPage()
        self.result_page = ResultPage()
        self.config_page = ConfigPage()
        self.plc_page = PlcPage()
        self.logs_page = LogsPage(self.log_reader)
        self.export_page = ExportPage(self.export_service, self)

        pages = [
            ("Overview", self.overview_page),
            ("Device Status", self.device_page),
            ("Device Scan", self.device_scan_page),
            ("Point Cloud", self.pointcloud_page),
            ("Results", self.result_page),
            ("Configuration", self.config_page),
            ("PLC Read Only", self.plc_page),
            ("Export", self.export_page),
            ("Logs", self.logs_page),
        ]
        self.nav_buttons: list[QPushButton] = []
        for index, (name, page) in enumerate(pages):
            self.stack.addWidget(page)
            button = QPushButton(name)
            button.setCheckable(True)
            button.clicked.connect(lambda checked=False, i=index: self._select_page(i))
            nav_layout.addWidget(button)
            self.nav_buttons.append(button)
        nav_layout.addStretch(1)
        footer = QLabel("READ ONLY\nDRY-RUN")
        footer.setObjectName("BadgeDryRun")
        nav_layout.addWidget(footer)

        root.addWidget(nav)
        root.addWidget(self.stack, 1)
        self._select_page(0)

    def _select_page(self, index: int) -> None:
        self.stack.setCurrentIndex(index)
        for i, button in enumerate(self.nav_buttons):
            button.setChecked(i == index)

    def _load_config(self) -> None:
        self.state.runtime_config = self.config_reader.read_runtime_config()
        self._refresh_pages()

    def _start_ros_worker(self) -> None:
        self.thread = QThread(self)
        self.worker = RosWorker(self._pointcloud_topics())
        self.worker.moveToThread(self.thread)
        self.thread.started.connect(self.worker.run)
        self.worker.status_changed.connect(self._on_ros_status)
        self.worker.topic_changed.connect(self._on_topic_changed)
        self.worker.robot_info_changed.connect(self._on_robot_info)
        self.worker.robot_info_range_changed.connect(self._on_robot_info_range)
        self.worker.range_detected_changed.connect(self._on_range_detected)
        self.worker.range_confidence_changed.connect(self._on_range_confidence)
        self.worker.pointcloud_changed.connect(self._on_pointcloud)
        self.thread.start()

    def _on_ros_status(self, available: bool, message: str) -> None:
        self.state.ros_available = available
        self.state.topics["ros_status"] = TopicStatus(name="ros_status", last_value=message)
        self._refresh_pages()

    def _on_topic_changed(self, topic: TopicStatus) -> None:
        self.state.topics[topic.name] = topic
        self._refresh_pages()

    def _on_robot_info(self, pose: RobotPose) -> None:
        self.state.robot_info = pose
        self._refresh_pages()

    def _on_robot_info_range(self, pose: RobotPose) -> None:
        self.state.robot_info_range = pose
        self._refresh_pages()

    def _on_range_detected(self, detected: bool) -> None:
        self.state.range_detected = detected
        self._refresh_pages()

    def _on_range_confidence(self, confidence: float) -> None:
        self.state.range_confidence = confidence
        self._refresh_pages()

    def _on_pointcloud(self, frame) -> None:
        self.state.pointclouds[frame.topic] = frame
        self._refresh_pages()

    def _refresh_pages(self) -> None:
        self.overview_page.update_state(self.state)
        self.device_page.update_state(self.state)
        self.device_scan_page.render_state(self.state)
        self.pointcloud_page.update_state(self.state)
        self.result_page.update_state(self.state)
        self.config_page.update_state(self.state)
        self.plc_page.update_state(self.state)
        self.export_page.update_state(self.state)

    def _start_device_scan(self) -> None:
        self.device_scan_page.start_scan(self.state.runtime_config)

    def _on_device_scan_completed(self, result) -> None:
        self.state.device_scan = result
        self._refresh_pages()

    def _pointcloud_topics(self) -> list[str]:
        topics = {
            "/robot_pointcloud",
            "/cluster_pointcloud",
            "/range_detector/target_cloud",
        }
        for cfg in (self.state.runtime_config.detect_config, self.state.runtime_config.range_config):
            topic = cfg.get("topic_name") if isinstance(cfg, dict) else None
            if topic:
                topics.add(str(topic))
        lidar_config = self.state.runtime_config.lidar_config
        for kind in ("lidar80", "lidar180"):
            for group in lidar_config.get(kind, []) if isinstance(lidar_config.get(kind, []), list) else []:
                if not isinstance(group, list):
                    continue
                for item in group:
                    if isinstance(item, dict) and item.get("sn"):
                        topics.add(f"/pointcloud_{item['sn']}")
        return sorted(topics)


def run_app(repo_root: Path) -> int:
    app = QApplication([])
    window = MainWindow(repo_root)
    window.show()
    return app.exec()
