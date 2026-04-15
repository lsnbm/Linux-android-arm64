#!/usr/bin/env python3
import json
from pathlib import Path
import re
import struct
import sys
import threading
from datetime import datetime

CURRENT_DIR = Path(__file__).resolve().parent
if str(CURRENT_DIR) not in sys.path:
    sys.path.insert(0, str(CURRENT_DIR))

from tcp_server import AndroidBridgeSession, BridgeConnectionError, BridgeError, discover_lan_devices

from PySide6.QtCore import QPoint, Qt, QTimer, Signal
from PySide6.QtGui import QMouseEvent, QTextCursor, QWheelEvent
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QDialog,
    QFrame,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QSplitter,
    QTabWidget,
    QTextEdit,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
    QMenu,
)

DEFAULT_PORT = 9494
NETWORK_TIMEOUT_SECONDS = 6
MAX_RESPONSE_BYTES = 8 * 1024 * 1024
BROWSER_CACHE_RADIUS_BYTES = 4096
BROWSER_WINDOW_BYTES = BROWSER_CACHE_RADIUS_BYTES * 2
BROWSER_VISIBLE_BYTES = 256
BROWSER_DISASM_WINDOW_LINES = 220
VALUE_TYPE_OPTIONS = (
    ("I8", "I8"),
    ("I16", "I16"),
    ("I32", "I32"),
    ("I64", "I64"),
    ("Float", "Float"),
    ("Double", "Double"),
)


class BrowserTextEdit(QTextEdit):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.wheel_navigate_handler = None
        self.drag_autoscroll_timer = QTimer(self)
        self.drag_autoscroll_timer.setInterval(40)
        self.drag_autoscroll_timer.timeout.connect(self._auto_scroll_selection)
        self.selection_drag_active = False
        self.last_drag_pos = QPoint()

    def wheelEvent(self, event: QWheelEvent) -> None:
        if callable(self.wheel_navigate_handler) and not (event.modifiers() & Qt.ControlModifier):
            delta_y = event.angleDelta().y()
            if delta_y == 0 and not event.pixelDelta().isNull():
                delta_y = event.pixelDelta().y()
            if delta_y != 0 and self.wheel_navigate_handler(delta_y):
                event.accept()
                return
        super().wheelEvent(event)

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.LeftButton:
            self.selection_drag_active = True
            self.last_drag_pos = event.position().toPoint()
            self.drag_autoscroll_timer.start()
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        if self.selection_drag_active:
            self.last_drag_pos = event.position().toPoint()
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.LeftButton:
            self.selection_drag_active = False
            self.drag_autoscroll_timer.stop()
        super().mouseReleaseEvent(event)

    def _auto_scroll_selection(self) -> None:
        if not self.selection_drag_active or not (QApplication.mouseButtons() & Qt.LeftButton):
            self.selection_drag_active = False
            self.drag_autoscroll_timer.stop()
            return

        view_rect = self.viewport().rect()
        margin = 20
        scroll_bar = self.verticalScrollBar()
        scroll_delta = 0

        if self.last_drag_pos.y() < margin:
            scroll_delta = -max(1, self.fontMetrics().lineSpacing())
        elif self.last_drag_pos.y() > view_rect.height() - margin:
            scroll_delta = max(1, self.fontMetrics().lineSpacing())

        if scroll_delta == 0:
            return

        scroll_bar.setValue(
            max(scroll_bar.minimum(), min(scroll_bar.maximum(), scroll_bar.value() + scroll_delta))
        )

        anchor = self.textCursor().anchor()
        clamped_pos = QPoint(
            min(max(self.last_drag_pos.x(), 0), max(0, view_rect.width() - 1)),
            min(max(self.last_drag_pos.y(), 0), max(0, view_rect.height() - 1)),
        )
        target_cursor = self.cursorForPosition(clamped_pos)
        selection_cursor = self.textCursor()
        selection_cursor.setPosition(anchor, QTextCursor.MoveAnchor)
        selection_cursor.setPosition(target_cursor.position(), QTextCursor.KeepAnchor)
        self.setTextCursor(selection_cursor)


