APP_STYLE = """
QMainWindow {
    background: #111827;
    color: #e5e7eb;
}
QWidget {
    background: #111827;
    color: #e5e7eb;
    font-size: 14px;
}
QFrame#NavFrame {
    background: #0b1220;
    border-right: 1px solid #263244;
}
QPushButton {
    background: #1f2937;
    border: 1px solid #374151;
    border-radius: 6px;
    padding: 9px 12px;
    text-align: left;
}
QPushButton:hover {
    background: #2b3647;
}
QPushButton:checked {
    background: #0f766e;
    border-color: #14b8a6;
}
QLabel#Title {
    font-size: 22px;
    font-weight: 700;
}
QLabel#SectionTitle {
    font-size: 17px;
    font-weight: 700;
}
QLabel#BadgeDryRun {
    background: #7c2d12;
    color: #ffedd5;
    border-radius: 5px;
    padding: 5px 8px;
    font-weight: 700;
}
QLabel#BadgeOk {
    background: #14532d;
    color: #dcfce7;
    border-radius: 5px;
    padding: 5px 8px;
    font-weight: 700;
}
QLabel#BadgeWarn {
    background: #713f12;
    color: #fef3c7;
    border-radius: 5px;
    padding: 5px 8px;
    font-weight: 700;
}
QGroupBox {
    border: 1px solid #374151;
    border-radius: 6px;
    margin-top: 12px;
    padding: 12px;
    font-weight: 700;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 4px;
}
QTableWidget {
    background: #0f172a;
    alternate-background-color: #111827;
    gridline-color: #334155;
    border: 1px solid #334155;
}
QHeaderView::section {
    background: #1f2937;
    color: #e5e7eb;
    border: 1px solid #334155;
    padding: 6px;
}
QPlainTextEdit {
    background: #020617;
    color: #d1d5db;
    border: 1px solid #334155;
    font-family: Consolas, monospace;
}
"""

