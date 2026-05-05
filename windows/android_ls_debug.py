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
BROWSER_WINDOW_BYTES = 100
BROWSER_DISASM_CACHE_LINES = BROWSER_WINDOW_BYTES // 4
VALUE_TYPE_OPTIONS = (
    ("I8", "I8"),
    ("I16", "I16"),
    ("I32", "I32"),
    ("I64", "I64"),
    ("Float", "Float"),
    ("Double", "Double"),
)
HWBP_BASE_FIELDS = ("pc", "hit_count", "lr", "sp", "pstate", "orig_x0", "syscallno", "fpsr", "fpcr")
HWBP_OP_LABELS = {
    "none": "未设置",
    "read": "读取",
    "write": "写入",
    "0": "未设置",
    "1": "读取",
    "2": "写入",
}
HWBP_MAX_REG_COUNT = 71
HWBP_REG_INDEX = {
    "pc": 0,
    "hit_count": 1,
    "lr": 2,
    "sp": 3,
    "orig_x0": 4,
    "syscallno": 5,
    "pstate": 6,
    "fpsr": 37,
    "fpcr": 38,
}
HWBP_X0_REG_INDEX = 7
HWBP_Q0_REG_INDEX = 39


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
        self.browser_cache_base = 0
        self.browser_cache_data = b""
        self.browser_current_addr = 0
        self.hwbp_info_data: dict | None = None
        self.hwbp_active = False
        self.hwbp_selected_index: int | None = None
        self.hwbp_editing_index: int | None = None
        self.hwbp_edit_original: dict[str, str] = {}
        self.hwbp_edit_values: dict[str, str] = {}
        self.hwbp_edit_dirty_fields: set[str] = set()
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
        return

    def _create_card(self, object_name: str) -> QFrame:
        card = QFrame(self)
        card.setObjectName(object_name)
        return card

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
            self.device_combo.clear()
            if host:
                self.device_combo.addItem(host, host)
                self.device_combo.setCurrentIndex(0)
            self.port_input.setText(port_input.text().strip())
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
        self.browser_size_label = QLabel(f"{BROWSER_WINDOW_BYTES} 字节缓存")
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
        self.browser_read_button.clicked.connect(self.on_browser_read)
        row1.addWidget(self.browser_read_button)
        self.browser_refresh_button = QPushButton("刷新缓存")
        self.browser_refresh_button.clicked.connect(self.on_browser_refresh_cache)
        row1.addWidget(self.browser_refresh_button)
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
        self.hwbp_type_combo.addItem("BP_READ", "1")
        self.hwbp_type_combo.addItem("BP_WRITE", "2")
        self.hwbp_type_combo.addItem("BP_READ_WRITE", "3")
        self.hwbp_type_combo.addItem("BP_EXECUTE", "4")
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
        self.hwbp_set_button.clicked.connect(self.on_hwbp_set)
        action_row.addWidget(self.hwbp_set_button)

        self.hwbp_remove_button = QPushButton("移除断点")
        self.hwbp_remove_button.clicked.connect(self.on_hwbp_remove_all)
        action_row.addWidget(self.hwbp_remove_button)
        action_row.addStretch(1)
        config_layout.addLayout(action_row)
        self._apply_hwbp_active_state()

        result_card, result_layout = self._create_section_card("断点记录", parent=self.breakpoint_page)
        layout.addWidget(result_card, 1)

        self.hwbp_tree = QTreeWidget()
        self.hwbp_tree.setHeaderLabels(["断点记录（按 PC 折叠）"])
        self.hwbp_tree.setUniformRowHeights(True)
        self.hwbp_tree.setAlternatingRowColors(True)
        self.hwbp_tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self.hwbp_tree.customContextMenuRequested.connect(self.on_hwbp_tree_context_menu)
        self.hwbp_tree.currentItemChanged.connect(self.on_hwbp_tree_current_item_changed)
        self.hwbp_tree.itemDoubleClicked.connect(self.on_hwbp_tree_item_double_clicked)
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
        self.hwbp_active = False
        self.hwbp_selected_index = None
        self._clear_hwbp_edit_state()
        self.global_pid_label.setText("--")
        self.pointer_status_label.setText("扫描状态: 未连接")
        self.hwbp_num_brps_label.setText("hwbp_info.num_brps: 0")
        self.hwbp_num_wrps_label.setText("hwbp_info.num_wrps: 0")
        self.hwbp_hit_addr_label.setText("hwbp_info.hit_addr: 0x0")
        self.browser_cache_base = 0
        self.browser_cache_data = b""
        self.browser_current_addr = 0
        self.hwbp_tree.clear()
        self._apply_hwbp_active_state()
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
                "error": str(exc),
                "data": None,
            }

        if log_enabled:
            log_summary = {
                "ok": bool(response_payload.get("ok", False)),
                "operation": response_payload.get("operation", operation_name),
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
        return "未知错误"

    @staticmethod
    def _response_data_dict(response: dict | None) -> dict:
        if not isinstance(response, dict) or not bool(response.get("ok", False)):
            return {}
        data = response.get("data")
        return data if isinstance(data, dict) else {}

    def _request_ok(
        self,
        operation: str,
        params: dict | None = None,
        *,
        error_title: str,
        error_prefix: str = "",
        error_message: str | None = None,
        status_on_error: str = "",
        log_enabled: bool = True,
        warn: bool = True,
    ) -> dict | None:
        response = self._send_operation(operation, params, log_enabled=log_enabled)
        if response is None:
            return None
        if self._response_ok(response):
            return response
        if warn:
            if error_message is not None:
                warning_text = error_message
            else:
                warning_text = f"{error_prefix}{self._response_error_text(response)}"
            QMessageBox.warning(self, error_title, warning_text)
        if status_on_error:
            self._set_status(status_on_error)
        return None

    def _request_data_dict(
        self,
        operation: str,
        params: dict | None = None,
        *,
        error_title: str,
        error_prefix: str = "",
        error_message: str | None = None,
        parse_title: str = "解析失败",
        parse_error_text: str = "响应格式异常。",
        status_on_error: str = "",
        log_enabled: bool = True,
        warn: bool = True,
    ) -> dict | None:
        response = self._request_ok(
            operation,
            params,
            error_title=error_title,
            error_prefix=error_prefix,
            error_message=error_message,
            status_on_error=status_on_error,
            log_enabled=log_enabled,
            warn=warn,
        )
        if response is None:
            return None
        data = response.get("data")
        if isinstance(data, dict):
            return data
        if warn:
            QMessageBox.warning(self, parse_title, parse_error_text)
        if status_on_error:
            self._set_status(status_on_error)
        return None

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
    def _make_hwbp_field_item(index: int, field_name: str, value: int | str, text: str) -> QTreeWidgetItem:
        item = QTreeWidgetItem([text])
        item.setData(0, Qt.UserRole, index)
        item.setData(0, Qt.UserRole + 2, field_name)
        if isinstance(value, str):
            item.setData(0, Qt.UserRole + 3, value)
        else:
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

    def _hwbp_record_index_exists(self, index: int) -> bool:
        if index < 0 or not isinstance(self.hwbp_info_data, dict):
            return False
        records_raw = self.hwbp_info_data.get("records")
        records = records_raw if isinstance(records_raw, list) else []
        return any(
            isinstance(record, dict) and self._safe_int(record.get("index"), -1) == index
            for record in records
        )

    def _extract_single_hwbp_index_from_group(self, item: QTreeWidgetItem | None) -> int | None:
        group_pc = self._extract_hwbp_group_pc_from_tree_item(item)
        group_payload = self._build_hwbp_group_payload(group_pc if group_pc is not None else -1)
        if group_payload is None:
            return None

        records_raw = group_payload.get("records")
        records = records_raw if isinstance(records_raw, list) else []
        indices = {
            self._safe_int(record.get("index"), -1)
            for record in records
            if isinstance(record, dict) and self._safe_int(record.get("index"), -1) >= 0
        }
        if len(indices) != 1:
            return None
        return next(iter(indices))

    def _get_hwbp_record_by_index(self, index: int) -> dict | None:
        if index < 0 or not isinstance(self.hwbp_info_data, dict):
            return None
        records_raw = self.hwbp_info_data.get("records")
        records = records_raw if isinstance(records_raw, list) else []
        for record in records:
            if isinstance(record, dict) and self._safe_int(record.get("index"), -1) == index:
                return record
        return None

    def _hwbp_record_field_values(self, record: dict) -> dict[str, str]:
        values: dict[str, str] = {}
        pc = self._safe_int(record.get("pc"), 0)
        for field_name in HWBP_BASE_FIELDS:
            if field_name in record:
                values[field_name] = f"0x{self._safe_int(record.get(field_name), 0):X}"
        if "pc" not in values:
            values["pc"] = f"0x{pc:X}"

        regs = self._hwbp_xregs(record)
        for reg_idx, reg_val in enumerate(regs[:30]):
            values[f"x{reg_idx}"] = f"0x{self._safe_int(reg_val, 0):X}"

        qregs = self._hwbp_qregs(record)
        for reg_idx, qreg_val in enumerate(qregs[:32]):
            hi, lo = self._hwbp_qreg_parts(qreg_val)
            values[f"q{reg_idx}"] = f"0x{hi:016X}{lo:016X}"
        return values

    @staticmethod
    def _parse_hwbp_hex_value(value_text: str, field_name: str) -> int | None:
        text = value_text.strip().replace("_", "").replace(" ", "")
        if not text:
            return None
        if text.lower().startswith("0x"):
            text = text[2:]
        if not text or not re.fullmatch(r"[0-9A-Fa-f]+", text):
            return None
        if field_name.lower().startswith("q") and len(text) > 32:
            text = text[-32:]
        return int(text, 16)

    @staticmethod
    def _format_hwbp_edit_value(field_name: str, value: int) -> str:
        lower_field = field_name.lower()
        if lower_field.startswith("q"):
            return f"0x{value & ((1 << 128) - 1):032X}"
        if lower_field in {"fpsr", "fpcr"}:
            return f"0x{value & 0xFFFFFFFF:X}"
        return f"0x{value & ((1 << 64) - 1):X}"

    def _apply_hwbp_edit_state(self) -> None:
        return

    def _clear_hwbp_edit_state(self) -> None:
        self.hwbp_editing_index = None
        self.hwbp_edit_original = {}
        self.hwbp_edit_values = {}
        self.hwbp_edit_dirty_fields = set()
        self._apply_hwbp_edit_state()

    def _begin_hwbp_edit_for_index(self, index: int) -> bool:
        record = self._get_hwbp_record_by_index(index)
        if record is None:
            return False
        values = self._hwbp_record_field_values(record)
        self.hwbp_editing_index = index
        self.hwbp_edit_original = dict(values)
        self.hwbp_edit_values = dict(values)
        self.hwbp_edit_dirty_fields = set()
        self.hwbp_selected_index = index
        self._apply_hwbp_edit_state()
        return True

    def _hwbp_effective_value_text(self, index: int, field_name: str, current_text: str) -> str:
        if self.hwbp_editing_index == index and field_name in self.hwbp_edit_values:
            return self.hwbp_edit_values[field_name]
        return current_text

    def _hwbp_value_display_text(self, index: int, field_name: str, current_text: str) -> str:
        if self.hwbp_editing_index == index and field_name in self.hwbp_edit_dirty_fields:
            new_text = self.hwbp_edit_values.get(field_name, current_text)
            return f"{current_text} -> {new_text}  [待应用]"
        return current_text

    def _refresh_hwbp_tree_from_cache(self) -> None:
        if not isinstance(self.hwbp_info_data, dict):
            return
        records_raw = self.hwbp_info_data.get("records")
        records = records_raw if isinstance(records_raw, list) else []
        self._render_hwbp_tree(records)

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
            response = self._request_ok(
                "breakpoint.record.remove",
                {"index": idx},
                error_title="删除失败",
                warn=False,
            )
            if response is not None:
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
        current_item = self.hwbp_tree.currentItem()
        index = self._extract_hwbp_index_from_tree_item(current_item)
        if index is None:
            index = self._extract_single_hwbp_index_from_group(current_item)
        if index is not None:
            self.hwbp_selected_index = index
            return index
        if self.hwbp_selected_index is not None and self._hwbp_record_index_exists(self.hwbp_selected_index):
            return self.hwbp_selected_index
        return None

    @staticmethod
    def _hwbp_xregs(record: dict) -> list[object]:
        return [record[f"x{reg_idx}"] for reg_idx in range(30) if f"x{reg_idx}" in record]

    @staticmethod
    def _hwbp_qregs(record: dict) -> list[object]:
        return [record[f"q{reg_idx}"] for reg_idx in range(32) if f"q{reg_idx}" in record]

    @staticmethod
    def _hwbp_reg_index(field_name: str) -> int | None:
        field = field_name.strip().lower()
        if field in HWBP_REG_INDEX:
            return HWBP_REG_INDEX[field]
        match = re.fullmatch(r"x(\d+)", field)
        if match:
            reg_idx = int(match.group(1), 10)
            if 0 <= reg_idx < 30:
                return HWBP_X0_REG_INDEX + reg_idx
        match = re.fullmatch(r"q(\d+)", field)
        if match:
            reg_idx = int(match.group(1), 10)
            if 0 <= reg_idx < 32:
                return HWBP_Q0_REG_INDEX + reg_idx
        return None

    @staticmethod
    def _hwbp_mask_op_at_index(rec: dict, reg_idx: int) -> int | None:
        if reg_idx < 0 or reg_idx >= HWBP_MAX_REG_COUNT:
            return None
        mask_raw = rec.get("mask")
        if not isinstance(mask_raw, list):
            return None
        byte_idx = reg_idx >> 2
        if byte_idx < 0 or byte_idx >= len(mask_raw):
            return None
        byte_value = TcpTestWindow._safe_int(mask_raw[byte_idx], 0) & 0xFF
        bit_offset = (reg_idx & 0x3) << 1
        return (byte_value >> bit_offset) & 0x3

    @staticmethod
    def _hwbp_mask_op_value(rec: dict, field_name: str) -> int | None:
        reg_idx = TcpTestWindow._hwbp_reg_index(field_name)
        if reg_idx is None:
            return None
        return TcpTestWindow._hwbp_mask_op_at_index(rec, reg_idx)

    @staticmethod
    def _hwbp_mask_counts(rec: dict) -> tuple[int, int]:
        read_count = 0
        write_count = 0
        for reg_idx in range(HWBP_MAX_REG_COUNT):
            op_value = TcpTestWindow._hwbp_mask_op_at_index(rec, reg_idx)
            if op_value == 1:
                read_count += 1
            elif op_value == 2:
                write_count += 1
        return read_count, write_count

    def _hwbp_reg_op(self, rec: dict, field_name: str) -> str:
        op_value = self._hwbp_mask_op_value(rec, field_name)
        if op_value is not None:
            return HWBP_OP_LABELS.get(str(op_value), str(op_value))
        return "未设置"

    def _hwbp_ops_summary(self, rec: dict) -> str:
        read_count, write_count = self._hwbp_mask_counts(rec)
        if write_count or read_count:
            return f"读 {read_count} / 写 {write_count}"
        return "未设置"

    @staticmethod
    def _hwbp_qreg_parts(qreg: object) -> tuple[int, int]:
        if isinstance(qreg, dict):
            return (
                TcpTestWindow._safe_int(qreg.get("hi"), 0),
                TcpTestWindow._safe_int(qreg.get("lo"), 0),
            )
        value = TcpTestWindow._safe_int(qreg, 0)
        return ((value >> 64) & ((1 << 64) - 1), value & ((1 << 64) - 1))

    def _decode_hwbp_rw_text(self, rec: dict) -> str:
        read_count, write_count = self._hwbp_mask_counts(rec)
        if write_count and read_count:
            return "读/写"
        if write_count:
            return "写入"
        if read_count:
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
                ops_summary = self._hwbp_ops_summary(rec)
                summary_item = QTreeWidgetItem([f"[{idx}] 命中 {hit_count} 次  |  类型 {rw_text}  |  掩码 {ops_summary}"])
                summary_item.setData(0, Qt.UserRole, idx)
                top.addChild(summary_item)

                lr = self._safe_int(rec.get("lr"), 0)
                sp = self._safe_int(rec.get("sp"), 0)
                orig_x0 = self._safe_int(rec.get("orig_x0"), 0)
                syscallno = self._safe_int(rec.get("syscallno"), 0)
                pstate = self._safe_int(rec.get("pstate"), 0)
                fpsr = self._safe_int(rec.get("fpsr"), 0)
                fpcr = self._safe_int(rec.get("fpcr"), 0)
                base_fields = (
                    ("pc", "PC", f"0x{pc:X}"),
                    ("lr", "LR", f"0x{lr:X}"),
                    ("sp", "SP", f"0x{sp:X}"),
                    ("orig_x0", "ORIG_X0", f"0x{orig_x0:X}"),
                    ("syscallno", "SYSCALLNO", f"0x{syscallno:X}"),
                    ("pstate", "PSTATE", f"0x{pstate:X}"),
                    ("fpsr", "FPSR", f"0x{fpsr:X}"),
                    ("fpcr", "FPCR", f"0x{fpcr:X}"),
                )
                for field_name, label, value_text in base_fields:
                    effective_text = self._hwbp_effective_value_text(idx, field_name, value_text)
                    display_text = self._hwbp_value_display_text(idx, field_name, value_text)
                    top.addChild(
                        self._make_hwbp_field_item(
                            idx,
                            field_name,
                            effective_text,
                            f"  {label}: {display_text}  [{self._hwbp_reg_op(rec, field_name)}]",
                        )
                    )

                mask_raw = rec.get("mask")
                if isinstance(mask_raw, list) and mask_raw:
                    mask_text = " ".join(f"{self._safe_int(byte, 0) & 0xFF:02X}" for byte in mask_raw[:18])
                    mask_item = QTreeWidgetItem([f"  MASK: {mask_text}"])
                    mask_item.setData(0, Qt.UserRole, idx)
                    top.addChild(mask_item)

                regs = self._hwbp_xregs(rec)
                regs_title = QTreeWidgetItem(["  寄存器快照 X0~X29"])
                regs_title.setData(0, Qt.UserRole, idx)
                top.addChild(regs_title)
                for reg_idx, reg_val in enumerate(regs):
                    reg_hex = self._safe_int(reg_val, 0)
                    field_name = f"x{reg_idx}"
                    value_text = f"0x{reg_hex:X}"
                    effective_text = self._hwbp_effective_value_text(idx, field_name, value_text)
                    display_text = self._hwbp_value_display_text(idx, field_name, value_text)
                    top.addChild(self._make_hwbp_field_item(idx, field_name, effective_text, f"    X{reg_idx}: {display_text}  [{self._hwbp_reg_op(rec, field_name)}]"))

                qregs = self._hwbp_qregs(rec)
                if qregs:
                    qregs_title = QTreeWidgetItem(["  SIMD 寄存器快照 Q0~Q31"])
                    qregs_title.setData(0, Qt.UserRole, idx)
                    top.addChild(qregs_title)
                    for reg_idx, qreg_val in enumerate(qregs):
                        hi, lo = self._hwbp_qreg_parts(qreg_val)
                        field_name = f"q{reg_idx}"
                        qreg_hex = f"0x{hi:016X}{lo:016X}"
                        qreg_display = f"0x{hi:016X}_{lo:016X}"
                        effective_text = self._hwbp_effective_value_text(idx, field_name, qreg_hex)
                        display_text = self._hwbp_value_display_text(idx, field_name, qreg_display)
                        top.addChild(self._make_hwbp_field_item(idx, field_name, effective_text, f"    Q{reg_idx}: {display_text}  [{self._hwbp_reg_op(rec, field_name)}]"))

                if self._hwbp_mask_counts(rec)[1] > 0:
                    write_title = QTreeWidgetItem(["  写入寄存器候选"])
                    write_title.setData(0, Qt.UserRole, idx)
                    top.addChild(write_title)
                    x0_val = self._safe_int(regs[0], 0) if len(regs) > 0 else 0
                    x1_val = self._safe_int(regs[1], 0) if len(regs) > 1 else 0
                    x0_text = f"0x{x0_val:X}"
                    x1_text = f"0x{x1_val:X}"
                    top.addChild(self._make_hwbp_field_item(idx, "x0", self._hwbp_effective_value_text(idx, "x0", x0_text), f"    候选写入值(X0): {self._hwbp_value_display_text(idx, 'x0', x0_text)}"))
                    top.addChild(self._make_hwbp_field_item(idx, "x1", self._hwbp_effective_value_text(idx, "x1", x1_text), f"    候选写入地址(X1): {self._hwbp_value_display_text(idx, 'x1', x1_text)}"))

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
        self.hwbp_active = bool(info.get("active", self.hwbp_active))
        active_addr = self._safe_int(info.get("active_address"), 0)
        self.hwbp_num_brps_label.setText(f"hwbp_info.num_brps: {num_brps}")
        self.hwbp_num_wrps_label.setText(f"hwbp_info.num_wrps: {num_wrps}")
        if self.hwbp_active:
            self.hwbp_hit_addr_label.setText(f"hwbp_info.hit_addr: 0x{hit_addr:X}  active: 0x{active_addr:X}")
        else:
            self.hwbp_hit_addr_label.setText(f"hwbp_info.hit_addr: 0x{hit_addr:X}  active: false")
        self._apply_hwbp_active_state()
        records_raw = info.get("records")
        records = records_raw if isinstance(records_raw, list) else []
        if self.hwbp_selected_index is not None and not self._hwbp_record_index_exists(self.hwbp_selected_index):
            self.hwbp_selected_index = None
        if self.hwbp_editing_index is not None and not self._hwbp_record_index_exists(self.hwbp_editing_index):
            self._clear_hwbp_edit_state()
        self._render_hwbp_tree(records)

    def _apply_hwbp_active_state(self) -> None:
        if hasattr(self, "hwbp_set_button"):
            self.hwbp_set_button.setEnabled(not self.hwbp_active)
        if hasattr(self, "hwbp_remove_button"):
            self.hwbp_remove_button.setEnabled(self.hwbp_active)

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
        data = TcpTestWindow._response_data_dict(response)
        if data:
            value = data.get("value")
            if value is not None:
                return str(value)
        return None

    @staticmethod
    def _pack_saved_value_for_type(type_token: str, value_text: str) -> tuple[bytes | None, str]:
        text = value_text.strip()
        if not text:
            return None, "值不能为空。"

        integer_sizes = {
            "I8": 1,
            "I16": 2,
            "I32": 4,
            "I64": 8,
        }
        if type_token in integer_sizes:
            size = integer_sizes[type_token]
            bits = size * 8
            min_value = -(1 << (bits - 1))
            max_value = (1 << bits) - 1
            try:
                value = int(text, 0)
            except ValueError:
                return None, f"{type_token} 需要整数，支持十进制或 0x 十六进制。"
            if value < min_value or value > max_value:
                return None, f"{type_token} 范围为 {min_value} 到 {max_value}。"
            return (value & max_value).to_bytes(size, "little", signed=False), ""

        if type_token == "Float":
            try:
                return struct.pack("<f", float(text)), ""
            except (OverflowError, ValueError):
                return None, "Float 需要有效浮点数。"

        if type_token == "Double":
            try:
                return struct.pack("<d", float(text)), ""
            except (OverflowError, ValueError):
                return None, "Double 需要有效浮点数。"

        return None, f"不支持的类型: {type_token}"

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

    def _set_saved_item_lock_state(self, item: dict[str, str], locked: bool, *, warn: bool) -> bool:
        addr = item.get("addr", "")
        if not addr:
            return False
        if not locked:
            response = self._request_ok(
                "lock.unset",
                {"address": addr},
                error_title="锁定失败",
                error_prefix="取消锁定失败: ",
                warn=warn,
            )
            if response is None:
                return False
            item["locked"] = "0"
            return True

        type_token = item.get("type", "")
        if not type_token:
            return False
        if not item.get("value", "") and not self._ensure_saved_item_value(item):
            if warn:
                QMessageBox.warning(self, "锁定失败", f"锁定前读取当前值失败: {addr}")
            return False

        response = self._request_ok(
            "lock.set",
            {"address": addr, "value_type": type_token, "value": item.get("value", "")},
            error_title="锁定失败",
            error_prefix="锁定失败: ",
            warn=warn,
        )
        if response is None:
            return False
        item["locked"] = "1"
        return True

    def _write_saved_item_value(self, item: dict[str, str]) -> bool:
        addr = item.get("addr", "")
        type_token = item.get("type", "")
        if not addr or not type_token:
            return False

        value_text, accepted = QInputDialog.getText(
            self,
            "改写值",
            f"写入地址 {addr} ({type_token})：",
            QLineEdit.Normal,
            item.get("value", ""),
        )
        if not accepted:
            return False

        packed_value, error_text = self._pack_saved_value_for_type(type_token, value_text)
        if packed_value is None:
            QMessageBox.warning(self, "写入失败", error_text)
            return False

        was_locked = item.get("locked", "0") == "1"
        if was_locked and not self._set_saved_item_lock_state(item, False, warn=True):
            return False

        response = self._request_ok(
            "memory.write_block",
            {"address": addr, "data_hex": packed_value.hex().upper()},
            error_title="写入失败",
            error_prefix="写入失败: ",
            warn=True,
        )
        if response is None:
            if was_locked:
                self._set_saved_item_lock_state(item, True, warn=False)
            return False

        item["value"] = value_text.strip()
        readback_value = self._read_saved_item_value(type_token, addr)
        if readback_value:
            item["value"] = readback_value

        if was_locked and not self._set_saved_item_lock_state(item, True, warn=True):
            return False

        self._set_status(f"已写入: {addr} = {item.get('value', '')}")
        return True

    def _apply_saved_item_lock_state(self, items: list[dict[str, str]], locked: bool) -> tuple[int, int]:
        success_count = 0
        fail_count = 0
        for item in items:
            if (item.get("locked", "0") == "1") == locked:
                continue
            if self._set_saved_item_lock_state(item, locked, warn=False):
                success_count += 1
            else:
                fail_count += 1
        return success_count, fail_count

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
            write_action = menu.addAction("改写值")
            menu.addSeparator()
            # 单选：显示锁定/取消锁定
            locked = items[0].get("locked", "0") == "1"
            lock_action = menu.addAction("取消锁定" if locked else "锁定此项")
        else:
            # 多选：显示批量操作选项
            note_action = None
            clear_note_action = None
            write_action = None
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
            if action == write_action:
                if self._write_saved_item_value(items[0]):
                    self._refresh_saved_view(force=True)
                return
            if action != lock_action:
                return
            item = items[0]
            locked = item.get("locked", "0") == "1"
            if not self._set_saved_item_lock_state(item, not locked, warn=True):
                return
            addr = item.get("addr", "")
            if locked:
                self._set_status(f"已取消锁定: {addr}")
            else:
                self._set_status(f"已锁定: {addr} = {item.get('value', '')}")
        else:
            if action not in actions.values():
                return

            if action == actions.get("lock"):
                success_count, fail_count = self._apply_saved_item_lock_state(items, True)
                self._set_status(f"已锁定 {success_count} 项" + (f"，失败 {fail_count} 项" if fail_count > 0 else ""))

            elif action == actions.get("unlock"):
                success_count, fail_count = self._apply_saved_item_lock_state(items, False)
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
        if page_count > 200:
            if not silent:
                QMessageBox.warning(self, "输入提示", "分页数量不能超过 200。")
            return None
        return page_count

    def _fetch_scan_page(self, start: int, *, silent: bool = False) -> bool:
        page_count = self._get_scan_page_size(silent=silent)
        if page_count is None:
            return False

        type_data = self.scan_type_combo.currentData()
        type_token = str(type_data).strip() if type_data is not None else self.scan_type_combo.currentText().strip()

        data = self._request_data_dict(
            "scan.page",
            {"start": start, "count": page_count, "value_type": type_token},
            error_title="获取失败",
            parse_title="解析失败",
            parse_error_text="扫描结果格式异常。",
            status_on_error="" if silent else "获取扫描结果失败",
            log_enabled=not silent,
            warn=not silent,
        )
        if data is None:
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
        response = self._request_ok(operation, params, error_title="扫描失败", status_on_error="首次扫描失败")
        if response is None:
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
        response = self._request_ok(operation, params, error_title="扫描失败", status_on_error="再次扫描失败")
        if response is None:
            return
        self._set_status("再次扫描已执行")
        self.on_scan_status()
        self.scan_page_start = 0
        self._fetch_scan_page(self.scan_page_start)

    def on_scan_clear(self) -> None:
        response = self._request_ok("scan.clear", error_title="清空失败")
        if response is None:
            return
        self.scan_view.clear()
        self.scan_page_start = 0
        self.scan_total_count = 0
        self.scan_live_refresh_enabled = False
        self.scan_total_label.setText("总结果数: 0")
        self._set_status("扫描结果已清空")

    def on_scan_status(self) -> None:
        data = self._request_data_dict("scan.status", error_title="状态失败")
        if data is None:
            return
        scanning = bool(data.get("scanning", False))
        progress = data.get("progress", 0)
        count = data.get("count", 0)
        self.scan_total_label.setText(f"总结果数: {count}")
        self._set_status(f"扫描状态: scanning={1 if scanning else 0}, progress={progress}, count={count}")

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

    def _browser_cache_contains(self, addr: int) -> bool:
        if not self.browser_cache_data:
            return False
        cache_start = self.browser_cache_base
        cache_end = cache_start + len(self.browser_cache_data)
        return cache_start <= addr and addr + BROWSER_WINDOW_BYTES <= cache_end

    def _render_browser_cached_view(self, addr: int, view_mode: str) -> bool:
        if not self._browser_cache_contains(addr):
            return False

        offset = addr - self.browser_cache_base
        data = self.browser_cache_data[offset : offset + BROWSER_WINDOW_BYTES]
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
        byte_delta = self._browser_scroll_unit(view_mode) * step_count
        next_addr = max(0, current - byte_delta) if delta_y > 0 else current + byte_delta
        if self._render_browser_cached_view(next_addr, view_mode):
            return True
        self._refresh_browser_view(next_addr)
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

        end_idx = min(len(disasm_list), BROWSER_DISASM_CACHE_LINES)

        window_items: list[dict] = []
        for item in disasm_list[:end_idx]:
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
        return self._request_data_dict(
            "viewer.snapshot",
            error_title="读取失败",
            error_prefix="内存浏览读取失败: ",
            parse_title="解析失败",
            parse_error_text="浏览器数据格式异常。",
            log_enabled=False,
        )

    def _open_viewer_snapshot(self, addr: int, mode_token: str) -> dict | None:
        open_resp = self._request_ok(
            "viewer.open",
            {"address": f"0x{addr:X}", "view_format": mode_token},
            error_title="读取失败",
            error_prefix="打开浏览器失败: ",
            log_enabled=False,
        )
        if open_resp is None:
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
        data = self._hex_to_bytes(str(snapshot.get("data_hex", "")))
        if data is not None:
            self.browser_cache_base = base_addr
            self.browser_cache_data = data[:BROWSER_WINDOW_BYTES]
        visible_addr = self._extract_disasm_window(snapshot)[1]
        self.browser_current_addr = visible_addr
        self.browser_addr_input.setText(f"0x{visible_addr:X}")
        self.browser_view.setPlainText(self._render_disasm_dump(snapshot))

    def _move_disasm_view(self, lines: int) -> None:
        move_resp = self._request_ok(
            "viewer.move",
            {"lines": lines},
            error_title="移动失败",
            error_prefix="反汇编移动失败: ",
            log_enabled=False,
        )
        if move_resp is None:
            return

        snapshot = self._read_viewer_snapshot()
        if snapshot is None:
            return
        base_addr = self._safe_int(snapshot.get("base"), 0)
        data = self._hex_to_bytes(str(snapshot.get("data_hex", "")))
        if data is not None:
            self.browser_cache_base = base_addr
            self.browser_cache_data = data[:BROWSER_WINDOW_BYTES]
        visible_addr = self._extract_disasm_window(snapshot)[1]
        self.browser_current_addr = visible_addr
        self.browser_addr_input.setText(f"0x{visible_addr:X}")
        self.browser_view.setPlainText(self._render_disasm_dump(snapshot))

    def _refresh_browser_view(self, addr: int, *, force: bool = False) -> None:
        mode_data = self.browser_view_combo.currentData()
        view_mode = str(mode_data).strip() if mode_data is not None else self._browser_mode_to_token(self.browser_view_combo.currentText().strip())
        if not force and self._render_browser_cached_view(addr, view_mode):
            return
        request_base = max(0, addr)
        snapshot = self._open_viewer_snapshot(request_base, view_mode)
        if not isinstance(snapshot, dict):
            return
        data = self._hex_to_bytes(str(snapshot.get("data_hex", "")))
        if data is None:
            QMessageBox.warning(self, "读取失败", "MemViewer HEX 数据解析失败。")
            return
        base_addr = self._safe_int(snapshot.get("base"), addr)
        data = data[:BROWSER_WINDOW_BYTES]
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
        self._refresh_browser_view(addr)

    def on_browser_refresh_cache(self) -> None:
        addr = self._parse_browser_addr()
        if addr is None:
            return
        self.browser_current_addr = addr
        view_mode = self._current_browser_view_mode()
        if view_mode == "disasm":
            self._refresh_disasm_view(addr)
            return
        self._refresh_browser_view(addr, force=True)

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
        response = self._request_ok(operation, params, error_title="指针扫描失败", status_on_error="指针扫描启动失败")
        if response is None:
            return

        self.pointer_view.append(f"启动操作: {operation} {json.dumps(params, ensure_ascii=False)}")
        self.pointer_scan_running = True
        self._set_status("指针扫描任务已启动")
        self.on_pointer_status()

    def _apply_pointer_status_data(self, data: dict, *, silent: bool = False) -> None:
        scanning = bool(data.get("scanning", False))
        progress = data.get("progress", 0)
        count = data.get("count", 0)
        self.pointer_scan_running = scanning
        status_text = f"扫描状态: scanning={1 if scanning else 0}, progress={progress}, count={count}"
        self.pointer_status_label.setText(status_text)
        if not silent:
            self.pointer_view.append(status_text)
            self._set_status("指针状态已刷新")

    def on_pointer_status(self) -> None:
        data = self._request_data_dict("pointer.status", error_title="状态失败")
        if data is None:
            return
        self._apply_pointer_status_data(data, silent=False)

    def _refresh_pointer_status_live(self) -> None:
        if not (self._is_pointer_tab_active() or self.pointer_scan_running):
            return
        if self.pointer_status_request_inflight:
            return

        self.pointer_status_request_inflight = True
        try:
            data = self._request_data_dict(
                "pointer.status",
                error_title="状态失败",
                log_enabled=False,
                warn=False,
            )
            if data is None:
                return
            self._apply_pointer_status_data(data, silent=True)
        finally:
            self.pointer_status_request_inflight = False

    def on_pointer_merge(self) -> None:
        response = self._request_ok("pointer.merge", error_title="合并失败")
        if response is None:
            return
        self.pointer_view.append("已触发 Pointer.bin 合并任务。")
        self._set_status("已触发合并任务")

    def on_pointer_export(self) -> None:
        response = self._request_ok("pointer.export", error_title="导出失败")
        if response is None:
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
            data = self._request_data_dict(
                "target.pid.get",
                {"package_name": input_text},
                error_title="同步失败",
                error_message="包名获取 PID 失败。",
                parse_title="同步失败",
                parse_error_text="包名获取 PID 失败。",
                status_on_error="同步失败：包名获取 PID 失败",
            )
            pid_value = self._safe_int(data.get("pid"), 0) if isinstance(data, dict) and "pid" in data else None
            if pid_value is None:
                return

        set_response = self._request_ok(
            "target.pid.set",
            {"pid": pid_value},
            error_title="同步失败",
            error_message="设置全局 PID 失败。",
            status_on_error="同步失败：设置全局 PID 失败",
        )
        if set_response is None:
            return

        current_data = self._request_data_dict(
            "target.pid.current",
            error_title="同步失败",
            error_message="同步后读取全局 PID 失败。",
            parse_title="同步失败",
            parse_error_text="同步后读取全局 PID 失败。",
            status_on_error="同步后读取全局 PID 失败",
        )
        current_pid: int | None = None
        if isinstance(current_data, dict) and "pid" in current_data:
            parsed_pid = self._safe_int(current_data.get("pid"), 0)
            current_pid = parsed_pid if parsed_pid > 0 else None
        if current_pid is None:
            self.global_pid_label.setText("--")
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
        data = self._request_data_dict(
            "breakpoint.info",
            error_title="刷新失败",
            parse_title="刷新失败",
            parse_error_text="断点信息响应异常。",
            status_on_error="" if silent else "断点信息刷新失败",
            log_enabled=not silent,
            warn=not silent,
        )
        if data is None:
            return
        self.hwbp_info_data = data
        self._render_hwbp_info(data)
        if not silent:
            self._set_status("断点信息已刷新")

    def on_hwbp_set(self) -> None:
        if self.hwbp_active:
            QMessageBox.information(self, "提示", "断点已激活，请先移除当前断点。")
            return

        addr_text = self.hwbp_addr_input.text().strip()
        len_text = self.hwbp_len_input.text().strip()
        type_data = self.hwbp_type_combo.currentData()
        scope_data = self.hwbp_scope_combo.currentData()
        try:
            addr = int(addr_text, 0)
        except ValueError:
            QMessageBox.warning(self, "输入提示", "断点地址格式无效。")
            return
        try:
            length = int(len_text, 10)
        except ValueError:
            length = 1
        if addr <= 0:
            QMessageBox.warning(self, "输入提示", "断点地址必须大于 0。")
            return
        length = max(1, min(8, length))
        bp_type = str(type_data) if type_data is not None else "1"
        bp_scope = str(scope_data) if scope_data is not None else "0"
        response = self._request_ok(
            "breakpoint.set",
            {"address": f"0x{addr:X}", "bp_type": bp_type, "bp_scope": bp_scope, "length": length},
            error_title="设置失败",
            status_on_error="设置硬件断点失败",
        )
        if response is None:
            return
        self.hwbp_active = True
        self._apply_hwbp_active_state()
        self._set_status("设置硬件断点成功")
        self.on_hwbp_refresh(silent=True)

    def on_hwbp_remove_all(self) -> None:
        if not self.hwbp_active:
            self._set_status("断点未激活，无需移除")
            return

        response = self._request_ok("breakpoint.clear", error_title="移除失败")
        if response is None:
            return
        self.hwbp_active = False
        self._apply_hwbp_active_state()
        self._set_status("已移除进程硬件断点")
        self.on_hwbp_refresh(silent=True)

    def on_hwbp_tree_current_item_changed(self, current: QTreeWidgetItem | None, _previous: QTreeWidgetItem | None) -> None:
        if current is None:
            return
        index = self._extract_hwbp_index_from_tree_item(current)
        if index is None:
            index = self._extract_single_hwbp_index_from_group(current)
        if index is not None:
            self.hwbp_selected_index = index

    def _edit_hwbp_tree_item_value(self, item: QTreeWidgetItem | None) -> None:
        if item is None:
            return
        field_data = item.data(0, Qt.UserRole + 2)
        if field_data is None:
            return
        field_name = str(field_data).strip()
        if not field_name:
            return
        index = self._extract_hwbp_index_from_tree_item(item)
        if index is None:
            QMessageBox.warning(self, "输入提示", "请先在断点树里选择一条 hwbp_record。")
            return
        if index < 0:
            QMessageBox.warning(self, "输入提示", "记录索引不能小于 0。")
            return

        if self.hwbp_editing_index is None:
            if not self._begin_hwbp_edit_for_index(index):
                QMessageBox.warning(self, "输入提示", "选中的 hwbp_record 已不存在，请刷新后重试。")
                return
        elif self.hwbp_editing_index != index:
            QMessageBox.warning(self, "输入提示", f"当前正在编辑 hwbp_record[{self.hwbp_editing_index}]，请先应用或取消。")
            return

        current_value = self.hwbp_edit_values.get(field_name, str(item.data(0, Qt.UserRole + 3) or "0x0"))
        value_text, accepted = QInputDialog.getText(
            self,
            "修改寄存器",
            f"hwbp_record[{index}].{field_name} =",
            QLineEdit.Normal,
            current_value,
        )
        if not accepted:
            return
        value = self._parse_hwbp_hex_value(value_text, field_name)
        if value is None:
            QMessageBox.warning(self, "输入提示", "写入值格式无效，请输入十六进制值。")
            return

        formatted_value = self._format_hwbp_edit_value(field_name, value)
        self.hwbp_edit_values[field_name] = formatted_value
        if formatted_value == self.hwbp_edit_original.get(field_name, ""):
            self.hwbp_edit_dirty_fields.discard(field_name)
        else:
            self.hwbp_edit_dirty_fields.add(field_name)
        self._apply_hwbp_edit_state()
        self._refresh_hwbp_tree_from_cache()
        self._set_status(f"已修改副本 hwbp_record[{index}].{field_name}，右键应用写回")

    def on_hwbp_tree_item_double_clicked(self, item: QTreeWidgetItem, _column: int) -> None:
        self._edit_hwbp_tree_item_value(item)

    def on_hwbp_apply_edit(self) -> None:
        if self.hwbp_editing_index is None:
            return
        if not self.hwbp_edit_dirty_fields:
            self._clear_hwbp_edit_state()
            self._set_status("没有寄存器改动需要应用")
            return

        index = self.hwbp_editing_index
        for field_name in sorted(self.hwbp_edit_dirty_fields):
            value_text = self.hwbp_edit_values.get(field_name, "")
            response = self._request_ok(
                "breakpoint.record.update",
                {"index": index, "field": field_name, "value": value_text},
                error_title="写入失败",
            )
            if response is None:
                return

        changed_count = len(self.hwbp_edit_dirty_fields)
        self._clear_hwbp_edit_state()
        self._set_status(f"已应用 hwbp_record[{index}] 的 {changed_count} 处寄存器改动")
        self.on_hwbp_refresh(silent=True)

    def on_hwbp_cancel_edit(self) -> None:
        if self.hwbp_editing_index is None:
            return
        index = self.hwbp_editing_index
        self._clear_hwbp_edit_state()
        self._set_status(f"已取消编辑 hwbp_record[{index}]")

    def on_hwbp_tree_context_menu(self, pos) -> None:
        item = self.hwbp_tree.itemAt(pos)
        if item is None:
            item = self.hwbp_tree.currentItem()
        if item is not None:
            self.hwbp_tree.setCurrentItem(item)
        item_index = self._extract_hwbp_index_from_tree_item(item)
        if item_index is None:
            item_index = self._extract_single_hwbp_index_from_group(item)
        item_field = str(item.data(0, Qt.UserRole + 2)).strip() if item is not None and item.data(0, Qt.UserRole + 2) is not None else ""
        group_pc = self._extract_hwbp_group_pc_from_tree_item(item)
        group_payload = self._build_hwbp_group_payload(group_pc if group_pc is not None else -1)

        menu = QMenu(self.hwbp_tree)
        edit_value_action = None
        apply_edit_action = None
        cancel_edit_action = None
        if item_field:
            edit_value_action = menu.addAction("修改寄存器值")
        if self.hwbp_editing_index is not None and item_index == self.hwbp_editing_index:
            if self.hwbp_edit_dirty_fields:
                apply_edit_action = menu.addAction(f"应用当前记录修改 ({len(self.hwbp_edit_dirty_fields)} 项)")
            cancel_edit_action = menu.addAction("取消当前记录修改")
        if edit_value_action is not None or apply_edit_action is not None or cancel_edit_action is not None:
            menu.addSeparator()
        copy_json_action = menu.addAction("复制当前折叠完整JSON")
        delete_action = menu.addAction("删除当前折叠页")

        if group_payload is None:
            copy_json_action.setEnabled(False)
            delete_action.setEnabled(False)

        action = menu.exec(self.hwbp_tree.mapToGlobal(pos))
        if action is None:
            return

        if action == edit_value_action:
            self._edit_hwbp_tree_item_value(item)
            return
        if action == apply_edit_action:
            self.on_hwbp_apply_edit()
            return
        if action == cancel_edit_action:
            self.on_hwbp_cancel_edit()
            self._refresh_hwbp_tree_from_cache()
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
        response = self._request_ok(
            "signature.scan_address",
            {"address": f"0x{addr:X}", "range": scan_range, "file_name": file_name},
            error_title="执行失败",
        )
        if response is None:
            self.sig_status_label.setText("特征码状态: 扫描并保存失败")
            return
        self.sig_status_label.setText("特征码状态: 扫描并保存成功")
        self._set_status(f"特征码已保存到 {file_name}")

    def _scan_signature_file_data(self, file_name: str) -> dict | None:
        return self._request_data_dict(
            "signature.scan_file",
            {"file_name": file_name},
            error_title="执行失败",
            parse_title="执行失败",
            parse_error_text="文件扫描响应异常。",
        )

    def on_sig_filter(self) -> None:
        addr_text = self.sig_verify_addr_input.text().strip()
        file_name = self.sig_file_input.text().strip() or "Signature.txt"
        try:
            addr = int(addr_text, 0)
        except ValueError:
            QMessageBox.warning(self, "输入提示", "过滤地址格式无效。")
            return
        data = self._request_data_dict(
            "signature.filter",
            {"address": f"0x{addr:X}", "file_name": file_name},
            error_title="执行失败",
            parse_title="执行失败",
            parse_error_text="过滤响应异常。",
        )
        if data is None:
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
        data = self._request_data_dict(
            "signature.scan_pattern",
            {"range_offset": range_offset, "pattern": pattern},
            error_title="执行失败",
            parse_title="执行失败",
            parse_error_text="特征码扫描响应异常。",
        )
        if data is None:
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