class TcpTestWindow(QWidget):
    scan_lan_finished = Signal(object, str, object)

    def __init__(self) -> None:
        super().__init__()
        self.bridge_session = AndroidBridgeSession(timeout_seconds=NETWORK_TIMEOUT_SECONDS)
        self.is_scanning = False
        self.memory_info_data: dict | None = None
        self.scan_page_start = 0
        self.scan_total_count = 0
        self.scan_live_refresh_enabled = False
        self.pointer_scan_running = False
        self.pointer_status_request_inflight = False
        self.hwbp_refresh_inflight = False
        self.saved_items: list[dict[str, str]] = []
        self.browser_base_addr = 0
        self.browser_cache_base = 0
        self.browser_cache_data = b""
        self.browser_current_addr = 0
        self.hwbp_info_data: dict | None = None
        self.live_refresh_timer = QTimer(self)
        self.live_refresh_timer.setInterval(1000)
        self.live_refresh_timer.timeout.connect(self.on_live_refresh_tick)
        self.setWindowTitle("Native TCP Bridge 控制台")
        self.resize(1140, 760)
        self.setMinimumSize(980, 680)
        self.scan_lan_finished.connect(self._on_scan_lan_finished)
        self._setup_ui()
        self.live_refresh_timer.start()

    def _apply_window_style(self) -> None:
        self.setObjectName("mainWindow")
        self.setStyleSheet(
            """
            QWidget#mainWindow {
                background: #f3f3f3;
                color: #1f1f1f;
                font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
                font-size: 12px;
            }
            QDialog {
                background: #f3f3f3;
                color: #1f1f1f;
                font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
                font-size: 12px;
            }
            QFrame#heroCard {
                background: #ffffff;
                border: 1px solid #c8c8c8;
                border-radius: 2px;
            }
            QFrame#panelCard, QFrame#metricCard {
                background: #ffffff;
                border: 1px solid #c8c8c8;
                border-radius: 2px;
            }
            QLabel#heroEyebrow {
                color: #666666;
                font-size: 11px;
                font-weight: 600;
            }
            QLabel#heroTitle {
                color: #111111;
                font-size: 18px;
                font-weight: 600;
            }
            QLabel#heroSubtitle, QLabel#sectionHint {
                color: #5a5a5a;
            }
            QLabel#sectionTitle {
                color: #111111;
                font-size: 13px;
                font-weight: 600;
            }
            QLabel#metricTitle {
                color: #666666;
                font-size: 11px;
                font-weight: 600;
            }
            QLabel#metricValue {
                color: #111111;
                font-size: 13px;
                font-weight: 600;
            }
            QLabel#metricValueStrong {
                color: #0f62a8;
                font-size: 18px;
                font-weight: 700;
            }
            QLabel#statusText {
                color: #1f1f1f;
                font-size: 12px;
                font-weight: 600;
            }
            QLabel#connectionBadge {
                padding: 3px 8px;
                border-radius: 2px;
                font-weight: 600;
            }
            QLabel#connectionBadge[state="connected"] {
                color: #0a5a0a;
                background: #eaf6ea;
                border: 1px solid #8fbc8f;
            }
            QLabel#connectionBadge[state="disconnected"] {
                color: #7a1d1d;
                background: #fbeaea;
                border: 1px solid #d5a6a6;
            }
            QTabWidget::pane {
                background: #ffffff;
                border: 1px solid #c8c8c8;
                border-radius: 0px;
                top: -1px;
            }
            QTabBar::tab {
                color: #2c2c2c;
                background: #ededed;
                border: 1px solid #c8c8c8;
                border-bottom: none;
                border-top-left-radius: 2px;
                border-top-right-radius: 2px;
                padding: 7px 12px;
                margin-right: 2px;
                min-width: 72px;
                font-weight: 600;
            }
            QTabBar::tab:selected {
                color: #111111;
                background: #ffffff;
            }
            QTabBar::tab:!selected:hover {
                color: #111111;
                background: #f5f5f5;
            }
            QSplitter::handle {
                background: #d9d9d9;
            }
            QSplitter::handle:horizontal {
                width: 5px;
            }
            QSplitter::handle:vertical {
                height: 5px;
            }
            QLineEdit, QComboBox, QTextEdit, QTreeWidget {
                background: #ffffff;
                border: 1px solid #bdbdbd;
                border-radius: 1px;
                padding: 4px 6px;
                selection-background-color: #cde6ff;
            }
            QTextEdit, QTreeWidget {
                padding: 6px;
            }
            QLineEdit:focus, QComboBox:focus, QTextEdit:focus, QTreeWidget:focus {
                border: 1px solid #0f62a8;
            }
            QComboBox::drop-down {
                width: 18px;
                border: none;
            }
            QPushButton {
                color: #1f1f1f;
                background: #efefef;
                border: 1px solid #bdbdbd;
                border-radius: 2px;
                padding: 4px 10px;
                font-weight: 600;
            }
            QPushButton:hover {
                background: #f5f5f5;
            }
            QPushButton:pressed {
                background: #e5e5e5;
            }
            QPushButton[variant="primary"] {
                color: #ffffff;
                background: #0f62a8;
                border: 1px solid #0b4f86;
            }
            QPushButton[variant="primary"]:hover {
                background: #0d5a99;
            }
            QPushButton[variant="primary"]:pressed {
                background: #0b4f86;
            }
            QPushButton[variant="secondary"] {
                color: #1f1f1f;
                background: #f6f6f6;
                border: 1px solid #bdbdbd;
            }
            QPushButton[variant="secondary"]:hover {
                background: #f0f0f0;
            }
            QPushButton:disabled {
                color: #8a8a8a;
                background: #f3f3f3;
                border-color: #d0d0d0;
            }
            QMenu {
                background: #ffffff;
                border: 1px solid #bdbdbd;
                padding: 2px;
            }
            QMenu::item {
                padding: 5px 12px;
                border-radius: 0px;
            }
            QMenu::item:selected {
                background: #dcecff;
            }
            """
        )

    def _create_card(self, object_name: str) -> QFrame:
        card = QFrame(self)
        card.setObjectName(object_name)
        return card

    def _set_button_variant(self, button: QPushButton, variant: str) -> None:
        button.setProperty("variant", variant)
        button.style().unpolish(button)
        button.style().polish(button)
        button.update()

    def _create_page_layout(self, page: QWidget, title_text: str, hint_text: str) -> QVBoxLayout:
        layout = QVBoxLayout(page)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(10)

        title = QLabel(title_text)
        title.setObjectName("sectionTitle")
        layout.addWidget(title)

        # 工具型界面默认隐藏页面说明文案，保留紧凑布局。
        _ = hint_text
        return layout

    def _create_section_card(
        self,
        title_text: str | None = None,
        hint_text: str | None = None,
        *,
        parent: QWidget | None = None,
    ) -> tuple[QFrame, QVBoxLayout]:
        card = QFrame(parent if parent is not None else self)
        card.setObjectName("panelCard")
        layout = QVBoxLayout(card)
        layout.setContentsMargins(18, 16, 18, 16)
        layout.setSpacing(12)

        if title_text:
            title = QLabel(title_text)
            title.setObjectName("sectionTitle")
            layout.addWidget(title)

        # 工具型界面默认隐藏区块说明文案，避免占用纵向空间。
        _ = hint_text

        return card, layout

    def _update_connection_badge(self, connected: bool) -> None:
        if not hasattr(self, "connection_badge"):
            return
        self.connection_badge.setText("已连接" if connected else "未连接")
        self.connection_badge.setProperty("state", "connected" if connected else "disconnected")
        self.connection_badge.style().unpolish(self.connection_badge)
        self.connection_badge.style().polish(self.connection_badge)
        self.connection_badge.update()

    def _build_header_panel(self, root: QVBoxLayout) -> None:
        hero_card = self._create_card("heroCard")
        hero_layout = QHBoxLayout(hero_card)
        hero_layout.setContentsMargins(12, 8, 12, 8)
        hero_layout.setSpacing(10)

        self.connection_badge = QLabel("未连接")
        self.connection_badge.setObjectName("connectionBadge")
        self._update_connection_badge(False)

        self.global_pid_label = QLabel("--")
        self.global_pid_label.setObjectName("metricValue")
        self.global_pid_label.setMinimumWidth(60)

        session_info_layout = QHBoxLayout()
        session_info_layout.setContentsMargins(0, 0, 0, 0)
        session_info_layout.setSpacing(10)
        pid_prefix = QLabel("PID")
        pid_prefix.setObjectName("metricTitle")
        session_info_layout.addWidget(self.connection_badge)
        session_info_layout.addWidget(pid_prefix)
        session_info_layout.addWidget(self.global_pid_label)

        self.status_label = QLabel("客户端已启动")
        self.status_label.setObjectName("statusText")
        self.status_label.setWordWrap(False)
        self.status_label.setMinimumWidth(220)

        status_prefix = QLabel("状态")
        status_prefix.setObjectName("metricTitle")
        session_info_layout.addWidget(status_prefix)
        session_info_layout.addWidget(self.status_label)

        pid_input_prefix = QLabel("PID / 包名")
        pid_input_prefix.setObjectName("metricTitle")
        session_info_layout.addWidget(pid_input_prefix)

        self.pid_input = QLineEdit()
        self.pid_input.setPlaceholderText("例如 12345 或 me.hd.ggtutorial")
        self.pid_input.setMinimumWidth(280)
        self.pid_input.returnPressed.connect(self.on_sync_pid)
        session_info_layout.addWidget(self.pid_input, 1)

        self.sync_pid_button = QPushButton("同步 PID")
        self.sync_pid_button.clicked.connect(self.on_sync_pid)
        session_info_layout.addWidget(self.sync_pid_button)

        hero_layout.addLayout(session_info_layout, 1)

        root.addWidget(hero_card)

    def _setup_ui(self) -> None:
        self._apply_window_style()
        root = QVBoxLayout(self)
        root.setContentsMargins(18, 18, 18, 14)
        root.setSpacing(14)

        self._build_header_panel(root)

        self._build_connection_panel(root)

        self.tabs = QTabWidget()
        self.tabs.setDocumentMode(True)
        self.tabs.setUsesScrollButtons(True)
        self.tabs.tabBar().setExpanding(False)
        root.addWidget(self.tabs, 1)

        self.memory_page = QWidget()
        self.search_page = QWidget()
        self.browser_page = QWidget()
        self.pointer_page = QWidget()
        self.breakpoint_page = QWidget()
        self.signature_page = QWidget()
        self.save_page = QWidget()
        self.log_page = QWidget()
        self.settings_page = QWidget()

        self.tabs.addTab(self.memory_page, "内存信息页")
        self.tabs.addTab(self.search_page, "扫描页")
        self.tabs.addTab(self.save_page, "保存页")
        self.tabs.addTab(self.browser_page, "内存浏览页")
        self.tabs.addTab(self.pointer_page, "指针页")
        self.tabs.addTab(self.breakpoint_page, "断点页")
        self.tabs.addTab(self.signature_page, "特征码页")
        self.tabs.addTab(self.log_page, "日志页")
        self.tabs.addTab(self.settings_page, "设置页")
        self.tabs.currentChanged.connect(self.on_tab_changed)

        self._build_memory_page()
        self._build_scan_page()
        self._build_browser_page()
        self._build_pointer_page()
        self._build_breakpoint_page()
        self._build_signature_page()
        self._build_save_page()
        self._build_log_page()

        self._build_settings_page()
        self._log("客户端已启动。")
        self._set_connection_ui(False)
        self._show_connect_login_dialog()

    def _build_connection_panel(self, root: QVBoxLayout) -> None:
        card = self._create_card("panelCard")
        root.addWidget(card)

        layout = QVBoxLayout(card)
        layout.setContentsMargins(14, 12, 14, 12)
        layout.setSpacing(8)

        heading_row = QHBoxLayout()
        heading_row.setSpacing(8)
        title = QLabel("连接")
        title.setObjectName("sectionTitle")
        heading_row.addWidget(title)
        heading_row.addStretch(1)

        self.scan_device_button = QPushButton("扫描设备")
        self._set_button_variant(self.scan_device_button, "secondary")
        self.scan_device_button.clicked.connect(self.on_scan_lan_devices)
        heading_row.addWidget(self.scan_device_button)
        layout.addLayout(heading_row)

        device_row = QHBoxLayout()
        device_row.setSpacing(10)
        device_row.addWidget(QLabel("局域网设备"))
        self.device_combo = QComboBox()
        self.device_combo.setEditable(False)
        self.device_combo.addItem("请点击“扫描设备”获取列表", "")
        device_row.addWidget(self.device_combo, 1)

        device_row.addWidget(QLabel("端口"))
        self.port_input = QLineEdit(str(DEFAULT_PORT))
        self.port_input.setPlaceholderText("请输入目标端口")
        self.port_input.setMaximumWidth(120)
        device_row.addWidget(self.port_input)

        self.test_button = QPushButton("连接到设备")
        self._set_button_variant(self.test_button, "primary")
        self.test_button.clicked.connect(self.on_toggle_connection)
        device_row.addWidget(self.test_button)
        layout.addLayout(device_row)

    def _show_connect_login_dialog(self) -> None:
        if self._is_connected():
            return

        dialog = QDialog(self)
        dialog.setWindowTitle("连接到设备")
        dialog.setModal(True)
        dialog.resize(540, 220)

        layout = QVBoxLayout(dialog)
        layout.setContentsMargins(18, 16, 18, 14)
        layout.setSpacing(10)

        card = self._create_card("panelCard")
        card.setParent(dialog)
        card_layout = QVBoxLayout(card)
        card_layout.setContentsMargins(18, 16, 18, 16)
        card_layout.setSpacing(12)
        layout.addWidget(card)

        device_row = QHBoxLayout()
        device_row.addWidget(QLabel("局域网设备"))
        device_combo = QComboBox()
        device_combo.addItem("请点击“扫描设备”获取列表", "")
        device_row.addWidget(device_combo, 1)
        scan_button = QPushButton("扫描设备")
        self._set_button_variant(scan_button, "secondary")
        device_row.addWidget(scan_button)
        card_layout.addLayout(device_row)

        port_row = QHBoxLayout()
        port_row.addWidget(QLabel("端口"))
        port_input = QLineEdit(str(DEFAULT_PORT))
        port_input.setMaximumWidth(140)
        port_row.addWidget(port_input)
        port_row.addStretch(1)
        card_layout.addLayout(port_row)

        status_label = QLabel("未连接")
        status_label.setObjectName("sectionHint")
        card_layout.addWidget(status_label)

        btn_row = QHBoxLayout()
        btn_row.addStretch(1)
        cancel_btn = QPushButton("取消")
        connect_btn = QPushButton("连接并进入")
        self._set_button_variant(connect_btn, "primary")
        btn_row.addWidget(cancel_btn)
        btn_row.addWidget(connect_btn)
        layout.addLayout(btn_row)

        def on_scan() -> None:
            scan_button.setEnabled(False)
            status_label.setText("正在扫描局域网设备，请稍候...")
            QApplication.processEvents()
            try:
                devices = self._discover_lan_devices()
            except Exception as exc:  # noqa: BLE001
                devices = []
                status_label.setText(f"扫描失败: {exc}")
            else:
                device_combo.clear()
                if not devices:
                    device_combo.addItem("未发现设备，请确认同网段后重试", "")
                    status_label.setText("扫描完成：未发现设备")
                else:
                    for ip_text, mac_text in devices:
                        device_combo.addItem(f"{ip_text}    [{mac_text}]", ip_text)
                    status_label.setText(f"扫描完成：发现 {len(devices)} 台设备")
            scan_button.setEnabled(True)

        def on_connect() -> None:
            host_data = device_combo.currentData()
            host = str(host_data).strip() if host_data is not None else ""
            if not host:
                QMessageBox.warning(dialog, "输入提示", "请先扫描并选择局域网设备。")
                return
            port_text = port_input.text().strip()
            try:
                port = int(port_text, 10)
            except ValueError:
                QMessageBox.warning(dialog, "输入提示", "端口必须是整数。")
                return
            if not (1 <= port <= 65535):
                QMessageBox.warning(dialog, "输入提示", "端口范围必须在 1 到 65535 之间。")
                return

            # 同步到主窗口连接控件后复用原有连接流程。
            self.device_combo.clear()
            self.device_combo.addItem(host, host)
            self.device_combo.setCurrentIndex(0)
            self.port_input.setText(str(port))
            self._connect_device()
            if self._is_connected():
                dialog.accept()
            else:
                status_label.setText(self.status_label.text())

        scan_button.clicked.connect(on_scan)
        connect_btn.clicked.connect(on_connect)
        cancel_btn.clicked.connect(dialog.reject)

        if dialog.exec() != QDialog.Accepted and not self._is_connected():
            QTimer.singleShot(0, self.close)

    def _is_pointer_tab_active(self) -> bool:
        return self.tabs.currentWidget() is self.pointer_page

    def _is_breakpoint_tab_active(self) -> bool:
        return self.tabs.currentWidget() is self.breakpoint_page

    def on_tab_changed(self, _index: int) -> None:
        # 仅在切到指针页时主动刷新一次状态，避免后台持续轮询。
        if self._is_pointer_tab_active() and self._is_connected():
            self.on_pointer_status()
        if self._is_breakpoint_tab_active() and self._is_connected():
            self.on_hwbp_refresh(silent=True)

    def _build_memory_page(self) -> None:
        layout = self._create_page_layout(
            self.memory_page,
            "内存信息",
            "查看完整 memory_info，并按模块名、地址或权限快速筛选当前结果。",
        )

        card, card_layout = self._create_section_card(parent=self.memory_page)
        layout.addWidget(card, 1)

        row = QHBoxLayout()
        row.setSpacing(10)
        self.refresh_memory_button = QPushButton("刷新内存信息")
        self._set_button_variant(self.refresh_memory_button, "primary")
        self.refresh_memory_button.clicked.connect(self.on_refresh_memory_info)
        row.addWidget(self.refresh_memory_button)
        row.addWidget(QLabel("搜索"))
        self.memory_filter_input = QLineEdit()
        self.memory_filter_input.setPlaceholderText("输入模块名/地址/权限关键字")
        self.memory_filter_input.returnPressed.connect(self.on_filter_memory_info)
        row.addWidget(self.memory_filter_input, 1)
        self.filter_memory_button = QPushButton("筛选")
        self.filter_memory_button.clicked.connect(self.on_filter_memory_info)
        row.addWidget(self.filter_memory_button)
        self.clear_filter_button = QPushButton("清空筛选")
        self.clear_filter_button.clicked.connect(self.on_clear_memory_filter)
        row.addWidget(self.clear_filter_button)
        row.addStretch(1)
        card_layout.addLayout(row)

        self.memory_view = QTextEdit()
        self.memory_view.setReadOnly(True)
        self.memory_view.setPlaceholderText("点击“刷新内存信息”后显示可读的 memory_info 结构数据。")
        card_layout.addWidget(self.memory_view, 1)

    def _build_scan_page(self) -> None:
        layout = self._create_page_layout(
            self.search_page,
            "内存扫描",
            "左侧专注查看结果，右侧集中调整类型、模式、分页和扫描动作。",
        )

        splitter = QSplitter(Qt.Horizontal)
        layout.addWidget(splitter, 1)

        left_panel, left_layout = self._create_section_card("扫描结果", parent=self.search_page)
        splitter.addWidget(left_panel)

        self.scan_view = QTextEdit()
        self.scan_view.setReadOnly(True)
        self.scan_view.setPlaceholderText("这里显示 MemScanner 扫描结果。")
        self.scan_view.setContextMenuPolicy(Qt.CustomContextMenu)
        self.scan_view.customContextMenuRequested.connect(self.on_scan_view_context_menu)
        left_layout.addWidget(self.scan_view, 1)

        right_panel, right_layout = self._create_section_card("扫描参数", "调整扫描条件后再执行首次扫描或再次扫描。", parent=self.search_page)
        splitter.addWidget(right_panel)
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)

        row1 = QHBoxLayout()
        row1.setSpacing(10)
        row1.addWidget(QLabel("类型"))
        self.scan_type_combo = QComboBox()
        self._populate_value_type_combo(self.scan_type_combo)
        row1.addWidget(self.scan_type_combo)

        row1.addWidget(QLabel("模式"))
        self.scan_mode_combo = QComboBox()
        self.scan_mode_combo.addItem("未知", "unknown")
        self.scan_mode_combo.addItem("等于", "eq")
        self.scan_mode_combo.addItem("大于", "gt")
        self.scan_mode_combo.addItem("小于", "lt")
        self.scan_mode_combo.addItem("增加", "inc")
        self.scan_mode_combo.addItem("减少", "dec")
        self.scan_mode_combo.addItem("已变化", "changed")
        self.scan_mode_combo.addItem("未变化", "unchanged")
        self.scan_mode_combo.addItem("范围", "range")
        self.scan_mode_combo.addItem("指针", "pointer")
        eq_index = self.scan_mode_combo.findData("eq")
        self.scan_mode_combo.setCurrentIndex(eq_index if eq_index >= 0 else 0)
        row1.addWidget(self.scan_mode_combo)
        row1.addStretch(1)
        right_layout.addLayout(row1)

        row_value = QHBoxLayout()
        row_value.setSpacing(10)
        row_value.addWidget(QLabel("值"))
        self.scan_value_input = QLineEdit()
        self.scan_value_input.setPlaceholderText("例如 100 或 3.14")
        self.scan_value_input.setMinimumWidth(280)
        row_value.addWidget(self.scan_value_input, 1)

        row_value.addWidget(QLabel("范围"))
        self.scan_range_input = QLineEdit("0")
        self.scan_range_input.setPlaceholderText("range 模式使用")
        self.scan_range_input.setMaximumWidth(100)
        row_value.addWidget(self.scan_range_input)
        row_value.addStretch(1)
        right_layout.addLayout(row_value)

        row2 = QHBoxLayout()
        row2.setSpacing(10)
        self.scan_first_button = QPushButton("首次扫描")
        self._set_button_variant(self.scan_first_button, "primary")
        self.scan_first_button.clicked.connect(self.on_scan_first)
        row2.addWidget(self.scan_first_button)

        self.scan_next_button = QPushButton("再次扫描")
        self.scan_next_button.clicked.connect(self.on_scan_next)
        row2.addWidget(self.scan_next_button)

        self.scan_status_button = QPushButton("扫描状态")
        self.scan_status_button.clicked.connect(self.on_scan_status)
        row2.addWidget(self.scan_status_button)

        self.scan_clear_button = QPushButton("清空结果")
        self.scan_clear_button.clicked.connect(self.on_scan_clear)
        row2.addWidget(self.scan_clear_button)

        self.scan_total_label = QLabel("总结果数: 0")
        row2.addWidget(self.scan_total_label)

        row2.addStretch(1)
        right_layout.addLayout(row2)

        row3 = QHBoxLayout()
        row3.setSpacing(10)
        row3.addWidget(QLabel("分页数量"))
        self.scan_page_count_input = QLineEdit("100")
        self.scan_page_count_input.setMaximumWidth(120)
        row3.addWidget(self.scan_page_count_input)

        self.scan_prev_button = QPushButton("上一页")
        self.scan_prev_button.clicked.connect(self.on_scan_prev_page)
        row3.addWidget(self.scan_prev_button)

        self.scan_next_page_button = QPushButton("下一页")
        self.scan_next_page_button.clicked.connect(self.on_scan_next_page)
        row3.addWidget(self.scan_next_page_button)
        row3.addStretch(1)
        right_layout.addLayout(row3)
        right_layout.addStretch(1)

    def _build_browser_page(self) -> None:
        layout = self._create_page_layout(
            self.browser_page,
            "内存浏览",
            "输入地址后按不同视图浏览缓存内容，适合快速定位十六进制、数值和反汇编结果。",
        )

        toolbar_card, toolbar_layout = self._create_section_card(parent=self.browser_page)
        layout.addWidget(toolbar_card)

        row1 = QHBoxLayout()
        row1.setSpacing(10)
        row1.addWidget(QLabel("地址"))
        self.browser_addr_input = QLineEdit("0x0")
        self.browser_addr_input.setPlaceholderText("输入起始地址，如 0x12345678 或 0x12345678+0xA8")
        self.browser_addr_input.returnPressed.connect(self.on_browser_read)
        row1.addWidget(self.browser_addr_input, 1)

        row1.addWidget(QLabel("缓存"))
        self.browser_size_label = QLabel(
            f"{BROWSER_WINDOW_BYTES} 字节缓存（上/下各 {BROWSER_CACHE_RADIUS_BYTES}）"
        )
        self.browser_size_label.setMinimumWidth(120)
        row1.addWidget(self.browser_size_label)

        row1.addWidget(QLabel("显示"))
        self.browser_view_combo = QComboBox()
        self.browser_view_combo.addItem("Hex", "hex")
        self.browser_view_combo.addItem("Hex64", "hex64")
        self.browser_view_combo.addItem("I8", "i8")
        self.browser_view_combo.addItem("I16", "i16")
        self.browser_view_combo.addItem("I32", "i32")
        self.browser_view_combo.addItem("I64", "i64")
        self.browser_view_combo.addItem("Float", "f32")
        self.browser_view_combo.addItem("Double", "f64")
        self.browser_view_combo.addItem("Disasm", "disasm")
        row1.addWidget(self.browser_view_combo)
        self.browser_read_button = QPushButton("读取")
        self._set_button_variant(self.browser_read_button, "primary")
        self.browser_read_button.clicked.connect(self.on_browser_read)
        row1.addWidget(self.browser_read_button)
        row1.addStretch(1)
        toolbar_layout.addLayout(row1)

        viewer_card, viewer_layout = self._create_section_card("浏览结果", parent=self.browser_page)
        layout.addWidget(viewer_card, 1)

        self.browser_view = BrowserTextEdit()
        self.browser_view.setReadOnly(True)
        self.browser_view.setPlaceholderText("内存浏览结果将显示在这里。")
        self.browser_view.wheel_navigate_handler = self.on_browser_wheel_navigate
        viewer_layout.addWidget(self.browser_view, 1)

    def _build_pointer_page(self) -> None:
        layout = self._create_page_layout(
            self.pointer_page,
            "指针扫描",
            "把扫描条件和结果输出拆开，方便连续调整参数并观察状态变化。",
        )

        config_card, config_layout = self._create_section_card("扫描配置", parent=self.pointer_page)
        layout.addWidget(config_card)
        config_layout.setContentsMargins(12, 10, 12, 10)
        config_layout.setSpacing(8)

        row1 = QHBoxLayout()
        row1.setSpacing(8)
        row1.addWidget(QLabel("目标地址"))
        self.pointer_target_input = QLineEdit("0x0")
        self.pointer_target_input.setPlaceholderText("例如 0x12345678")
        row1.addWidget(self.pointer_target_input, 1)

        row1.addWidget(QLabel("深度"))
        self.pointer_depth_input = QLineEdit("5")
        self.pointer_depth_input.setMaximumWidth(72)
        row1.addWidget(self.pointer_depth_input)

        row1.addWidget(QLabel("最大偏移"))
        self.pointer_max_offset_input = QLineEdit("4096")
        self.pointer_max_offset_input.setMaximumWidth(100)
        row1.addWidget(self.pointer_max_offset_input)
        config_layout.addLayout(row1)

        row2 = QHBoxLayout()
        row2.setSpacing(8)
        row2.addWidget(QLabel("基址模式"))
        self.pointer_mode_combo = QComboBox()
        self.pointer_mode_combo.addItem("Module", "module")
        self.pointer_mode_combo.addItem("Manual", "manual")
        self.pointer_mode_combo.addItem("Array", "array")
        self.pointer_mode_combo.setMaximumWidth(120)
        row2.addWidget(self.pointer_mode_combo)

        row2.addWidget(QLabel("模块过滤"))
        self.pointer_filter_input = QLineEdit()
        self.pointer_filter_input.setPlaceholderText("可选，例如 libil2cpp.so")
        row2.addWidget(self.pointer_filter_input, 1)
        config_layout.addLayout(row2)

        row3 = QHBoxLayout()
        row3.setSpacing(8)
        row3.addWidget(QLabel("手动基址"))
        self.pointer_manual_base_input = QLineEdit("0x0")
        self.pointer_manual_base_input.setMaximumWidth(160)
        row3.addWidget(self.pointer_manual_base_input)
        row3.addWidget(QLabel("数组基址"))
        self.pointer_array_base_input = QLineEdit("0x0")
        self.pointer_array_base_input.setMaximumWidth(160)
        row3.addWidget(self.pointer_array_base_input)
        row3.addWidget(QLabel("数组数量"))
        self.pointer_array_count_input = QLineEdit("128")
        self.pointer_array_count_input.setMaximumWidth(100)
        row3.addWidget(self.pointer_array_count_input)
        config_layout.addLayout(row3)

        row4 = QHBoxLayout()
        row4.setSpacing(8)
        self.pointer_scan_button = QPushButton("开始扫描")
        self._set_button_variant(self.pointer_scan_button, "primary")
        self.pointer_scan_button.clicked.connect(self.on_pointer_scan)
        row4.addWidget(self.pointer_scan_button)

        self.pointer_status_button = QPushButton("刷新状态")
        self.pointer_status_button.clicked.connect(self.on_pointer_status)
        row4.addWidget(self.pointer_status_button)

        self.pointer_merge_button = QPushButton("合并Bin")
        self.pointer_merge_button.clicked.connect(self.on_pointer_merge)
        row4.addWidget(self.pointer_merge_button)

        self.pointer_export_button = QPushButton("导出文本")
        self.pointer_export_button.clicked.connect(self.on_pointer_export)
        row4.addWidget(self.pointer_export_button)
        row4.addStretch(1)
        config_layout.addLayout(row4)

        result_card, result_layout = self._create_section_card("扫描输出", parent=self.pointer_page)
        layout.addWidget(result_card, 1)

        self.pointer_status_label = QLabel("扫描状态: 未开始")
        result_layout.addWidget(self.pointer_status_label)

        self.pointer_view = QTextEdit()
        self.pointer_view.setReadOnly(True)
        self.pointer_view.setPlaceholderText("这里显示指针扫描指令与状态。")
        result_layout.addWidget(self.pointer_view, 1)

    def _build_breakpoint_page(self) -> None:
        layout = self._create_page_layout(
            self.breakpoint_page,
            "硬件断点",
            "上方负责配置与寄存器写入，下方查看按 PC 折叠后的断点记录。",
        )

        config_card, config_layout = self._create_section_card("断点配置", parent=self.breakpoint_page)
        layout.addWidget(config_card)

        summary_row = QHBoxLayout()
        summary_row.setSpacing(10)
        self.hwbp_num_brps_label = QLabel("hwbp_info.num_brps: 0")
        summary_row.addWidget(self.hwbp_num_brps_label)
        self.hwbp_num_wrps_label = QLabel("hwbp_info.num_wrps: 0")
        summary_row.addWidget(self.hwbp_num_wrps_label)
        self.hwbp_hit_addr_label = QLabel("hwbp_info.hit_addr: 0x0")
        summary_row.addWidget(self.hwbp_hit_addr_label, 1)
        config_layout.addLayout(summary_row)

        config_row = QHBoxLayout()
        config_row.setSpacing(10)
        config_row.addWidget(QLabel("断点地址"))
        self.hwbp_addr_input = QLineEdit("0x0")
        self.hwbp_addr_input.setPlaceholderText("例如 0x7A12345678")
        config_row.addWidget(self.hwbp_addr_input, 1)

        config_row.addWidget(QLabel("类型"))
        self.hwbp_type_combo = QComboBox()
        self.hwbp_type_combo.addItem("BP_READ", "0")
        self.hwbp_type_combo.addItem("BP_WRITE", "1")
        self.hwbp_type_combo.addItem("BP_READ_WRITE", "2")
        self.hwbp_type_combo.addItem("BP_EXECUTE", "3")
        config_row.addWidget(self.hwbp_type_combo)

        config_row.addWidget(QLabel("范围"))
        self.hwbp_scope_combo = QComboBox()
        self.hwbp_scope_combo.addItem("SCOPE_MAIN_THREAD", "0")
        self.hwbp_scope_combo.addItem("SCOPE_OTHER_THREADS", "1")
        self.hwbp_scope_combo.addItem("SCOPE_ALL_THREADS", "2")
        config_row.addWidget(self.hwbp_scope_combo)

        config_row.addWidget(QLabel("长度"))
        self.hwbp_len_input = QLineEdit("4")
        self.hwbp_len_input.setMaximumWidth(80)
        config_row.addWidget(self.hwbp_len_input)
        config_layout.addLayout(config_row)

        action_row = QHBoxLayout()
        action_row.setSpacing(10)
        self.hwbp_refresh_button = QPushButton("刷新断点信息")
        self.hwbp_refresh_button.clicked.connect(self.on_hwbp_refresh)
        action_row.addWidget(self.hwbp_refresh_button)

        self.hwbp_set_button = QPushButton("设置断点")
        self._set_button_variant(self.hwbp_set_button, "primary")
        self.hwbp_set_button.clicked.connect(self.on_hwbp_set)
        action_row.addWidget(self.hwbp_set_button)

        self.hwbp_remove_button = QPushButton("移除断点")
        self.hwbp_remove_button.clicked.connect(self.on_hwbp_remove_all)
        action_row.addWidget(self.hwbp_remove_button)
        action_row.addStretch(1)
        config_layout.addLayout(action_row)

        edit_row = QHBoxLayout()
        edit_row.setSpacing(10)
        edit_row.addWidget(QLabel("写入字段"))
        self.hwbp_field_combo = QComboBox()
        for field_name in ["pc", "lr", "sp", "pstate", "orig_x0", "syscallno"]:
            self.hwbp_field_combo.addItem(field_name, field_name)
        for reg_idx in range(30):
            field_name = f"x{reg_idx}"
            self.hwbp_field_combo.addItem(field_name, field_name)
        edit_row.addWidget(self.hwbp_field_combo)

        edit_row.addWidget(QLabel("写入值"))
        self.hwbp_value_input = QLineEdit("0x0")
        self.hwbp_value_input.setPlaceholderText("输入十六进制或十进制值")
        edit_row.addWidget(self.hwbp_value_input, 1)

        self.hwbp_write_button = QPushButton("写入寄存器")
        self.hwbp_write_button.clicked.connect(self.on_hwbp_write_field)
        edit_row.addWidget(self.hwbp_write_button)
        config_layout.addLayout(edit_row)

        result_card, result_layout = self._create_section_card("断点记录", parent=self.breakpoint_page)
        layout.addWidget(result_card, 1)

        self.hwbp_tree = QTreeWidget()
        self.hwbp_tree.setHeaderLabels(["断点记录（按 PC 折叠）"])
        self.hwbp_tree.setUniformRowHeights(True)
        self.hwbp_tree.setAlternatingRowColors(True)
        self.hwbp_tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self.hwbp_tree.customContextMenuRequested.connect(self.on_hwbp_tree_context_menu)
        self.hwbp_tree.currentItemChanged.connect(self.on_hwbp_tree_current_item_changed)
        result_layout.addWidget(self.hwbp_tree, 1)

    def _build_signature_page(self) -> None:
        layout = self._create_page_layout(
            self.signature_page,
            "特征码",
            "把保存、过滤和按模式扫描集中到一页，方便连续验证 Signature 文件和模式匹配结果。",
        )

        action_card, action_layout = self._create_section_card("扫描与过滤", parent=self.signature_page)
        layout.addWidget(action_card)

        scan_row = QHBoxLayout()
        scan_row.setSpacing(10)
        scan_row.addWidget(QLabel("目标地址"))
        self.sig_addr_input = QLineEdit("0x0")
        self.sig_addr_input.setPlaceholderText("扫描并保存时使用")
        scan_row.addWidget(self.sig_addr_input, 1)
        scan_row.addWidget(QLabel("范围"))
        self.sig_range_input = QLineEdit("50")
        self.sig_range_input.setMaximumWidth(100)
        scan_row.addWidget(self.sig_range_input)
        scan_row.addWidget(QLabel("文件"))
        self.sig_file_input = QLineEdit("Signature.txt")
        scan_row.addWidget(self.sig_file_input, 1)
        self.sig_scan_addr_button = QPushButton("找特征")
        self._set_button_variant(self.sig_scan_addr_button, "primary")
        self.sig_scan_addr_button.clicked.connect(self.on_sig_scan_address)
        scan_row.addWidget(self.sig_scan_addr_button)
        action_layout.addLayout(scan_row)

        filter_row = QHBoxLayout()
        filter_row.setSpacing(10)
        filter_row.addWidget(QLabel("过滤地址"))
        self.sig_verify_addr_input = QLineEdit("0x0")
        self.sig_verify_addr_input.setPlaceholderText("过滤 Signature.txt")
        filter_row.addWidget(self.sig_verify_addr_input, 1)
        self.sig_filter_button = QPushButton("过滤特征")
        self.sig_filter_button.clicked.connect(self.on_sig_filter)
        filter_row.addWidget(self.sig_filter_button)
        action_layout.addLayout(filter_row)

        pattern_row = QHBoxLayout()
        pattern_row.setSpacing(10)
        pattern_row.addWidget(QLabel("特征码"))
        self.sig_pattern_input = QLineEdit()
        self.sig_pattern_input.setPlaceholderText("例如 A1h ?? FFh 00h")
        pattern_row.addWidget(self.sig_pattern_input, 1)
        pattern_row.addWidget(QLabel("偏移"))
        self.sig_pattern_range_input = QLineEdit("0")
        self.sig_pattern_range_input.setMaximumWidth(100)
        pattern_row.addWidget(self.sig_pattern_range_input)
        self.sig_scan_pattern_button = QPushButton("扫特征")
        self.sig_scan_pattern_button.clicked.connect(self.on_sig_scan_pattern)
        pattern_row.addWidget(self.sig_scan_pattern_button)
        action_layout.addLayout(pattern_row)

        result_card, result_layout = self._create_section_card("执行结果", parent=self.signature_page)
        layout.addWidget(result_card, 1)

        self.sig_status_label = QLabel("特征码状态: 未执行")
        result_layout.addWidget(self.sig_status_label)

        self.sig_view = QTextEdit()
        self.sig_view.setReadOnly(True)
        self.sig_view.setPlaceholderText("这里显示特征码扫描和过滤结果。")
        result_layout.addWidget(self.sig_view, 1)

    def _build_save_page(self) -> None:
        layout = self._create_page_layout(
            self.save_page,
            "保存列表",
            "管理从扫描结果保存的地址，也可以手动追加地址并实时观察值变化。",
        )

        card, card_layout = self._create_section_card(parent=self.save_page)
        layout.addWidget(card, 1)

        manual_row = QHBoxLayout()
        manual_row.setSpacing(10)
        self.saved_count_label = QLabel("已保存: 0")
        manual_row.addWidget(self.saved_count_label)
        self.clear_saved_button = QPushButton("清空保存")
        self.clear_saved_button.clicked.connect(self.on_clear_saved_items)
        manual_row.addWidget(self.clear_saved_button)
        manual_row.addWidget(QLabel("手动添加"))
        self.saved_manual_addr_input = QLineEdit()
        self.saved_manual_addr_input.setPlaceholderText("输入地址，如 0x12345678")
        self.saved_manual_addr_input.returnPressed.connect(self.on_add_saved_item)
        manual_row.addWidget(self.saved_manual_addr_input, 1)
        manual_row.addWidget(QLabel("类型"))
        self.saved_manual_type_combo = QComboBox()
        self._populate_value_type_combo(self.saved_manual_type_combo)
        manual_row.addWidget(self.saved_manual_type_combo)
        self.saved_manual_add_button = QPushButton("添加地址")
        self._set_button_variant(self.saved_manual_add_button, "primary")
        self.saved_manual_add_button.clicked.connect(self.on_add_saved_item)
        manual_row.addWidget(self.saved_manual_add_button)
        card_layout.addLayout(manual_row)

        self.saved_view = QTextEdit()
        self.saved_view.setReadOnly(True)
        self.saved_view.setPlaceholderText("在扫描结果里右键保存后，这里会显示地址和数据。")
        self.saved_view.setContextMenuPolicy(Qt.CustomContextMenu)
        self.saved_view.customContextMenuRequested.connect(self.on_saved_view_context_menu)
        card_layout.addWidget(self.saved_view, 1)

    def _build_settings_page(self) -> None:
        layout = self._create_page_layout(
            self.settings_page,
            "设置",
            "这里目前保留为引导页，未连接设备时会自动停留在这个页面。",
        )
        card, card_layout = self._create_section_card(parent=self.settings_page)
        layout.addWidget(card, 1)
        tip = QLabel("设置页已留空。请先在顶部连接到设备后再使用其它页面功能。")
        tip.setWordWrap(True)
        card_layout.addWidget(tip)
        card_layout.addStretch(1)

    def _build_log_page(self) -> None:
        layout = self._create_page_layout(
            self.log_page,
            "运行日志",
            "集中查看当前客户端操作、状态变化和网络通信过程中记录的关键信息。",
        )

        card, card_layout = self._create_section_card(parent=self.log_page)
        layout.addWidget(card, 1)

        self.log_view = QTextEdit()
        self.log_view.setReadOnly(True)
        card_layout.addWidget(self.log_view, 1)

        clear_row = QHBoxLayout()
        clear_button = QPushButton("清空日志")
        clear_button.clicked.connect(self.log_view.clear)
        clear_row.addWidget(clear_button)
        clear_row.addStretch(1)
        card_layout.addLayout(clear_row)

    def _log(self, text: str) -> None:
        time_text = datetime.now().strftime("%H:%M:%S")
        self.log_view.append(f"[{time_text}] {text}")

    def _set_status(self, text: str) -> None:
        self.status_label.setText(text)
        self._log(f"状态: {text}")

    def _is_connected(self) -> bool:
        return self.bridge_session.is_connected()

    def _set_feature_gate(self, connected: bool) -> None:
        # 未连接时，仅保留“设置页”可操作。
        for i in range(self.tabs.count()):
            page = self.tabs.widget(i)
            enabled = connected or (page is self.settings_page)
            self.tabs.setTabEnabled(i, enabled)
        if not connected and self.tabs.currentWidget() is not self.settings_page:
            self.tabs.setCurrentWidget(self.settings_page)
        self.pid_input.setEnabled(connected)
        self.sync_pid_button.setEnabled(connected)

    def _discover_lan_devices(self) -> list[tuple[str, str]]:
        return [(device.host, device.mac) for device in discover_lan_devices()]

    def _finish_scan_lan_devices(self, devices: list[tuple[str, str]], previous_ip: str, error_text: str | None = None) -> None:
        self.device_combo.clear()

        if error_text:
            self.device_combo.addItem("扫描失败，请重试", "")
            self._set_status(f"扫描失败：{error_text}")
        elif not devices:
            self.device_combo.addItem("未发现设备，请确认同网段后重试", "")
            self._set_status("扫描完成：未发现设备")
        else:
            selected_index = 0
            for idx, (ip_text, mac_text) in enumerate(devices):
                self.device_combo.addItem(f"{ip_text}    [{mac_text}]", ip_text)
                if previous_ip and previous_ip == ip_text:
                    selected_index = idx
            self.device_combo.setCurrentIndex(selected_index)
            self._set_status(f"扫描完成：发现 {len(devices)} 台设备")

        self.is_scanning = False
        self.scan_device_button.setEnabled(True)

    def _on_scan_lan_finished(self, devices_obj: object, previous_ip: str, error_obj: object) -> None:
        devices = devices_obj if isinstance(devices_obj, list) else []
        error_text = str(error_obj) if isinstance(error_obj, str) and error_obj else None
        self._finish_scan_lan_devices(devices, previous_ip, error_text)

    def on_scan_lan_devices(self) -> None:
        if self.is_scanning:
            return

        self.is_scanning = True
        self.scan_device_button.setEnabled(False)
        previous_ip = self.device_combo.currentData() if self.device_combo.count() > 0 else ""
        self.device_combo.clear()
        self.device_combo.addItem("正在扫描局域网设备，请稍候...", "")
        self._set_status("正在扫描局域网设备，请稍候...")

        def worker() -> None:
            try:
                devices = self._discover_lan_devices()
                error_text = None
            except Exception as exc:  # noqa: BLE001
                devices = []
                error_text = str(exc)
            self.scan_lan_finished.emit(devices, str(previous_ip), error_text)

        threading.Thread(target=worker, daemon=True).start()

    def _parse_endpoint(self) -> tuple[str, int] | None:
        host_data = self.device_combo.currentData()
        host = str(host_data).strip() if host_data is not None else ""
        if not host:
            QMessageBox.warning(self, "输入提示", "请先扫描并选择局域网设备。")
            return None

        port_text = self.port_input.text().strip()
        try:
            port = int(port_text, 10)
        except ValueError:
            QMessageBox.warning(self, "输入提示", "端口必须是整数。")
            return None

        if not (1 <= port <= 65535):
            QMessageBox.warning(self, "输入提示", "端口范围必须在 1 到 65535 之间。")
            return None

        return host, port

    def _set_connection_ui(self, connected: bool) -> None:
        self.test_button.setText("断开连接" if connected else "连接到设备")
        self._set_button_variant(self.test_button, "secondary" if connected else "primary")
        self.device_combo.setEnabled(not connected)
        self.scan_device_button.setEnabled(not connected)
        self.port_input.setEnabled(not connected)
        self._update_connection_badge(connected)
        self._set_feature_gate(connected)

    def _disconnect_device(self, reason: str | None = None) -> None:
        self.bridge_session.disconnect()
        self._set_connection_ui(False)
        self.scan_live_refresh_enabled = False
        self.pointer_scan_running = False
        self.pointer_status_request_inflight = False
        self.hwbp_refresh_inflight = False
        self.global_pid_label.setText("--")
        self.pointer_status_label.setText("扫描状态: 未连接")
        self.hwbp_num_brps_label.setText("hwbp_info.num_brps: 0")
        self.hwbp_num_wrps_label.setText("hwbp_info.num_wrps: 0")
        self.hwbp_hit_addr_label.setText("hwbp_info.hit_addr: 0x0")
        self.browser_base_addr = 0
        self.browser_cache_base = 0
        self.browser_cache_data = b""
        self.browser_current_addr = 0
        self.hwbp_tree.clear()
        if reason:
            self._set_status(reason)

    def _connect_device(self) -> None:
        endpoint = self._parse_endpoint()
        if endpoint is None:
            return

        host, port = endpoint
        try:
            self.bridge_session.connect(host, port)
        except BridgeConnectionError as exc:
            self._set_status(str(exc))
            return

        self._set_connection_ui(True)
        self._set_status(f"已连接到设备：{host}:{port}")

    def _send_operation(self, operation: str, params: dict | None = None, *, log_enabled: bool = True) -> dict | None:
        operation_name = operation.strip()
        request_params = params or {}
        if log_enabled:
            self._log(f"发送操作: {operation_name} params={json.dumps(request_params, ensure_ascii=False)}")
        if not self.bridge_session.is_connected():
            self._set_status("未连接设备，请先点击“连接到设备”")
            return None

        try:
            bridge_response = self.bridge_session.call_operation(operation_name, request_params)
            response_payload = bridge_response.to_dict()
        except BridgeConnectionError as exc:
            self._disconnect_device(f"连接已断开：{exc}")
            return None
        except BridgeError as exc:
            response_payload = {
                "ok": False,
                "operation": operation_name,
                "message": "",
                "error": str(exc),
                "pairs": {},
                "data": None,
            }

        if log_enabled:
            log_summary = {
                "ok": bool(response_payload.get("ok", False)),
                "operation": response_payload.get("operation", operation_name),
                "message": response_payload.get("message", ""),
                "error": response_payload.get("error", ""),
            }
            self._log(f"收到响应: {json.dumps(log_summary, ensure_ascii=False)}")
        return response_payload

    @staticmethod
    def _response_ok(response: dict | None) -> bool:
        return isinstance(response, dict) and bool(response.get("ok", False))

    @staticmethod
    def _response_error_text(response: dict | None) -> str:
        if not isinstance(response, dict):
            return "无响应"
        error = str(response.get("error", "")).strip()
        if error:
            return error
        message = str(response.get("message", "")).strip()
        if message:
            return message
        return "未知错误"

    @staticmethod
    def _safe_int(value: object, default: int = 0) -> int:
        if isinstance(value, int):
            return value
        if isinstance(value, float):
            return int(value)
        if isinstance(value, str):
            text = value.strip()
            if not text:
                return default
            try:
                return int(text, 0)
            except ValueError:
                return default
        return default

    @staticmethod
    def _format_addr(value: object) -> str:
        addr = TcpTestWindow._safe_int(value, 0)
        return f"0x{addr:016X}"

    @staticmethod
    def _format_prot(prot_value: object) -> str:
        prot = TcpTestWindow._safe_int(prot_value, 0)
        return f"{'r' if (prot & 1) else '-'}{'w' if (prot & 2) else '-'}{'x' if (prot & 4) else '-'}({prot})"

    def _module_matches_keyword(self, module: object, keyword: str) -> bool:
        if not isinstance(module, dict):
            return keyword in str(module).lower()

        name = str(module.get("name", "")).lower()
        if keyword in name:
            return True

        segs_raw = module.get("segs")
        segs = segs_raw if isinstance(segs_raw, list) else []
        for seg in segs:
            if not isinstance(seg, dict):
                continue
            index_val = self._safe_int(seg.get("index"), 0)
            prot_val = self._safe_int(seg.get("prot"), 0)
            start_val = self._safe_int(seg.get("start"), 0)
            end_val = self._safe_int(seg.get("end"), 0)
            tokens = [
                str(index_val),
                str(prot_val),
                self._format_prot(prot_val).lower(),
                f"0x{start_val:x}",
                f"0x{end_val:x}",
                str(start_val),
                str(end_val),
            ]
            if any(keyword in token for token in tokens):
                return True
        return False

    def _region_matches_keyword(self, region: object, keyword: str) -> bool:
        if not isinstance(region, dict):
            return keyword in str(region).lower()
        start_val = self._safe_int(region.get("start"), 0)
        end_val = self._safe_int(region.get("end"), 0)
        tokens = [f"0x{start_val:x}", f"0x{end_val:x}", str(start_val), str(end_val)]
        return any(keyword in token for token in tokens)

    def _populate_value_type_combo(self, combo: QComboBox, *, default_type: str = "I32") -> None:
        combo.clear()
        for label, data in VALUE_TYPE_OPTIONS:
            combo.addItem(label, data)
        index = combo.findData(default_type)
        combo.setCurrentIndex(index if index >= 0 else 0)

    def _filter_memory_info(self, info: dict, keyword: str) -> dict:
        keyword_text = keyword.strip().lower()
        if not keyword_text:
            return info

        modules_raw = info.get("modules")
        regions_raw = info.get("regions")
        modules = modules_raw if isinstance(modules_raw, list) else []
        regions = regions_raw if isinstance(regions_raw, list) else []

        filtered_modules = [m for m in modules if self._module_matches_keyword(m, keyword_text)]
        filtered_regions = [r for r in regions if self._region_matches_keyword(r, keyword_text)]

        return {
            "status": info.get("status", 0),
            "module_count": len(filtered_modules),
            "region_count": len(filtered_regions),
            "modules": filtered_modules,
            "regions": filtered_regions,
            "_source_module_count": len(modules),
            "_source_region_count": len(regions),
            "_filter_keyword": keyword,
        }

    def _format_memory_info_text(self, info: dict) -> str:
        status = self._safe_int(info.get("status"), 0)
        module_count = self._safe_int(info.get("module_count"), 0)
        region_count = self._safe_int(info.get("region_count"), 0)
        source_module_count = self._safe_int(info.get("_source_module_count"), module_count)
        source_region_count = self._safe_int(info.get("_source_region_count"), region_count)
        filter_keyword = str(info.get("_filter_keyword", "")).strip()

        modules_raw = info.get("modules")
        regions_raw = info.get("regions")
        modules = modules_raw if isinstance(modules_raw, list) else []
        regions = regions_raw if isinstance(regions_raw, list) else []

        lines: list[str] = []
        lines.append("【内存信息概要】")
        lines.append(f"状态: {status}")
        if filter_keyword:
            lines.append(f"筛选关键字: {filter_keyword}")
        lines.append(f"模块数量(头部): {module_count}")
        lines.append(f"内存区域数量(头部): {region_count}")
        lines.append(f"模块数量(实际): {len(modules)}")
        lines.append(f"内存区域数量(实际): {len(regions)}")
        if filter_keyword:
            lines.append(f"模块总量(筛选前): {source_module_count}")
            lines.append(f"区域总量(筛选前): {source_region_count}")
        lines.append("")

        lines.append("【模块信息】")
        if not modules:
            lines.append("无模块数据。")
        else:
            for idx, module in enumerate(modules, start=1):
                if isinstance(module, dict):
                    name = str(module.get("name", ""))
                    segs_raw = module.get("segs")
                    segs = segs_raw if isinstance(segs_raw, list) else []
                    seg_count = self._safe_int(module.get("seg_count"), len(segs))
                else:
                    name = str(module)
                    segs = []
                    seg_count = 0

                lines.append(f"{idx}. 模块: {name if name else '(空名称)'}")
                lines.append(f"   段数量: {seg_count}")
                if not segs:
                    lines.append("   段列表: (空)")
                    continue

                for seg_idx, seg in enumerate(segs, start=1):
                    if not isinstance(seg, dict):
                        lines.append(f"   - 段{seg_idx}: 非法数据")
                        continue
                    seg_index = self._safe_int(seg.get("index"), -999)
                    prot_text = self._format_prot(seg.get("prot"))
                    start_text = self._format_addr(seg.get("start"))
                    end_text = self._format_addr(seg.get("end"))
                    lines.append(
                        f"   - 段{seg_idx}: index={seg_index} prot={prot_text} start={start_text} end={end_text}"
                    )
        lines.append("")

        lines.append("【可扫描内存区域】")
        if not regions:
            lines.append("无区域数据。")
        else:
            for idx, region in enumerate(regions, start=1):
                if not isinstance(region, dict):
                    lines.append(f"{idx}. 非法数据")
                    continue
                start_text = self._format_addr(region.get("start"))
                end_text = self._format_addr(region.get("end"))
                lines.append(f"{idx}. start={start_text} end={end_text}")

        return "\n".join(lines)

    def _render_memory_info(self) -> None:
        if self.memory_info_data is None:
            self.memory_view.setPlainText("暂无内存信息，请先点击“刷新内存信息”。")
            return

        keyword = self.memory_filter_input.text().strip()
        filtered_info = self._filter_memory_info(self.memory_info_data, keyword)
        self.memory_view.setPlainText(self._format_memory_info_text(filtered_info))

    def on_toggle_connection(self) -> None:
        if self._is_connected():
            self._disconnect_device("已断开连接")
            return
        self._connect_device()

    def _build_scan_request(self, is_first: bool) -> tuple[str, dict] | None:
        data_type_data = self.scan_type_combo.currentData()
        data_type = str(data_type_data).strip() if data_type_data is not None else self.scan_type_combo.currentText().strip()
        mode_data = self.scan_mode_combo.currentData()
        mode = str(mode_data).strip() if mode_data is not None else ""
        value = self.scan_value_input.text().strip()
        range_text = self.scan_range_input.text().strip()

        operation = "scan.start" if is_first else "scan.refine"
        params: dict[str, str] = {
            "value_type": data_type,
            "mode": mode,
        }
        if mode == "unknown":
            if range_text:
                params["range_max"] = range_text
            return operation, params

        if not value:
            QMessageBox.warning(self, "输入提示", "当前扫描模式需要输入“值”。")
            return None

        params["value"] = value
        if mode == "range":
            if not range_text:
                QMessageBox.warning(self, "输入提示", "range 模式需要输入“范围”。")
                return None
            params["range_max"] = range_text
            return operation, params

        if range_text and range_text != "0":
            params["range_max"] = range_text
        return operation, params

    @staticmethod
    def _make_hwbp_field_item(index: int, field_name: str, value: int, text: str) -> QTreeWidgetItem:
        item = QTreeWidgetItem([text])
        item.setData(0, Qt.UserRole, index)
        item.setData(0, Qt.UserRole + 2, field_name)
        item.setData(0, Qt.UserRole + 3, f"0x{value:X}")
        return item

    def _extract_hwbp_index_from_tree_item(self, item: QTreeWidgetItem | None) -> int | None:
        current = item
        while current is not None:
            data = current.data(0, Qt.UserRole)
            if data is not None:
                try:
                    idx = int(str(data), 10)
                except (TypeError, ValueError):
                    idx = -1
                if idx >= 0:
                    return idx
            current = current.parent()
        return None

    def _extract_hwbp_group_pc_from_tree_item(self, item: QTreeWidgetItem | None) -> int | None:
        current = item
        while current is not None:
            data = current.data(0, Qt.UserRole + 1)
            if data is not None:
                try:
                    pc = int(str(data), 10)
                except (TypeError, ValueError):
                    pc = -1
                if pc >= 0:
                    return pc
            current = current.parent()
        return None

    def _build_hwbp_group_payload(self, pc: int) -> dict | None:
        if pc < 0 or not isinstance(self.hwbp_info_data, dict):
            return None
        records_raw = self.hwbp_info_data.get("records")
        records = records_raw if isinstance(records_raw, list) else []
        matched_records = [
            record
            for record in records
            if isinstance(record, dict) and self._safe_int(record.get("pc"), -1) == pc
        ]
        if not matched_records:
            return None
        total_hit = sum(self._safe_int(record.get("hit_count"), 0) for record in matched_records)
        type_tags = sorted({self._decode_hwbp_rw_text(record) for record in matched_records})
        return {
            "pc": pc,
            "pc_hex": f"0x{pc:X}",
            "record_count": len(matched_records),
            "total_hit_count": total_hit,
            "types": type_tags,
            "records": matched_records,
        }

    def _remove_hwbp_group(self, group_payload: dict) -> None:
        records_raw = group_payload.get("records")
        records = records_raw if isinstance(records_raw, list) else []
        indices = sorted(
            {
                self._safe_int(record.get("index"), -1)
                for record in records
                if isinstance(record, dict) and self._safe_int(record.get("index"), -1) >= 0
            },
            reverse=True,
        )
        if not indices:
            QMessageBox.warning(self, "删除失败", "当前折叠下没有可删除的 hwbp_record。")
            return

        success_count = 0
        failed_indices: list[int] = []
        for idx in indices:
            response = self._send_operation("breakpoint.record.remove", {"index": idx})
            if self._response_ok(response):
                success_count += 1
            else:
                failed_indices.append(idx)

        self.on_hwbp_refresh(silent=True)

        pc = self._safe_int(group_payload.get("pc"), 0)
        if failed_indices:
            failed_text = ", ".join(str(idx) for idx in failed_indices)
            QMessageBox.warning(self, "删除失败", f"折叠删除未完全成功，失败索引: {failed_text}")
            self._set_status(f"已删除 PC 0x{pc:X} 折叠中的 {success_count} 条，失败 {len(failed_indices)} 条")
            return

        self._set_status(f"已删除 PC 0x{pc:X} 折叠，共 {success_count} 条记录")

    def _get_selected_hwbp_index(self) -> int | None:
        return self._extract_hwbp_index_from_tree_item(self.hwbp_tree.currentItem())

    def _decode_hwbp_rw_text(self, rec: dict) -> str:
        rw_text = str(rec.get("rw", "")).lower()
        if rw_text == "write":
            return "写入"
        if rw_text == "read":
            return "读取"
        return "未知"

    def _render_hwbp_tree(self, records: list[dict]) -> None:
        prev_expanded_pc: set[int] = set()
        for i in range(self.hwbp_tree.topLevelItemCount()):
            top_item = self.hwbp_tree.topLevelItem(i)
            if top_item is None or not top_item.isExpanded():
                continue
            pc_val = self._safe_int(top_item.data(0, Qt.UserRole + 1), -1)
            if pc_val >= 0:
                prev_expanded_pc.add(pc_val)
        had_previous_items = self.hwbp_tree.topLevelItemCount() > 0
        old_scroll = self.hwbp_tree.verticalScrollBar().value()

        self.hwbp_tree.clear()
        if not records:
            empty_item = QTreeWidgetItem(["暂无 hwbp_record 命中记录"])
            self.hwbp_tree.addTopLevelItem(empty_item)
            return

        grouped: dict[int, list[dict]] = {}
        for rec in records:
            pc = self._safe_int(rec.get("pc"), 0)
            grouped.setdefault(pc, []).append(rec)

        for pc, rec_list in sorted(grouped.items(), key=lambda kv: kv[0]):
            total_hit = sum(self._safe_int(r.get("hit_count"), 0) for r in rec_list)
            type_tags = sorted({self._decode_hwbp_rw_text(r) for r in rec_list})
            type_text = "/".join(type_tags) if type_tags else "未知"
            top = QTreeWidgetItem(
                [f"PC 0x{pc:X}  |  记录 {len(rec_list)} 条  |  总命中 {total_hit}  |  触发类型 {type_text}"]
            )
            top.setData(0, Qt.UserRole + 1, pc)
            self.hwbp_tree.addTopLevelItem(top)
            top.setExpanded((not had_previous_items) or (pc in prev_expanded_pc))

            for rec in rec_list:
                idx = self._safe_int(rec.get("index"), -1)
                hit_count = self._safe_int(rec.get("hit_count"), 0)
                rw_text = self._decode_hwbp_rw_text(rec)
                summary_item = QTreeWidgetItem([f"[{idx}] 命中 {hit_count} 次  |  类型 {rw_text}"])
                summary_item.setData(0, Qt.UserRole, idx)
                top.addChild(summary_item)

                lr = self._safe_int(rec.get("lr"), 0)
                sp = self._safe_int(rec.get("sp"), 0)
                orig_x0 = self._safe_int(rec.get("orig_x0"), 0)
                syscallno = self._safe_int(rec.get("syscallno"), 0)
                pstate = self._safe_int(rec.get("pstate"), 0)
                top.addChild(self._make_hwbp_field_item(idx, "pc", pc, f"  PC: 0x{pc:X}"))
                top.addChild(self._make_hwbp_field_item(idx, "lr", lr, f"  LR: 0x{lr:X}"))
                top.addChild(self._make_hwbp_field_item(idx, "sp", sp, f"  SP: 0x{sp:X}"))
                top.addChild(self._make_hwbp_field_item(idx, "orig_x0", orig_x0, f"  ORIG_X0: 0x{orig_x0:X}"))
                top.addChild(self._make_hwbp_field_item(idx, "syscallno", syscallno, f"  SYSCALLNO: {syscallno}"))
                top.addChild(self._make_hwbp_field_item(idx, "pstate", pstate, f"  PSTATE: 0x{pstate:X}"))

                regs_raw = rec.get("regs")
                regs = regs_raw if isinstance(regs_raw, list) else []
                regs_title = QTreeWidgetItem(["  寄存器快照 X0~X29"])
                regs_title.setData(0, Qt.UserRole, idx)
                top.addChild(regs_title)
                for reg_idx, reg_val in enumerate(regs):
                    reg_hex = self._safe_int(reg_val, 0)
                    top.addChild(self._make_hwbp_field_item(idx, f"x{reg_idx}", reg_hex, f"    X{reg_idx}: 0x{reg_hex:X}"))

                if rw_text == "写入":
                    write_title = QTreeWidgetItem(["  写入寄存器候选"])
                    write_title.setData(0, Qt.UserRole, idx)
                    top.addChild(write_title)
                    x0_val = self._safe_int(regs[0], 0) if len(regs) > 0 else 0
                    x1_val = self._safe_int(regs[1], 0) if len(regs) > 1 else 0
                    top.addChild(self._make_hwbp_field_item(idx, "x0", x0_val, f"    候选写入值(X0): 0x{x0_val:X}"))
                    top.addChild(self._make_hwbp_field_item(idx, "x1", x1_val, f"    候选写入地址(X1): 0x{x1_val:X}"))

                separator = QTreeWidgetItem([""])
                separator.setData(0, Qt.UserRole, idx)
                top.addChild(separator)

        self.hwbp_tree.resizeColumnToContents(0)
        self.hwbp_tree.verticalScrollBar().setValue(
            min(old_scroll, self.hwbp_tree.verticalScrollBar().maximum())
        )

    def _render_hwbp_info(self, info: dict) -> None:
        num_brps = self._safe_int(info.get("num_brps"), 0)
        num_wrps = self._safe_int(info.get("num_wrps"), 0)
        hit_addr = self._safe_int(info.get("hit_addr"), 0)
        self.hwbp_num_brps_label.setText(f"hwbp_info.num_brps: {num_brps}")
        self.hwbp_num_wrps_label.setText(f"hwbp_info.num_wrps: {num_wrps}")
        self.hwbp_hit_addr_label.setText(f"hwbp_info.hit_addr: 0x{hit_addr:X}")
        records_raw = info.get("records")
        records = records_raw if isinstance(records_raw, list) else []
        self._render_hwbp_tree(records)

    @staticmethod
    def _format_sig_result(data: dict) -> str:
        lines: list[str] = []
        count = TcpTestWindow._safe_int(data.get("count"), 0)
        returned_count = TcpTestWindow._safe_int(data.get("returned_count"), 0)
        truncated = bool(data.get("truncated", False))
        changed_count = data.get("changed_count")
        total_count = data.get("total_count")
        if changed_count is not None and total_count is not None:
            lines.append(f"过滤变化: {changed_count}/{total_count}")
        lines.append(f"匹配数量: {count}")
        lines.append(f"返回数量: {returned_count}")
        lines.append(f"是否截断: {'是' if truncated else '否'}")
        if "file" in data:
            lines.append(f"文件: {data.get('file')}")
        if "pattern" in data and str(data.get("pattern", "")):
            lines.append(f"特征码: {data.get('pattern')}")
        if "range" in data:
            lines.append(f"偏移: {data.get('range')}")
        if "old_signature" in data:
            lines.append(f"旧特征: {data.get('old_signature')}")
        if "new_signature" in data:
            lines.append(f"新特征: {data.get('new_signature')}")
        lines.append("")
        lines.append("【匹配地址】")
        matches_raw = data.get("matches")
        matches = matches_raw if isinstance(matches_raw, list) else []
        if not matches:
            lines.append("无")
        else:
            for idx, item in enumerate(matches, start=1):
                if isinstance(item, dict):
                    lines.append(f"{idx:04d}. {item.get('addr_hex', '0x0')}")
                else:
                    lines.append(f"{idx:04d}. {item}")
        return "\n".join(lines)

    def _render_scan_page(self, payload: dict) -> None:
        start = self._safe_int(payload.get("start"), 0)
        items_raw = payload.get("items")
        items = items_raw if isinstance(items_raw, list) else []

        lines: list[str] = []
        if not items:
            lines.append("本页没有结果。")
        else:
            for idx, item in enumerate(items, start=1):
                if not isinstance(item, dict):
                    lines.append(f"{start + idx:08d} | 非法数据")
                    continue
                addr_hex = str(item.get("addr_hex", ""))
                value = str(item.get("value", ""))
                lines.append(f"{start + idx:08d} | {addr_hex:<18} | {value}")

        self._set_text_preserve_interaction(self.scan_view, "\n".join(lines))

    @staticmethod
    def _parse_scan_line(line: str) -> tuple[str, str] | None:
        match = re.match(r"^\s*\d+\s*\|\s*(0x[0-9A-Fa-f]+)\s*\|\s*(.*)$", line)
        if not match:
            return None
        addr = match.group(1).strip()
        value = match.group(2).strip()
        if not addr:
            return None
        return addr, value

    @staticmethod
    def _build_read_operation_for_type(type_token: str, addr: str) -> tuple[str, dict] | None:
        mapping = {
            "I8": "u8",
            "I16": "u16",
            "I32": "u32",
            "I64": "u64",
            "Float": "f32",
            "Double": "f64",
        }
        value_type = mapping.get(type_token)
        if value_type is None:
            return None
        return "memory.read_value", {"address": addr, "value_type": value_type}

    @staticmethod
    def _extract_value_field(response: dict | None) -> str | None:
        if not isinstance(response, dict) or not bool(response.get("ok", False)):
            return None
        data = response.get("data")
        if isinstance(data, dict):
            value = data.get("value")
            if value is not None:
                return str(value)

        pairs = response.get("pairs")
        if isinstance(pairs, dict):
            value = pairs.get("value")
            if value is not None:
                return str(value)
        return None

    @staticmethod
    def _normalize_saved_note(note_text: str) -> str:
        return " ".join(part.strip() for part in note_text.replace("\r", "\n").split("\n") if part.strip())

    def _append_saved_item(self, addr: str, value: str, type_token: str, *, note: str = "") -> None:
        self.saved_items.append(
            {
                "addr": addr,
                "value": value,
                "type": type_token,
                "locked": "0",
                "note": self._normalize_saved_note(note),
            }
        )

    def _read_saved_item_value(self, type_token: str, addr: str) -> str:
        if not self._is_connected():
            return ""
        request = self._build_read_operation_for_type(type_token, addr)
        if request is None:
            return ""
        operation, params = request
        response = self._send_operation(operation, params, log_enabled=False)
        value = self._extract_value_field(response)
        return value if value is not None else ""

    def _ensure_saved_item_value(self, item: dict[str, str]) -> bool:
        if item.get("value", ""):
            return True
        addr = item.get("addr", "")
        type_token = item.get("type", "")
        if not addr or not type_token:
            return False
        value = self._read_saved_item_value(type_token, addr)
        if not value:
            return False
        item["value"] = value
        return True

    @staticmethod
    def _set_text_preserve_interaction(editor: QTextEdit, text: str) -> bool:
        if editor.toPlainText() == text:
            return True

        cursor = editor.textCursor()
        if editor.hasFocus() and cursor.hasSelection():
            return False

        old_scroll = editor.verticalScrollBar().value()
        old_pos = cursor.position()

        editor.setPlainText(text)

        new_cursor = editor.textCursor()
        new_cursor.setPosition(min(old_pos, len(text)))
        editor.setTextCursor(new_cursor)
        editor.verticalScrollBar().setValue(min(old_scroll, editor.verticalScrollBar().maximum()))
        return True

    def _refresh_saved_view(self, force: bool = False) -> None:
        self.saved_count_label.setText(f"已保存: {len(self.saved_items)}")
        if not self.saved_items:
            self._set_text_preserve_interaction(self.saved_view, "")
            return

        lines = []
        for idx, item in enumerate(self.saved_items, start=1):
            addr = item.get("addr", "")
            value = item.get("value", "") or "--"
            type_token = item.get("type", "")
            lock_text = "锁定" if item.get("locked", "0") == "1" else "未锁"
            note = self._normalize_saved_note(item.get("note", ""))
            note_text = f" | 备注: {note}" if note else ""
            lines.append(f"{idx}. {addr} | {value} | {type_token} | {lock_text}{note_text}")
        text = "\n".join(lines)

        if force:
            # 强制刷新：保存滚动位置，直接设置文本
            scroll = self.saved_view.verticalScrollBar().value()
            self.saved_view.setPlainText(text)
            self.saved_view.verticalScrollBar().setValue(scroll)
        else:
            self._set_text_preserve_interaction(self.saved_view, text)

    def on_scan_view_context_menu(self, pos) -> None:
        cursor = self.scan_view.cursorForPosition(pos)

        # 检查是否有选中文本（支持多选）
        text_cursor = self.scan_view.textCursor()
        if text_cursor.hasSelection():
            # 用户已通过鼠标选中多行，使用选中的文本
            selected_text = text_cursor.selectedText()
            # Qt 使用 U+2029 作为段落分隔符
            lines = selected_text.replace('\u2029', '\n').split('\n')
        else:
            # 没有选中文本，选择当前行
            cursor.select(cursor.SelectionType.LineUnderCursor)
            lines = [cursor.selectedText().strip()]

        # 解析所有选中的行
        parsed_items = []
        for line_text in lines:
            line_text = line_text.strip()
            if not line_text:
                continue
            parsed = self._parse_scan_line(line_text)
            if parsed is not None:
                parsed_items.append(parsed)

        if not parsed_items:
            return

        menu = QMenu(self.scan_view)
        save_action = menu.addAction(f"保存到保存页 ({len(parsed_items)} 项)" if len(parsed_items) > 1 else "保存到保存页")
        action = menu.exec(self.scan_view.mapToGlobal(pos))
        if action != save_action:
            return

        type_data = self.scan_type_combo.currentData()
        type_token = str(type_data).strip() if type_data is not None else self.scan_type_combo.currentText().strip()

        for addr, value in parsed_items:
            self._append_saved_item(addr, value, type_token)

        self._refresh_saved_view()
        if len(parsed_items) == 1:
            self._set_status(f"已保存: {parsed_items[0][0]} -> {parsed_items[0][1]}")
        else:
            self._set_status(f"已保存 {len(parsed_items)} 项")

    def on_add_saved_item(self) -> None:
        addr_text = self.saved_manual_addr_input.text().strip()
        if not addr_text:
            QMessageBox.warning(self, "输入提示", "请输入要手动添加的地址。")
            return

        try:
            addr_value = int(addr_text, 0)
        except ValueError:
            QMessageBox.warning(self, "输入提示", "地址格式无效，请输入十进制或 0x 开头的十六进制。")
            return

        if addr_value < 0:
            QMessageBox.warning(self, "输入提示", "地址不能为负数。")
            return

        type_data = self.saved_manual_type_combo.currentData()
        type_token = str(type_data).strip() if type_data is not None else self.saved_manual_type_combo.currentText().strip()
        addr = self._format_addr(addr_value)
        value = self._read_saved_item_value(type_token, addr)

        self._append_saved_item(addr, value, type_token)
        self.saved_manual_addr_input.clear()
        self._refresh_saved_view()
        self._set_status(f"已手动添加地址: {addr}")
        self.saved_manual_addr_input.setFocus()

    def _edit_saved_item_note(self, item: dict[str, str]) -> bool:
        addr = item.get("addr", "")
        current_note = item.get("note", "")
        note_text, accepted = QInputDialog.getText(
            self,
            "文字备注",
            f"为地址 {addr} 设置备注：",
            QLineEdit.Normal,
            current_note,
        )
        if not accepted:
            return False

        normalized_note = self._normalize_saved_note(note_text)
        if normalized_note == self._normalize_saved_note(current_note):
            return False

        item["note"] = normalized_note
        self._set_status(f"已更新备注: {addr}" if normalized_note else f"已清空备注: {addr}")
        return True

    def _saved_index_from_line(self, line_text: str) -> int | None:
        match = re.match(r"^\s*(\d+)\.", line_text)
        if not match:
            return None
        idx = int(match.group(1), 10) - 1
        if idx < 0 or idx >= len(self.saved_items):
            return None
        return idx

    def on_saved_view_context_menu(self, pos) -> None:
        # 检查是否有选中文本（支持多选）
        text_cursor = self.saved_view.textCursor()
        if text_cursor.hasSelection():
            # 用户已通过鼠标选中多行，使用选中的文本
            selected_text = text_cursor.selectedText()
            # Qt 使用 U+2029 作为段落分隔符
            lines = selected_text.replace('\u2029', '\n').split('\n')
        else:
            # 没有选中文本，选择当前行
            cursor = self.saved_view.cursorForPosition(pos)
            cursor.select(cursor.SelectionType.LineUnderCursor)
            lines = [cursor.selectedText().strip()]

        # 解析所有选中的行，获取索引
        item_indices = []
        for line_text in lines:
            line_text = line_text.strip()
            if not line_text:
                continue
            item_idx = self._saved_index_from_line(line_text)
            if item_idx is not None:
                item_indices.append(item_idx)

        if not item_indices:
            return

        # 检查选中项的锁定状态
        items = [self.saved_items[idx] for idx in item_indices]
        locked_count = sum(1 for item in items if item.get("locked", "0") == "1")
        unlocked_count = len(items) - locked_count

        menu = QMenu(self.saved_view)
        if len(items) == 1:
            note_action = menu.addAction("编辑备注")
            clear_note_action = None
            if self._normalize_saved_note(items[0].get("note", "")):
                clear_note_action = menu.addAction("清空备注")
            menu.addSeparator()
            # 单选：显示锁定/取消锁定
            locked = items[0].get("locked", "0") == "1"
            lock_action = menu.addAction("取消锁定" if locked else "锁定此项")
        else:
            # 多选：显示批量操作选项
            note_action = None
            clear_note_action = None
            lock_action = None
            actions = {}
            if unlocked_count > 0:
                actions["lock"] = menu.addAction(f"锁定 ({unlocked_count} 项)")
            if locked_count > 0:
                actions["unlock"] = menu.addAction(f"取消锁定 ({locked_count} 项)")

        action = menu.exec(self.saved_view.mapToGlobal(pos))

        if len(items) == 1:
            if action == note_action:
                if self._edit_saved_item_note(items[0]):
                    self._refresh_saved_view(force=True)
                return
            if action == clear_note_action:
                items[0]["note"] = ""
                self._refresh_saved_view(force=True)
                self._set_status(f"已清空备注: {items[0].get('addr', '')}")
                return
            if action != lock_action:
                return
            # 处理单项锁定/取消锁定
            item = items[0]
            locked = item.get("locked", "0") == "1"
            addr = item.get("addr", "")
            type_token = item.get("type", "")
            value = item.get("value", "")
            if not addr or not type_token:
                return

            if locked:
                response = self._send_operation("lock.unset", {"address": addr})
                if not self._response_ok(response):
                    QMessageBox.warning(self, "锁定失败", f"取消锁定失败: {self._response_error_text(response)}")
                    return
                item["locked"] = "0"
                self._set_status(f"已取消锁定: {addr}")
            else:
                if not value and not self._ensure_saved_item_value(item):
                    QMessageBox.warning(self, "锁定失败", f"锁定前读取当前值失败: {addr}")
                    return
                value = item.get("value", "")
                response = self._send_operation("lock.set", {"address": addr, "value_type": type_token, "value": value})
                if not self._response_ok(response):
                    QMessageBox.warning(self, "锁定失败", f"锁定失败: {self._response_error_text(response)}")
                    return
                item["locked"] = "1"
                self._set_status(f"已锁定: {addr} = {value}")
        else:
            # 处理批量操作
            if action not in actions.values():
                return

            success_count = 0
            fail_count = 0

            if action == actions.get("lock"):
                # 批量锁定未锁定的项
                for item in items:
                    if item.get("locked", "0") == "1":
                        continue
                    addr = item.get("addr", "")
                    type_token = item.get("type", "")
                    value = item.get("value", "")
                    if not addr or not type_token:
                        fail_count += 1
                        continue
                    if not value and not self._ensure_saved_item_value(item):
                        fail_count += 1
                        continue
                    value = item.get("value", "")
                    response = self._send_operation("lock.set", {"address": addr, "value_type": type_token, "value": value})
                    if self._response_ok(response):
                        item["locked"] = "1"
                        success_count += 1
                    else:
                        fail_count += 1
                self._set_status(f"已锁定 {success_count} 项" + (f"，失败 {fail_count} 项" if fail_count > 0 else ""))

            elif action == actions.get("unlock"):
                # 批量取消锁定
                for item in items:
                    if item.get("locked", "0") != "1":
                        continue
                    addr = item.get("addr", "")
                    if not addr:
                        fail_count += 1
                        continue
                    response = self._send_operation("lock.unset", {"address": addr})
                    if self._response_ok(response):
                        item["locked"] = "0"
                        success_count += 1
                    else:
                        fail_count += 1
                self._set_status(f"已取消锁定 {success_count} 项" + (f"，失败 {fail_count} 项" if fail_count > 0 else ""))

        # 清除选择后强制刷新显示
        cursor = self.saved_view.textCursor()
        cursor.clearSelection()
        self.saved_view.setTextCursor(cursor)
        self._refresh_saved_view(force=True)

    def on_clear_saved_items(self) -> None:
        self.saved_items.clear()
        self._refresh_saved_view()
        self._set_status("保存页已清空")

    def _get_scan_page_size(self, *, silent: bool = False) -> int | None:
        count_text = self.scan_page_count_input.text().strip()
        try:
            page_count = int(count_text, 10)
        except ValueError:
            if not silent:
                QMessageBox.warning(self, "输入提示", "分页数量必须是整数。")
            return None

        if page_count <= 0:
            if not silent:
                QMessageBox.warning(self, "输入提示", "分页数量必须大于 0。")
            return None
        return page_count

    def _fetch_scan_page(self, start: int, *, silent: bool = False) -> bool:
        page_count = self._get_scan_page_size(silent=silent)
        if page_count is None:
            return False

        type_data = self.scan_type_combo.currentData()
        type_token = str(type_data).strip() if type_data is not None else self.scan_type_combo.currentText().strip()

        response = self._send_operation(
            "scan.page",
            {"start": start, "count": page_count, "value_type": type_token},
            log_enabled=not silent,
        )
        if response is None:
            return False
        if not self._response_ok(response):
            if not silent:
                QMessageBox.warning(self, "获取失败", self._response_error_text(response))
                self._set_status("获取扫描结果失败")
            return False

        data = response.get("data")
        if not isinstance(data, dict):
            if not silent:
                QMessageBox.warning(self, "解析失败", "扫描结果格式异常。")
            return False

        self._render_scan_page(data)
        self.scan_page_start = self._safe_int(data.get("start"), start)
        self.scan_total_count = self._safe_int(data.get("total_count"), 0)
        self.scan_total_label.setText(f"总结果数: {self.scan_total_count}")
        self.scan_live_refresh_enabled = True
        if not silent:
            total = self.scan_total_count
            self._set_status(f"扫描结果已刷新：start={self.scan_page_start}, total={total}")
        return True

    def on_scan_first(self) -> None:
        request = self._build_scan_request(is_first=True)
        if request is None:
            return
        operation, params = request
        response = self._send_operation(operation, params)
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "扫描失败", self._response_error_text(response))
            self._set_status("首次扫描失败")
            return
        self._set_status("首次扫描已执行")
        self.on_scan_status()
        self.scan_page_start = 0
        self._fetch_scan_page(self.scan_page_start)

    def on_scan_next(self) -> None:
        request = self._build_scan_request(is_first=False)
        if request is None:
            return
        operation, params = request
        response = self._send_operation(operation, params)
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "扫描失败", self._response_error_text(response))
            self._set_status("再次扫描失败")
            return
        self._set_status("再次扫描已执行")
        self.on_scan_status()
        self.scan_page_start = 0
        self._fetch_scan_page(self.scan_page_start)

    def on_scan_clear(self) -> None:
        response = self._send_operation("scan.clear")
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "清空失败", self._response_error_text(response))
            return
        self.scan_view.clear()
        self.scan_page_start = 0
        self.scan_total_count = 0
        self.scan_live_refresh_enabled = False
        self.scan_total_label.setText("总结果数: 0")
        self._set_status("扫描结果已清空")

    def on_scan_status(self) -> None:
        response = self._send_operation("scan.status")
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "状态失败", self._response_error_text(response))
            return
        pairs = response.get("pairs", {})
        if not isinstance(pairs, dict):
            pairs = {}
        scanning = pairs.get("scanning", "0")
        progress = pairs.get("progress", "0")
        count = pairs.get("count", "0")
        self.scan_total_label.setText(f"总结果数: {count}")
        self._set_status(f"扫描状态: scanning={scanning}, progress={progress}, count={count}")

    def on_scan_fetch_page(self) -> None:
        if self.scan_page_start < 0:
            self.scan_page_start = 0
        self._fetch_scan_page(self.scan_page_start)

    def on_scan_prev_page(self) -> None:
        page_count = self._get_scan_page_size()
        if page_count is None:
            return
        self.scan_page_start = max(0, self.scan_page_start - page_count)
        self._fetch_scan_page(self.scan_page_start)

    def on_scan_next_page(self) -> None:
        page_count = self._get_scan_page_size()
        if page_count is None:
            return
        if self.scan_total_count > 0 and self.scan_page_start + page_count >= self.scan_total_count:
            self._set_status("已经是最后一页")
            return
        self.scan_page_start = self.scan_page_start + page_count
        self._fetch_scan_page(self.scan_page_start)

    def _parse_browser_addr(self) -> int | None:
        text = self.browser_addr_input.text().strip()
        if not text:
            QMessageBox.warning(self, "输入提示", "请输入地址。")
            return None
        addr = self._parse_address_expression(text)
        if addr is None:
            QMessageBox.warning(self, "输入提示", "地址格式无效，支持如 0x12345678+0xA8。")
            return None
        if addr < 0:
            QMessageBox.warning(self, "输入提示", "地址必须为非负数。")
            return None
        return addr

    @staticmethod
    def _parse_address_expression(text: str) -> int | None:
        expr = re.sub(r"\s+", "", text)
        if not expr:
            return None

        parts = re.split(r"([+-])", expr)
        if not parts:
            return None

        try:
            total = int(parts[0], 0)
        except ValueError:
            return None

        index = 1
        while index + 1 < len(parts):
            operator = parts[index]
            operand_text = parts[index + 1]
            if operator not in {"+", "-"} or not operand_text:
                return None
            try:
                operand = int(operand_text, 0)
            except ValueError:
                return None
            total = total + operand if operator == "+" else total - operand
            index += 2

        if index != len(parts):
            return None
        return total

    @staticmethod
    def _browser_window_size() -> int:
        return BROWSER_WINDOW_BYTES

    def _browser_cache_request_base(self, addr: int) -> int:
        if BROWSER_CACHE_RADIUS_BYTES <= 0:
            return max(0, addr)
        return max(0, addr - BROWSER_CACHE_RADIUS_BYTES)

    def _browser_visible_size(self, view_mode: str) -> int:
        unit = max(1, self._browser_scroll_unit(view_mode))
        visible_size = max(unit, BROWSER_VISIBLE_BYTES - (BROWSER_VISIBLE_BYTES % unit))
        return min(visible_size, self._browser_window_size())

    def _browser_cache_contains(self, addr: int, visible_size: int) -> bool:
        if not self.browser_cache_data:
            return False
        cache_start = self.browser_cache_base
        cache_end = cache_start + len(self.browser_cache_data)
        return cache_start <= addr and addr + visible_size <= cache_end

    def _render_browser_cached_view(self, addr: int, view_mode: str) -> bool:
        visible_size = self._browser_visible_size(view_mode)
        if not self._browser_cache_contains(addr, visible_size):
            return False

        offset = addr - self.browser_cache_base
        data = self.browser_cache_data[offset : offset + visible_size]
        if view_mode == "hex":
            text = self._render_hex_dump(addr, data)
        elif view_mode == "hex64":
            text = self._render_hex64_dump(addr, data)
        else:
            text = self._render_typed_dump(addr, data, view_mode)

        self.browser_current_addr = addr
        self.browser_addr_input.setText(f"0x{addr:X}")
        self.browser_view.setPlainText(text)
        return True

    @staticmethod
    def _hex_to_bytes(hex_text: str) -> bytes | None:
        compact = re.sub(r"[^0-9A-Fa-f]", "", hex_text)
        if not compact or len(compact) % 2 != 0:
            return None
        try:
            return bytes.fromhex(compact)
        except ValueError:
            return None

    @staticmethod
    def _browser_mode_to_token(mode_text: str) -> str:
        mapping = {
            "Hex": "hex",
            "Hex64": "hex64",
            "I8": "i8",
            "I16": "i16",
            "I32": "i32",
            "I64": "i64",
            "Float": "f32",
            "Double": "f64",
            "Disasm": "disasm",
        }
        return mapping.get(mode_text, "hex")

    def _current_browser_view_mode(self) -> str:
        mode_data = self.browser_view_combo.currentData()
        if mode_data is not None:
            return str(mode_data).strip()
        return self._browser_mode_to_token(self.browser_view_combo.currentText().strip())

    @staticmethod
    def _browser_scroll_unit(view_mode: str) -> int:
        mapping = {
            "hex": 16,
            "hex64": 8,
            "i8": 1,
            "i16": 2,
            "i32": 4,
            "i64": 8,
            "f32": 4,
            "f64": 8,
        }
        return mapping.get(view_mode, 16)

    def on_browser_wheel_navigate(self, delta_y: int) -> bool:
        view_mode = self._current_browser_view_mode()
        step_count = max(1, abs(delta_y) // 120) if abs(delta_y) >= 120 else 1

        if view_mode == "disasm":
            line_delta = -step_count if delta_y > 0 else step_count
            self._move_disasm_view(line_delta)
            return True

        current = self.browser_current_addr
        if current <= 0 and self.browser_addr_input.text().strip():
            parsed_addr = self._parse_browser_addr()
            if parsed_addr is None:
                return False
            current = parsed_addr
        size = self._browser_window_size()

        byte_delta = self._browser_scroll_unit(view_mode) * step_count
        next_addr = max(0, current - byte_delta) if delta_y > 0 else current + byte_delta
        if self._render_browser_cached_view(next_addr, view_mode):
            return True
        self._refresh_browser_view(next_addr, size)
        return True

    @staticmethod
    def _render_hex_dump(addr: int, data: bytes) -> str:
        lines: list[str] = []
        for offset in range(0, len(data), 16):
            chunk = data[offset : offset + 16]
            hex_part = " ".join(f"{b:02X}" for b in chunk)
            ascii_part = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
            lines.append(f"0x{addr + offset:016X}  {hex_part:<47}  {ascii_part}")
        return "\n".join(lines)

    @staticmethod
    def _render_hex64_dump(addr: int, data: bytes) -> str:
        lines: list[str] = []
        for offset in range(0, len(data), 8):
            chunk = data[offset : offset + 8]
            if len(chunk) == 8:
                value = struct.unpack("<Q", chunk)[0]
                lines.append(f"0x{addr + offset:016X}  0x{value:016X}")
            else:
                raw_hex = " ".join(f"{b:02X}" for b in chunk)
                lines.append(f"0x{addr + offset:016X}  {raw_hex}")
        return "\n".join(lines)

    @staticmethod
    def _render_typed_dump(addr: int, data: bytes, fmt: str) -> str:
        mapping = {
            "i8": ("<b", 1),
            "i16": ("<h", 2),
            "i32": ("<i", 4),
            "i64": ("<q", 8),
            "f32": ("<f", 4),
            "f64": ("<d", 8),
            "I8": ("<b", 1),
            "I16": ("<h", 2),
            "I32": ("<i", 4),
            "I64": ("<q", 8),
            "Float": ("<f", 4),
            "Double": ("<d", 8),
        }
        if fmt not in mapping:
            return TcpTestWindow._render_hex_dump(addr, data)

        unpack_fmt, unit = mapping[fmt]
        lines: list[str] = []
        for offset in range(0, len(data) - (len(data) % unit), unit):
            chunk = data[offset : offset + unit]
            try:
                value = struct.unpack(unpack_fmt, chunk)[0]
            except struct.error:
                continue
            lines.append(f"0x{addr + offset:016X}  {value}")
        if not lines:
            lines.append("没有可显示的数据。")
        return "\n".join(lines)

    @staticmethod
    def _extract_disasm_window(snapshot: dict) -> tuple[list[dict], int]:
        base_addr = TcpTestWindow._safe_int(snapshot.get("base"), 0)
        disasm_list_raw = snapshot.get("disasm")
        disasm_list = disasm_list_raw if isinstance(disasm_list_raw, list) else []
        if not disasm_list:
            return [], base_addr

        scroll_idx = TcpTestWindow._safe_int(snapshot.get("disasm_scroll_idx"), 0)
        scroll_idx = max(0, min(scroll_idx, len(disasm_list) - 1))
        end_idx = min(len(disasm_list), scroll_idx + BROWSER_DISASM_WINDOW_LINES)

        window_items: list[dict] = []
        for item in disasm_list[scroll_idx:end_idx]:
            if isinstance(item, dict):
                window_items.append(item)

        if not window_items:
            return [], base_addr

        visible_addr = TcpTestWindow._safe_int(window_items[0].get("address"), base_addr)
        return window_items, visible_addr

    @staticmethod
    def _render_disasm_dump(snapshot: dict) -> str:
        lines: list[str] = []
        window_items, _visible_addr = TcpTestWindow._extract_disasm_window(snapshot)
        if not window_items:
            return "没有可显示的反汇编结果。"

        for item in window_items:
            address_hex = str(item.get("address_hex", "0x0"))
            bytes_hex = str(item.get("bytes_hex", ""))
            mnemonic = str(item.get("mnemonic", "")).strip()
            op_str = str(item.get("op_str", "")).strip()
            if op_str:
                lines.append(f"{address_hex:<18} {bytes_hex:<24} {mnemonic} {op_str}")
            else:
                lines.append(f"{address_hex:<18} {bytes_hex:<24} {mnemonic}")

        return "\n".join(lines) if lines else "没有可显示的反汇编结果。"

    def _read_viewer_snapshot(self) -> dict | None:
        response = self._send_operation("viewer.snapshot", log_enabled=False)
        if response is None:
            return None
        if not self._response_ok(response):
            QMessageBox.warning(self, "读取失败", f"内存浏览读取失败: {self._response_error_text(response)}")
            return None
        data = response.get("data")
        if not isinstance(data, dict):
            QMessageBox.warning(self, "解析失败", "浏览器数据格式异常。")
            return None
        return data

    def _open_viewer_snapshot(self, addr: int, mode_token: str) -> dict | None:
        open_resp = self._send_operation(
            "viewer.open",
            {"address": f"0x{addr:X}", "view_format": mode_token},
            log_enabled=False,
        )
        if not self._response_ok(open_resp):
            QMessageBox.warning(self, "读取失败", f"打开浏览器失败: {self._response_error_text(open_resp)}")
            return None

        snapshot = self._read_viewer_snapshot()
        if snapshot is None:
            return None
        if not bool(snapshot.get("read_success", False)):
            QMessageBox.warning(self, "读取失败", "MemViewer 读取失败。")
            return None
        return snapshot

    def _refresh_disasm_view(self, addr: int) -> None:
        mode_data = self.browser_view_combo.currentData()
        mode_token = str(mode_data).strip() if mode_data is not None else self._browser_mode_to_token(self.browser_view_combo.currentText().strip())
        snapshot = self._open_viewer_snapshot(addr, mode_token)
        if not isinstance(snapshot, dict):
            return

        base_addr = self._safe_int(snapshot.get("base"), addr)
        visible_addr = self._extract_disasm_window(snapshot)[1]
        self.browser_base_addr = base_addr
        self.browser_current_addr = visible_addr
        self.browser_addr_input.setText(f"0x{visible_addr:X}")
        self.browser_view.setPlainText(self._render_disasm_dump(snapshot))

    def _move_disasm_view(self, lines: int) -> None:
        move_resp = self._send_operation("viewer.move", {"lines": lines}, log_enabled=False)
        if not self._response_ok(move_resp):
            QMessageBox.warning(self, "移动失败", f"反汇编移动失败: {self._response_error_text(move_resp)}")
            return

        snapshot = self._read_viewer_snapshot()
        if snapshot is None:
            return
        base_addr = self._safe_int(snapshot.get("base"), 0)
        visible_addr = self._extract_disasm_window(snapshot)[1]
        self.browser_base_addr = base_addr
        self.browser_current_addr = visible_addr
        self.browser_addr_input.setText(f"0x{visible_addr:X}")
        self.browser_view.setPlainText(self._render_disasm_dump(snapshot))

    def _refresh_browser_view(self, addr: int, size: int) -> None:
        mode_data = self.browser_view_combo.currentData()
        view_mode = str(mode_data).strip() if mode_data is not None else self._browser_mode_to_token(self.browser_view_combo.currentText().strip())
        if self._render_browser_cached_view(addr, view_mode):
            return
        request_base = self._browser_cache_request_base(addr)
        snapshot = self._open_viewer_snapshot(request_base, view_mode)
        if not isinstance(snapshot, dict):
            return
        data = self._hex_to_bytes(str(snapshot.get("data_hex", "")))
        if data is None:
            QMessageBox.warning(self, "读取失败", "MemViewer HEX 数据解析失败。")
            return
        base_addr = self._safe_int(snapshot.get("base"), addr)
        if size > 0:
            data = data[:size]
        self.browser_base_addr = base_addr
        self.browser_cache_base = base_addr
        self.browser_cache_data = data
        if not self._render_browser_cached_view(addr, view_mode):
            self.browser_current_addr = base_addr
            self.browser_addr_input.setText(f"0x{base_addr:X}")
            self.browser_view.setPlainText("缓存长度不足以显示当前视图。")

    def on_browser_read(self) -> None:
        mode_data = self.browser_view_combo.currentData()
        view_mode = str(mode_data).strip() if mode_data is not None else self._browser_mode_to_token(self.browser_view_combo.currentText().strip())
        if view_mode == "disasm":
            addr = self._parse_browser_addr()
            if addr is None:
                return
            self.browser_current_addr = addr
            self._refresh_disasm_view(addr)
            return

        addr = self._parse_browser_addr()
        if addr is None:
            return
        self.browser_current_addr = addr
        size = self._browser_window_size()
        self._refresh_browser_view(addr, size)

    def _build_pointer_scan_request(self) -> tuple[str, dict] | None:
        target_text = self.pointer_target_input.text().strip()
        depth_text = self.pointer_depth_input.text().strip()
        max_offset_text = self.pointer_max_offset_input.text().strip()
        filter_text = self.pointer_filter_input.text().strip()

        try:
            target = int(target_text, 0)
            depth = int(depth_text, 10)
            max_offset = int(max_offset_text, 10)
        except ValueError:
            QMessageBox.warning(self, "输入提示", "目标地址/深度/最大偏移格式无效。")
            return None

        if target <= 0 or depth <= 0 or max_offset <= 0:
            QMessageBox.warning(self, "输入提示", "目标地址、深度、最大偏移必须大于 0。")
            return None

        mode_data = self.pointer_mode_combo.currentData()
        mode = str(mode_data).strip() if mode_data is not None else "module"
        params: dict[str, str] = {
            "mode": mode,
            "target": f"0x{target:X}",
            "depth": str(depth),
            "max_offset": str(max_offset),
        }

        if mode == "manual":
            manual_base_text = self.pointer_manual_base_input.text().strip()
            try:
                manual_base = int(manual_base_text, 0)
            except ValueError:
                QMessageBox.warning(self, "输入提示", "手动基址格式无效。")
                return None
            if manual_base <= 0:
                QMessageBox.warning(self, "输入提示", "手动基址必须大于 0。")
                return None
            params["manual_base"] = f"0x{manual_base:X}"
        elif mode == "array":
            array_base_text = self.pointer_array_base_input.text().strip()
            array_count_text = self.pointer_array_count_input.text().strip()
            try:
                array_base = int(array_base_text, 0)
                array_count = int(array_count_text, 10)
            except ValueError:
                QMessageBox.warning(self, "输入提示", "数组基址或数组数量格式无效。")
                return None
            if array_base <= 0 or array_count <= 0:
                QMessageBox.warning(self, "输入提示", "数组基址和数组数量必须大于 0。")
                return None
            params["array_base"] = f"0x{array_base:X}"
            params["array_count"] = str(array_count)

        if filter_text:
            params["module_filter"] = filter_text
        return "pointer.scan", params

    def on_pointer_scan(self) -> None:
        request = self._build_pointer_scan_request()
        if request is None:
            return

        operation, params = request
        response = self._send_operation(operation, params)
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "指针扫描失败", self._response_error_text(response))
            self._set_status("指针扫描启动失败")
            return

        self.pointer_view.append(f"启动操作: {operation} {json.dumps(params, ensure_ascii=False)}")
        self.pointer_scan_running = True
        self._set_status("指针扫描任务已启动")
        self.on_pointer_status()

    def _update_pointer_status_from_response(self, response: dict, *, silent: bool = False) -> None:
        if not self._response_ok(response):
            if not silent:
                QMessageBox.warning(self, "状态失败", self._response_error_text(response))
            return

        pairs = response.get("pairs", {})
        if not isinstance(pairs, dict):
            pairs = {}
        scanning = pairs.get("scanning", "0")
        progress = pairs.get("progress", "0")
        count = pairs.get("count", "0")
        self.pointer_scan_running = (scanning == "1")
        status_text = f"扫描状态: scanning={scanning}, progress={progress}, count={count}"
        self.pointer_status_label.setText(status_text)
        if not silent:
            self.pointer_view.append(status_text)
            self._set_status("指针状态已刷新")

    def on_pointer_status(self) -> None:
        response = self._send_operation("pointer.status")
        if response is None:
            return
        self._update_pointer_status_from_response(response, silent=False)

    def _refresh_pointer_status_live(self) -> None:
        if not (self._is_pointer_tab_active() or self.pointer_scan_running):
            return
        if self.pointer_status_request_inflight:
            return

        self.pointer_status_request_inflight = True
        try:
            response = self._send_operation("pointer.status", log_enabled=False)
            if response is None:
                return
            self._update_pointer_status_from_response(response, silent=True)
        finally:
            self.pointer_status_request_inflight = False

    def on_pointer_merge(self) -> None:
        response = self._send_operation("pointer.merge")
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "合并失败", self._response_error_text(response))
            return
        self.pointer_view.append("已触发 Pointer.bin 合并任务。")
        self._set_status("已触发合并任务")

    def on_pointer_export(self) -> None:
        response = self._send_operation("pointer.export")
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "导出失败", self._response_error_text(response))
            return
        self.pointer_view.append("已触发指针链文本导出。")
        self._set_status("已触发导出任务")

    def _refresh_saved_items_live(self) -> None:
        if not self.saved_items:
            return
        if not self._is_connected():
            return

        changed = False
        # 实时刷新时做上限保护，避免一次轮询过重。
        refresh_count = min(len(self.saved_items), 300)
        for i in range(refresh_count):
            item = self.saved_items[i]
            addr = item.get("addr", "")
            type_token = item.get("type", "")
            request = self._build_read_operation_for_type(type_token, addr)
            if request is None:
                continue
            operation, params = request
            response = self._send_operation(operation, params, log_enabled=False)
            value = self._extract_value_field(response)
            if value is None:
                continue
            if item.get("value", "") != value:
                item["value"] = value
                changed = True

        if changed:
            self._refresh_saved_view()

    def on_live_refresh_tick(self) -> None:
        if not self._is_connected():
            return

        if self.scan_live_refresh_enabled:
            self._fetch_scan_page(self.scan_page_start, silent=True)

        self._refresh_saved_items_live()
        self._refresh_pointer_status_live()
        self._refresh_hwbp_info_live()

    def on_sync_pid(self) -> None:
        input_text = self.pid_input.text().strip()
        if not input_text:
            QMessageBox.warning(self, "输入提示", "请输入 PID 或包名。")
            return

        pid_value: int | None = None
        if input_text.isdigit():
            pid_value = int(input_text, 10)
            if pid_value <= 0:
                QMessageBox.warning(self, "输入提示", "PID 必须大于 0。")
                return
        else:
            get_response = self._send_operation("target.pid.get", {"package_name": input_text})
            if not self._response_ok(get_response):
                pid_value = None
            else:
                pairs = get_response.get("pairs", {})
                if isinstance(pairs, dict) and "pid" in pairs:
                    pid_value = self._safe_int(pairs.get("pid"), 0)
                else:
                    pid_value = None
            if pid_value is None:
                QMessageBox.warning(self, "同步失败", "包名获取 PID 失败。")
                self._set_status("同步失败：包名获取 PID 失败")
                return

        set_response = self._send_operation("target.pid.set", {"pid": pid_value})
        if not self._response_ok(set_response):
            QMessageBox.warning(self, "同步失败", "设置全局 PID 失败。")
            self._set_status("同步失败：设置全局 PID 失败")
            return

        current_response = self._send_operation("target.pid.current")
        current_pid: int | None = None
        if self._response_ok(current_response):
            pairs = current_response.get("pairs", {})
            if isinstance(pairs, dict) and "pid" in pairs:
                parsed_pid = self._safe_int(pairs.get("pid"), 0)
                current_pid = parsed_pid if parsed_pid > 0 else None
        if current_pid is None:
            self.global_pid_label.setText("--")
            self._set_status("同步后读取全局 PID 失败")
            return

        self.global_pid_label.setText(str(current_pid))
        self._set_status(f"同步成功：全局PID={current_pid}")

    def on_refresh_memory_info(self) -> None:
        response = self._send_operation("memory.info.full")
        if response is None:
            return

        if not self._response_ok(response):
            err_text = self._response_error_text(response)
            self.memory_view.setPlainText(f"刷新失败：\n{err_text}")
            self._set_status("刷新内存信息失败")
            QMessageBox.warning(self, "刷新失败", f"内存信息刷新失败：{err_text}")
            return

        info = response.get("data")
        if not isinstance(info, dict):
            self.memory_view.setPlainText("JSON 解析失败：返回数据不是对象")
            self._set_status("刷新内存信息失败：JSON解析失败")
            return

        self.memory_info_data = info
        self._render_memory_info()

        module_count = info.get("module_count", "未知")
        region_count = info.get("region_count", "未知")
        self._set_status(f"内存信息刷新成功：模块={module_count}，区域={region_count}")

    def on_filter_memory_info(self) -> None:
        if self.memory_info_data is None:
            QMessageBox.warning(self, "提示", "暂无内存信息，请先点击“刷新内存信息”。")
            return
        self._render_memory_info()
        keyword = self.memory_filter_input.text().strip()
        if keyword:
            self._set_status(f"已应用筛选：{keyword}")
        else:
            self._set_status("已取消筛选，显示全部数据")

    def on_clear_memory_filter(self) -> None:
        self.memory_filter_input.clear()
        if self.memory_info_data is not None:
            self._render_memory_info()
        self._set_status("已清空筛选条件")

    def on_hwbp_refresh(self, silent: bool = False) -> None:
        response = self._send_operation("breakpoint.info", log_enabled=not silent)
        if response is None:
            return
        data = response.get("data") if self._response_ok(response) else None
        if not isinstance(data, dict):
            if not silent:
                QMessageBox.warning(self, "刷新失败", f"断点信息响应异常: {self._response_error_text(response)}")
                self._set_status("断点信息刷新失败")
            return
        self.hwbp_info_data = data
        self._render_hwbp_info(data)
        if not silent:
            self._set_status("断点信息已刷新")

    def on_hwbp_set(self) -> None:
        addr_text = self.hwbp_addr_input.text().strip()
        len_text = self.hwbp_len_input.text().strip()
        type_data = self.hwbp_type_combo.currentData()
        scope_data = self.hwbp_scope_combo.currentData()
        try:
            addr = int(addr_text, 0)
            length = int(len_text, 10)
        except ValueError:
            QMessageBox.warning(self, "输入提示", "断点地址或长度格式无效。")
            return
        if addr <= 0:
            QMessageBox.warning(self, "输入提示", "断点地址必须大于 0。")
            return
        if length <= 0:
            QMessageBox.warning(self, "输入提示", "长度必须大于 0。")
            return
        bp_type = str(type_data) if type_data is not None else "0"
        bp_scope = str(scope_data) if scope_data is not None else "0"
        response = self._send_operation(
            "breakpoint.set",
            {"address": f"0x{addr:X}", "bp_type": bp_type, "bp_scope": bp_scope, "length": length},
        )
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "设置失败", self._response_error_text(response))
            self._set_status("设置硬件断点失败")
            return
        self._set_status("设置硬件断点成功")
        self.on_hwbp_refresh(silent=True)

    def on_hwbp_remove_all(self) -> None:
        response = self._send_operation("breakpoint.clear")
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "移除失败", self._response_error_text(response))
            return
        self._set_status("已移除进程硬件断点")
        self.on_hwbp_refresh(silent=True)

    def on_hwbp_tree_current_item_changed(self, current: QTreeWidgetItem | None, _previous: QTreeWidgetItem | None) -> None:
        if current is None:
            return
        field_data = current.data(0, Qt.UserRole + 2)
        if field_data is not None:
            field_name = str(field_data)
            combo_index = self.hwbp_field_combo.findData(field_name)
            if combo_index >= 0:
                self.hwbp_field_combo.setCurrentIndex(combo_index)
        value_data = current.data(0, Qt.UserRole + 3)
        if value_data is not None:
            self.hwbp_value_input.setText(str(value_data))

    def on_hwbp_write_field(self) -> None:
        value_text = self.hwbp_value_input.text().strip()
        field_data = self.hwbp_field_combo.currentData()
        field_name = str(field_data).strip() if field_data is not None else ""
        try:
            value = int(value_text, 0)
        except ValueError:
            QMessageBox.warning(self, "输入提示", "写入值格式无效。")
            return
        index = self._get_selected_hwbp_index()
        if index is None:
            QMessageBox.warning(self, "输入提示", "请先在断点树里选择一条 hwbp_record。")
            return
        if index < 0:
            QMessageBox.warning(self, "输入提示", "记录索引不能小于 0。")
            return
        if not field_name:
            QMessageBox.warning(self, "输入提示", "请选择要写入的字段。")
            return

        response = self._send_operation(
            "breakpoint.record.update",
            {"index": index, "field": field_name, "value": f"0x{value:X}"},
        )
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "写入失败", self._response_error_text(response))
            return

        self._set_status(f"已写入 hwbp_record[{index}].{field_name}")
        self.on_hwbp_refresh(silent=True)

    def on_hwbp_tree_context_menu(self, pos) -> None:
        item = self.hwbp_tree.itemAt(pos)
        if item is None:
            item = self.hwbp_tree.currentItem()
        group_pc = self._extract_hwbp_group_pc_from_tree_item(item)
        group_payload = self._build_hwbp_group_payload(group_pc if group_pc is not None else -1)

        menu = QMenu(self.hwbp_tree)
        copy_json_action = menu.addAction("复制当前折叠完整JSON")
        delete_action = menu.addAction("删除当前折叠页")

        if group_payload is None:
            copy_json_action.setEnabled(False)
            delete_action.setEnabled(False)

        action = menu.exec(self.hwbp_tree.mapToGlobal(pos))
        if action is None:
            return

        if action == copy_json_action:
            if group_payload is None:
                return
            QApplication.clipboard().setText(json.dumps(group_payload, ensure_ascii=False, indent=2))
            self._set_status(f"已复制 PC 0x{group_payload['pc']:X} 折叠 JSON")
            return
        if action != delete_action or group_payload is None:
            return

        self._remove_hwbp_group(group_payload)

    def _refresh_hwbp_info_live(self) -> None:
        if not self._is_breakpoint_tab_active():
            return
        if self.hwbp_refresh_inflight:
            return
        self.hwbp_refresh_inflight = True
        try:
            self.on_hwbp_refresh(silent=True)
        finally:
            self.hwbp_refresh_inflight = False

    def _render_signature_data(self, data: dict, status_text: str) -> None:
        self.sig_status_label.setText(status_text)
        self._set_text_preserve_interaction(self.sig_view, self._format_sig_result(data))

    def on_sig_scan_address(self) -> None:
        addr_text = self.sig_addr_input.text().strip()
        range_text = self.sig_range_input.text().strip()
        file_name = self.sig_file_input.text().strip() or "Signature.txt"
        try:
            addr = int(addr_text, 0)
            scan_range = int(range_text, 10)
        except ValueError:
            QMessageBox.warning(self, "输入提示", "目标地址或范围格式无效。")
            return
        response = self._send_operation(
            "signature.scan_address",
            {"address": f"0x{addr:X}", "range": scan_range, "file_name": file_name},
        )
        if response is None:
            return
        if not self._response_ok(response):
            QMessageBox.warning(self, "执行失败", self._response_error_text(response))
            self.sig_status_label.setText("特征码状态: 扫描并保存失败")
            return
        self.sig_status_label.setText("特征码状态: 扫描并保存成功")
        self._set_status(f"特征码已保存到 {file_name}")

    def _scan_signature_file_data(self, file_name: str) -> dict | None:
        response = self._send_operation("signature.scan_file", {"file_name": file_name})
        if response is None:
            return None
        data = response.get("data") if self._response_ok(response) else None
        if not isinstance(data, dict):
            QMessageBox.warning(self, "执行失败", f"文件扫描响应异常: {self._response_error_text(response)}")
            return None
        return data

    def on_sig_filter(self) -> None:
        addr_text = self.sig_verify_addr_input.text().strip()
        file_name = self.sig_file_input.text().strip() or "Signature.txt"
        try:
            addr = int(addr_text, 0)
        except ValueError:
            QMessageBox.warning(self, "输入提示", "过滤地址格式无效。")
            return
        response = self._send_operation("signature.filter", {"address": f"0x{addr:X}", "file_name": file_name})
        if response is None:
            return
        data = response.get("data") if self._response_ok(response) else None
        if not isinstance(data, dict):
            QMessageBox.warning(self, "执行失败", f"过滤响应异常: {self._response_error_text(response)}")
            return
        success = bool(data.get("success", False))
        display_data = data
        if success:
            file_data = self._scan_signature_file_data(file_name)
            if isinstance(file_data, dict):
                display_data = file_data
                display_data["success"] = data.get("success", False)
                display_data["changed_count"] = data.get("changed_count", 0)
                display_data["total_count"] = data.get("total_count", 0)
                display_data["old_signature"] = data.get("old_signature", "")
                display_data["new_signature"] = data.get("new_signature", "")
                display_data["file"] = data.get("file", file_name)
        self._render_signature_data(display_data, "特征码状态: 过滤成功" if success else "特征码状态: 过滤失败")
        self._set_status("特征码过滤已完成")

    def on_sig_scan_pattern(self) -> None:
        pattern = self.sig_pattern_input.text().strip()
        if not pattern:
            QMessageBox.warning(self, "输入提示", "请输入特征码。")
            return
        range_text = self.sig_pattern_range_input.text().strip()
        try:
            range_offset = int(range_text, 10)
        except ValueError:
            QMessageBox.warning(self, "输入提示", "偏移必须是整数。")
            return
        response = self._send_operation("signature.scan_pattern", {"range_offset": range_offset, "pattern": pattern})
        if response is None:
            return
        data = response.get("data") if self._response_ok(response) else None
        if not isinstance(data, dict):
            QMessageBox.warning(self, "执行失败", f"特征码扫描响应异常: {self._response_error_text(response)}")
            return
        self._render_signature_data(data, "特征码状态: 按特征码扫描完成")
        self._set_status("按特征码扫描已完成")

    def closeEvent(self, event) -> None:  # type: ignore[override]
        self.live_refresh_timer.stop()
        self._disconnect_device()
        super().closeEvent(event)


def main() -> int:
    app = QApplication(sys.argv)
    window = TcpTestWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
