#!/usr/bin/env python3
import ctypes
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

from http_bridge import (
    AndroidHttpBridge,
    BridgeConnectionError,
    BridgeError,
    DEFAULT_ANDROID_PORT,
    DEFAULT_ANDROID_TIMEOUT_SECONDS,
    discover_lan_devices,
)

from PySide6.QtCore import QPoint, Qt, QTimer, Signal
from PySide6.QtGui import QFontDatabase, QIcon, QMouseEvent, QTextCursor, QWheelEvent
from PySide6.QtWidgets import (
    QApplication,
    QButtonGroup,
    QComboBox,
    QFrame,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QRadioButton,
    QSizePolicy,
    QSplitter,
    QTabWidget,
    QTextEdit,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
    QMenu,
)

BROWSER_WINDOW_BYTES = 100
BROWSER_DISASM_CACHE_LINES = BROWSER_WINDOW_BYTES // 4
APPLICATION_NAME = "LuckyStar"
APPLICATION_ID = "LuckyStar.AndroidDebugTool"


def _resource_path(name: str) -> Path:
    bundle_dir = Path(getattr(sys, "_MEIPASS", CURRENT_DIR))
    return bundle_dir / name


def _set_windows_app_id() -> None:
    if sys.platform != "win32":
        return
    try:
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(APPLICATION_ID)
    except (AttributeError, OSError):
        pass


VALUE_TYPE_OPTIONS = (
    ("I8", "I8"),
    ("I16", "I16"),
    ("I32", "I32"),
    ("I64", "I64"),
    ("Float", "Float"),
    ("Double", "Double"),
)
SCAN_HISTORY_MODES = {"inc", "dec", "changed", "unchanged"}
SCAN_VALUE_MODES = {"eq", "gt", "lt", "range", "pointer", "string"}
SCAN_MODE_ALIASES = {
    "equal": "eq",
    "greater": "gt",
    "less": "lt",
    "increased": "inc",
    "decreased": "dec",
}
HWBP_OP_LABELS = {
    "none": "未设置",
    "read": "读取",
    "write": "写入",
    "0": "未设置",
    "1": "读取",
    "2": "写入",
}
HWBP_POINT_TYPE_LABELS = {
    0: "未设置",
    1: "读取",
    2: "写入",
    3: "读写",
    4: "执行",
}
HWBP_SCOPE_LABELS = {
    0: "主线程",
    1: "子线程",
    2: "全部",
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


class HttpBridgeWindow(QWidget):
    scan_lan_finished = Signal(object, str, object)
    live_refresh_finished = Signal(object)

    def __init__(self) -> None:
        super().__init__()
        self.http_bridge = AndroidHttpBridge(timeout_seconds=DEFAULT_ANDROID_TIMEOUT_SECONDS)
        self.is_scanning = False
        self.memory_info_data: dict | None = None
        self.scan_page_start = 0
        self.scan_total_count = 0
        self.scan_live_refresh_enabled = False
        self.scan_session_type: str | None = None
        self.pointer_scan_running = False
        self.saved_items: list[dict[str, str]] = []
        self.browser_current_addr = 0
        self.bp_info_data: dict | None = None
        self.breakpoint_mode = ""
        self.syscall_active = False
        self.hwbp_selected_index: int | None = None
        self.hwbp_point_rows: list[dict[str, object]] = []
        self.live_refresh_inflight = False
        self.live_refresh_closing = False
        self.saved_refresh_cursor = 0
        self.target_generation = 0
        self.live_refresh_timer = QTimer(self)
        self.live_refresh_timer.setInterval(1000)
        self.live_refresh_timer.timeout.connect(self.on_live_refresh_tick)
        self.setWindowTitle(APPLICATION_NAME)
        self.setWindowIcon(QIcon(str(_resource_path("icon.png"))))
        self.resize(1140, 760)
        self.setMinimumSize(980, 680)
        self.scan_lan_finished.connect(self._on_scan_lan_finished)
        self.live_refresh_finished.connect(self._apply_live_refresh_result)
        self._setup_ui()
        self.live_refresh_timer.start()

    def _create_card(self, object_name: str) -> QFrame:
        card = QFrame(self)
        card.setObjectName(object_name)
        return card

    def _create_page_layout(self, page: QWidget) -> QVBoxLayout:
        layout = QVBoxLayout(page)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(10)
        return layout

    def _create_section_card(
        self,
        title_text: str | None = None,
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

        return card, layout

    @staticmethod
    def _configure_data_view(editor: QTextEdit) -> None:
        font = QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
        font.setPointSize(10)
        editor.setFont(font)
        editor.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        editor.setTabStopDistance(editor.fontMetrics().horizontalAdvance(" ") * 4)

    def _update_connection_badge(self, connected: bool) -> None:
        if not hasattr(self, "connection_badge"):
            return
        self.connection_badge.setText("通信成功" if connected else "未通信")

    def _build_header_panel(self, root: QVBoxLayout) -> None:
        hero_card = self._create_card("heroCard")
        hero_layout = QVBoxLayout(hero_card)
        hero_layout.setContentsMargins(10, 6, 10, 6)
        hero_layout.setSpacing(6)

        status_row = QHBoxLayout()
        status_row.setContentsMargins(0, 0, 0, 0)
        status_row.setSpacing(8)

        self.connection_badge = QLabel("未通信")
        self.connection_badge.setObjectName("connectionBadge")
        self._update_connection_badge(False)
        status_row.addWidget(self.connection_badge)

        self.global_pid_label = QLabel("--")
        self.global_pid_label.setObjectName("metricValue")
        self.global_pid_label.setMinimumWidth(50)
        pid_prefix = QLabel("PID")
        pid_prefix.setObjectName("metricTitle")
        status_row.addWidget(pid_prefix)
        status_row.addWidget(self.global_pid_label)

        self.status_label = QLabel("客户端已启动")
        self.status_label.setObjectName("statusText")
        self.status_label.setWordWrap(False)
        self.status_label.setMinimumWidth(180)
        self.status_label.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)

        status_prefix = QLabel("状态")
        status_prefix.setObjectName("metricTitle")
        status_row.addWidget(status_prefix)
        status_row.addWidget(self.status_label, 1)

        connection_row = QHBoxLayout()
        connection_row.setContentsMargins(0, 0, 0, 0)
        connection_row.setSpacing(8)

        self.scan_device_button = QPushButton("扫描设备")
        self.scan_device_button.clicked.connect(self.on_scan_lan_devices)
        connection_row.addWidget(self.scan_device_button)

        self.device_combo = QComboBox()
        self.device_combo.setEditable(True)
        self.device_combo.addItem("", "")
        self.device_combo.setCurrentText("")
        if self.device_combo.lineEdit() is not None:
            self.device_combo.lineEdit().setPlaceholderText("设备 IP 或 https://xxxx.trycloudflare.com")
            self.device_combo.lineEdit().returnPressed.connect(self.on_test_communication)
        self.device_combo.setMinimumWidth(320)
        self.device_combo.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        connection_row.addWidget(self.device_combo, 1)

        port_label = QLabel("端口")
        port_label.setObjectName("metricTitle")
        connection_row.addWidget(port_label)

        self.port_input = QLineEdit(str(DEFAULT_ANDROID_PORT))
        self.port_input.setPlaceholderText("端口")
        self.port_input.setFixedWidth(78)
        connection_row.addWidget(self.port_input)

        self.test_button = QPushButton("测试通信")
        self.test_button.clicked.connect(self.on_test_communication)
        connection_row.addWidget(self.test_button)

        pid_input_prefix = QLabel("PID / 包名")
        pid_input_prefix.setObjectName("metricTitle")
        connection_row.addWidget(pid_input_prefix)

        self.pid_input = QLineEdit()
        self.pid_input.setPlaceholderText("例如 12345 或 me.hd.ggtutorial")
        self.pid_input.setMinimumWidth(260)
        self.pid_input.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self.pid_input.returnPressed.connect(self.on_sync_pid)
        connection_row.addWidget(self.pid_input, 1)

        self.sync_pid_button = QPushButton("同步 PID")
        self.sync_pid_button.clicked.connect(self.on_sync_pid)
        connection_row.addWidget(self.sync_pid_button)

        hero_layout.addLayout(connection_row)
        hero_layout.addLayout(status_row)

        root.addWidget(hero_card)

    def _setup_ui(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(12, 10, 12, 10)
        root.setSpacing(8)

        self._build_header_panel(root)

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
        self.syscall_page = QWidget()
        self.environment_page = QWidget()
        self.signature_page = QWidget()
        self.save_page = QWidget()
        self.log_page = QWidget()

        self.tabs.addTab(self.memory_page, "内存信息页")
        self.tabs.addTab(self.search_page, "扫描页")
        self.tabs.addTab(self.save_page, "保存页")
        self.tabs.addTab(self.browser_page, "内存浏览页")
        self.tabs.addTab(self.pointer_page, "指针页")
        self.tabs.addTab(self.breakpoint_page, "断点页")
        self.tabs.addTab(self.syscall_page, "系统调用页")
        self.tabs.addTab(self.environment_page, "环境参数页")
        self.tabs.addTab(self.signature_page, "特征码页")
        self.tabs.addTab(self.log_page, "日志页")
        self.tabs.currentChanged.connect(self.on_tab_changed)

        self._build_memory_page()
        self._build_scan_page()
        self._build_browser_page()
        self._build_pointer_page()
        self._build_breakpoint_page()
        self._build_syscall_page()
        self._build_environment_page()
        self._build_signature_page()
        self._build_save_page()
        self._build_log_page()

        self._log("客户端已启动。")
        self._set_connection_ui(False)

    def _is_pointer_tab_active(self) -> bool:
        return self.tabs.currentWidget() is self.pointer_page

    def _is_scan_tab_active(self) -> bool:
        return self.tabs.currentWidget() is self.search_page

    def _is_save_tab_active(self) -> bool:
        return self.tabs.currentWidget() is self.save_page

    def _is_breakpoint_tab_active(self) -> bool:
        return self.tabs.currentWidget() is self.breakpoint_page

    def _is_syscall_tab_active(self) -> bool:
        return self.tabs.currentWidget() is self.syscall_page

    def on_tab_changed(self, _index: int) -> None:
        if self._is_connected():
            self.on_live_refresh_tick()

    def _build_memory_page(self) -> None:
        layout = self._create_page_layout(self.memory_page)

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

        dump_row = QHBoxLayout()
        dump_row.setSpacing(10)
        dump_row.addWidget(QLabel("模块名/地址范围"))
        self.memory_dump_input = QLineEdit()
        self.memory_dump_input.setPlaceholderText("例如 unity 或 0x5000-0x6000")
        self.memory_dump_input.returnPressed.connect(self.on_dump_memory)
        dump_row.addWidget(self.memory_dump_input, 1)
        self.dump_memory_button = QPushButton("Dump")
        self.dump_memory_button.clicked.connect(self.on_dump_memory)
        dump_row.addWidget(self.dump_memory_button)
        dump_row.addStretch(1)
        card_layout.addLayout(dump_row)

        self.memory_view = QTextEdit()
        self.memory_view.setReadOnly(True)
        self._configure_data_view(self.memory_view)
        self.memory_view.setPlaceholderText("点击“刷新内存信息”后显示可读的 memory_info 结构数据。")
        card_layout.addWidget(self.memory_view, 1)

    def _build_environment_page(self) -> None:
        layout = self._create_page_layout(self.environment_page)
        card, card_layout = self._create_section_card(parent=self.environment_page)
        layout.addWidget(card, 1)

        row = QHBoxLayout()
        row.setSpacing(10)
        row.addWidget(QLabel("线程名"))
        self.env_thread_input = QLineEdit()
        self.env_thread_input.setPlaceholderText("可选，task->comm，最多 15 字符")
        self.env_thread_input.setMaxLength(15)
        self.env_thread_input.returnPressed.connect(self.on_get_environment_params)
        row.addWidget(self.env_thread_input, 1)
        self.get_env_button = QPushButton("获取环境参数")
        self.get_env_button.clicked.connect(self.on_get_environment_params)
        row.addWidget(self.get_env_button)
        row.addStretch(1)
        card_layout.addLayout(row)

        self.environment_view = QTextEdit()
        self.environment_view.setReadOnly(True)
        self.environment_view.setPlaceholderText("留空线程名可获取 PACGA；填写线程名时同时获取 TPIDR_EL0。")
        card_layout.addWidget(self.environment_view, 1)

    def _build_scan_page(self) -> None:
        layout = self._create_page_layout(self.search_page)

        splitter = QSplitter(Qt.Horizontal)
        layout.addWidget(splitter, 1)

        left_panel, left_layout = self._create_section_card(parent=self.search_page)
        splitter.addWidget(left_panel)

        self.scan_view = QTextEdit()
        self.scan_view.setReadOnly(True)
        self.scan_view.setContextMenuPolicy(Qt.CustomContextMenu)
        self.scan_view.customContextMenuRequested.connect(self.on_scan_view_context_menu)
        left_layout.addWidget(self.scan_view, 1)

        right_panel, right_layout = self._create_section_card("扫描参数", parent=self.search_page)
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
        self.scan_mode_combo.addItem("字符串", "string")
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

        self.scan_mode_combo.currentIndexChanged.connect(self._apply_scan_control_state)
        self._apply_scan_control_state()

    def _scan_mode_token(self) -> str:
        mode_data = self.scan_mode_combo.currentData()
        return str(mode_data).strip().lower() if mode_data is not None else ""

    def _selected_scan_type_token(self) -> str:
        type_data = self.scan_type_combo.currentData()
        return str(type_data).strip() if type_data is not None else self.scan_type_combo.currentText().strip()

    @staticmethod
    def _normalize_scan_type_token(value: object) -> str | None:
        token = str(value or "").strip().lower()
        return {
            "i8": "I8",
            "i16": "I16",
            "i32": "I32",
            "i64": "I64",
            "float": "Float",
            "f32": "Float",
            "double": "Double",
            "f64": "Double",
            "str": "string",
            "string": "string",
            "text": "string",
        }.get(token)

    def _scan_result_type_token(self) -> str:
        return self.scan_session_type or self._selected_scan_type_token()

    def _set_scan_type_combo(self, type_token: str) -> None:
        if type_token == "string":
            return
        index = self.scan_type_combo.findData(type_token)
        if index >= 0 and index != self.scan_type_combo.currentIndex():
            self.scan_type_combo.setCurrentIndex(index)

    def _set_scan_mode_combo(self, mode_token: str) -> None:
        normalized = SCAN_MODE_ALIASES.get(mode_token.strip().lower(), mode_token.strip().lower())
        index = self.scan_mode_combo.findData(normalized)
        if index >= 0 and index != self.scan_mode_combo.currentIndex():
            self.scan_mode_combo.setCurrentIndex(index)

    def _adopt_scan_state(self, data: dict, *, fallback_type: str | None = None, fallback_mode: str = "") -> None:
        state_type = self._normalize_scan_type_token(data.get("value_type"))
        if state_type is None and bool(data.get("string_scan", False)):
            state_type = "string"
        if state_type is None:
            state_type = self._normalize_scan_type_token(fallback_type)
        self.scan_session_type = state_type
        if state_type is not None:
            self._set_scan_type_combo(state_type)

        state_mode = str(data.get("mode") or fallback_mode).strip().lower()
        if state_type == "string":
            state_mode = "string"
        if state_mode:
            self._set_scan_mode_combo(state_mode)
        self._apply_scan_control_state()

    def _reset_scan_session(self) -> None:
        self.scan_session_type = None
        self.scan_live_refresh_enabled = False
        self._apply_scan_control_state()

    def _apply_scan_control_state(self, *_args) -> None:
        has_baseline = self.scan_session_type is not None
        running = self.scan_live_refresh_enabled
        mode = self._scan_mode_token()

        if not has_baseline and mode in SCAN_HISTORY_MODES:
            self._set_scan_mode_combo("eq")
            mode = self._scan_mode_token()
        if self.scan_session_type == "string" and mode != "string":
            self._set_scan_mode_combo("string")
            mode = "string"
        if mode == "pointer":
            self._set_scan_type_combo("I64")

        mode_model = self.scan_mode_combo.model()
        for index in range(self.scan_mode_combo.count()):
            item = mode_model.item(index)
            if item is None:
                continue
            item_mode = str(self.scan_mode_combo.itemData(index)).strip().lower()
            enabled = True
            if item_mode in SCAN_HISTORY_MODES:
                enabled = has_baseline and self.scan_session_type != "string"
            elif item_mode == "pointer":
                enabled = not has_baseline or self.scan_session_type == "I64"
            elif item_mode == "string":
                enabled = not has_baseline or self.scan_session_type == "string"
            item.setEnabled(enabled)

        self.scan_type_combo.setEnabled(not running and not has_baseline and mode not in {"pointer", "string"})
        self.scan_mode_combo.setEnabled(not running and self.scan_session_type != "string")
        self.scan_value_input.setEnabled(not running and mode in SCAN_VALUE_MODES)
        self.scan_range_input.setEnabled(not running and mode == "range")
        self.scan_first_button.setEnabled(not running and not has_baseline and mode not in SCAN_HISTORY_MODES)
        self.scan_next_button.setEnabled(not running and has_baseline)
        self.scan_clear_button.setEnabled(not running and has_baseline)
        self.scan_prev_button.setEnabled(not running and has_baseline)
        self.scan_next_page_button.setEnabled(not running and has_baseline)

        if mode == "string":
            self.scan_value_input.setPlaceholderText("输入要扫描的文本")
        elif mode == "pointer":
            self.scan_value_input.setPlaceholderText("输入十六进制目标地址，例如 7F12345678")
        else:
            self.scan_value_input.setPlaceholderText("例如 100 或 3.14")

    def _build_browser_page(self) -> None:
        layout = self._create_page_layout(self.browser_page)

        toolbar_card, toolbar_layout = self._create_section_card(parent=self.browser_page)
        layout.addWidget(toolbar_card)

        row1 = QHBoxLayout()
        row1.setSpacing(10)
        row1.addWidget(QLabel("地址"))
        self.browser_addr_input = QLineEdit("0x0")
        self.browser_addr_input.setPlaceholderText("输入起始地址，如 0x12345678 或 0x12345678+0xA8")
        self.browser_addr_input.returnPressed.connect(self.on_browser_read)
        row1.addWidget(self.browser_addr_input, 1)

        row1.addWidget(QLabel("快照"))
        self.browser_size_label = QLabel(f"{BROWSER_WINDOW_BYTES} 字节")
        self.browser_size_label.setMinimumWidth(120)
        row1.addWidget(self.browser_size_label)

        row1.addWidget(QLabel("显示"))
        self.browser_view_combo = QComboBox()
        self.browser_view_combo.addItem("Hexadecimal", "hexadecimal")
        self.browser_view_combo.addItem("Hex", "hex")
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
        self.browser_refresh_button = QPushButton("刷新")
        self.browser_refresh_button.clicked.connect(self.on_browser_refresh_cache)
        row1.addWidget(self.browser_refresh_button)
        row1.addStretch(1)
        toolbar_layout.addLayout(row1)

        viewer_card, viewer_layout = self._create_section_card(parent=self.browser_page)
        layout.addWidget(viewer_card, 1)

        self.browser_view = BrowserTextEdit()
        self.browser_view.setReadOnly(True)
        self._configure_data_view(self.browser_view)
        self.browser_view.wheel_navigate_handler = self.on_browser_wheel_navigate
        self.browser_view_combo.currentIndexChanged.connect(self.on_browser_format_changed)
        viewer_layout.addWidget(self.browser_view, 1)

    def _build_pointer_page(self) -> None:
        layout = self._create_page_layout(self.pointer_page)

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

        self.pointer_mode_group = QButtonGroup(self)

        module_row = QHBoxLayout()
        module_row.setSpacing(8)
        self.pointer_module_mode_button = QRadioButton("Module")
        self.pointer_module_mode_button.setChecked(True)
        self.pointer_module_mode_button.toggled.connect(self._update_pointer_mode_inputs)
        self.pointer_mode_group.addButton(self.pointer_module_mode_button)
        module_row.addWidget(self.pointer_module_mode_button)
        module_row.addWidget(QLabel("模块过滤"))
        self.pointer_filter_input = QLineEdit()
        self.pointer_filter_input.setPlaceholderText("可选，例如 libil2cpp.so")
        module_row.addWidget(self.pointer_filter_input, 1)
        config_layout.addLayout(module_row)

        manual_row = QHBoxLayout()
        manual_row.setSpacing(8)
        self.pointer_manual_mode_button = QRadioButton("Manual")
        self.pointer_manual_mode_button.toggled.connect(self._update_pointer_mode_inputs)
        self.pointer_mode_group.addButton(self.pointer_manual_mode_button)
        manual_row.addWidget(self.pointer_manual_mode_button)
        manual_row.addWidget(QLabel("手动基址"))
        self.pointer_manual_base_input = QLineEdit("0x0")
        manual_row.addWidget(self.pointer_manual_base_input, 1)
        config_layout.addLayout(manual_row)

        array_row = QHBoxLayout()
        array_row.setSpacing(8)
        self.pointer_array_mode_button = QRadioButton("Array")
        self.pointer_array_mode_button.toggled.connect(self._update_pointer_mode_inputs)
        self.pointer_mode_group.addButton(self.pointer_array_mode_button)
        array_row.addWidget(self.pointer_array_mode_button)
        array_row.addWidget(QLabel("数组基址"))
        self.pointer_array_base_input = QLineEdit("0x0")
        array_row.addWidget(self.pointer_array_base_input, 1)
        array_row.addWidget(QLabel("数组数量"))
        self.pointer_array_count_input = QLineEdit("128")
        self.pointer_array_count_input.setMaximumWidth(100)
        array_row.addWidget(self.pointer_array_count_input)
        config_layout.addLayout(array_row)
        self._update_pointer_mode_inputs()

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

        result_card, result_layout = self._create_section_card(parent=self.pointer_page)
        layout.addWidget(result_card, 1)

        self.pointer_status_label = QLabel("扫描状态: 未开始")
        result_layout.addWidget(self.pointer_status_label)

        self.pointer_view = QTextEdit()
        self.pointer_view.setReadOnly(True)
        result_layout.addWidget(self.pointer_view, 1)

    def _build_breakpoint_page(self) -> None:
        layout = self._create_page_layout(self.breakpoint_page)

        config_card, config_layout = self._create_section_card("断点配置", parent=self.breakpoint_page)
        layout.addWidget(config_card)

        summary_row = QHBoxLayout()
        summary_row.setSpacing(10)
        self.hwbp_num_brps_label = QLabel("bp_info.num_brps: 0")
        summary_row.addWidget(self.hwbp_num_brps_label)
        self.hwbp_num_wrps_label = QLabel("bp_info.num_wrps: 0")
        summary_row.addWidget(self.hwbp_num_wrps_label)
        self.hwbp_points_label = QLabel("bp_info.points: []")
        self.hwbp_points_label.setWordWrap(True)
        summary_row.addWidget(self.hwbp_points_label, 1)
        config_layout.addLayout(summary_row)

        config_layout.addWidget(QLabel("points"))
        self.hwbp_points_container = QWidget(self.breakpoint_page)
        self.hwbp_points_layout = QVBoxLayout(self.hwbp_points_container)
        self.hwbp_points_layout.setContentsMargins(0, 0, 0, 0)
        self.hwbp_points_layout.setSpacing(6)
        config_layout.addWidget(self.hwbp_points_container)
        self._add_hwbp_point_row()

        point_action_row = QHBoxLayout()
        point_action_row.setSpacing(10)
        self.hwbp_add_point_button = QPushButton("添加point")
        self.hwbp_add_point_button.clicked.connect(self._add_hwbp_point_row)
        point_action_row.addWidget(self.hwbp_add_point_button)
        self.hwbp_remove_point_button = QPushButton("删除point")
        self.hwbp_remove_point_button.clicked.connect(self._remove_hwbp_point_row)
        point_action_row.addWidget(self.hwbp_remove_point_button)
        point_action_row.addStretch(1)
        config_layout.addLayout(point_action_row)

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

        self.ptebp_set_button = QPushButton("设置PTEBP")
        self.ptebp_set_button.clicked.connect(self.on_ptebp_set)
        action_row.addWidget(self.ptebp_set_button)

        self.ptebp_remove_button = QPushButton("移除PTEBP")
        self.ptebp_remove_button.clicked.connect(self.on_ptebp_remove_all)
        action_row.addWidget(self.ptebp_remove_button)

        self.stepbp_set_button = QPushButton("设置STEPBP")
        self.stepbp_set_button.clicked.connect(self.on_stepbp_set)
        action_row.addWidget(self.stepbp_set_button)

        self.stepbp_remove_button = QPushButton("移除STEPBP")
        self.stepbp_remove_button.clicked.connect(self.on_stepbp_remove_all)
        action_row.addWidget(self.stepbp_remove_button)
        action_row.addStretch(1)
        config_layout.addLayout(action_row)
        self._apply_hwbp_active_state()

        result_card, result_layout = self._create_section_card(parent=self.breakpoint_page)
        layout.addWidget(result_card, 1)

        self.hwbp_tree = QTreeWidget()
        self.hwbp_tree.setHeaderLabels(["断点树（hit_addr / records）"])
        self.hwbp_tree.setUniformRowHeights(True)
        self.hwbp_tree.setAlternatingRowColors(True)
        self.hwbp_tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self.hwbp_tree.customContextMenuRequested.connect(self.on_hwbp_tree_context_menu)
        self.hwbp_tree.currentItemChanged.connect(self.on_hwbp_tree_current_item_changed)
        result_layout.addWidget(self.hwbp_tree, 1)

    def _add_hwbp_point_row(self) -> None:
        if len(self.hwbp_point_rows) >= 16:
            return

        row_widget = QWidget(self.breakpoint_page)
        row_layout = QHBoxLayout(row_widget)
        row_layout.setContentsMargins(0, 0, 0, 0)
        row_layout.setSpacing(8)

        label = QLabel(f"P{len(self.hwbp_point_rows)}")
        label.setMinimumWidth(28)
        row_layout.addWidget(label)

        addr_input = QLineEdit()
        addr_input.setPlaceholderText("0x7A12345678")
        row_layout.addWidget(addr_input, 1)

        type_combo = QComboBox()
        type_combo.addItem("BP_READ", "read")
        type_combo.addItem("BP_WRITE", "write")
        type_combo.addItem("BP_READ_WRITE", "read_write")
        type_combo.addItem("BP_EXECUTE", "execute")
        type_combo.setCurrentIndex(3)
        row_layout.addWidget(type_combo)

        scope_combo = QComboBox()
        scope_combo.addItem("BP_SCOPE_MAIN_THREAD", "main")
        scope_combo.addItem("BP_SCOPE_OTHER_THREADS", "other")
        scope_combo.addItem("BP_SCOPE_ALL_THREADS", "all")
        scope_combo.setCurrentIndex(2)
        row_layout.addWidget(scope_combo)

        len_combo = QComboBox()
        for length in range(1, 9):
            len_combo.addItem(f"{length}字节", length)
        len_combo.setCurrentIndex(3)
        row_layout.addWidget(len_combo)

        remove_button = QPushButton("删除")
        remove_button.clicked.connect(lambda _checked=False, widget=row_widget: self._remove_hwbp_point_row(widget))
        row_layout.addWidget(remove_button)

        self.hwbp_point_rows.append(
            {
                "widget": row_widget,
                "label": label,
                "addr": addr_input,
                "type": type_combo,
                "scope": scope_combo,
                "length": len_combo,
                "remove": remove_button,
            }
        )
        self.hwbp_points_layout.addWidget(row_widget)
        self._renumber_hwbp_point_rows()
        self._apply_hwbp_active_state()

    def _remove_hwbp_point_row(self, widget: QWidget | None = None) -> None:
        if len(self.hwbp_point_rows) <= 1:
            return

        remove_index = len(self.hwbp_point_rows) - 1
        if widget is not None:
            for i, row in enumerate(self.hwbp_point_rows):
                if row.get("widget") is widget:
                    remove_index = i
                    break

        row = self.hwbp_point_rows.pop(remove_index)
        row_widget = row.get("widget")
        if isinstance(row_widget, QWidget):
            self.hwbp_points_layout.removeWidget(row_widget)
            row_widget.deleteLater()
        self._renumber_hwbp_point_rows()
        self._apply_hwbp_active_state()

    def _renumber_hwbp_point_rows(self) -> None:
        for i, row in enumerate(self.hwbp_point_rows):
            label = row.get("label")
            if isinstance(label, QLabel):
                label.setText(f"P{i}")

    def _build_signature_page(self) -> None:
        layout = self._create_page_layout(self.signature_page)

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

        result_card, result_layout = self._create_section_card(parent=self.signature_page)
        layout.addWidget(result_card, 1)

        self.sig_status_label = QLabel("特征码状态: 未执行")
        result_layout.addWidget(self.sig_status_label)

        self.sig_view = QTextEdit()
        self.sig_view.setReadOnly(True)
        result_layout.addWidget(self.sig_view, 1)

    def _build_save_page(self) -> None:
        layout = self._create_page_layout(self.save_page)

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

    def _build_log_page(self) -> None:
        layout = self._create_page_layout(self.log_page)

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

    def _build_syscall_page(self) -> None:
        layout = self._create_page_layout(self.syscall_page)
        card, card_layout = self._create_section_card(parent=self.syscall_page)
        layout.addWidget(card, 1)

        row = QHBoxLayout()
        self.syscall_status_label = QLabel("未监听")
        self.syscall_start_button = QPushButton("开始监听")
        self.syscall_stop_button = QPushButton("停止监听")
        refresh_button = QPushButton("刷新日志")
        clear_button = QPushButton("清空显示")
        self.syscall_log_view = QTextEdit()
        self.syscall_log_view.setReadOnly(True)
        self.syscall_log_view.setPlaceholderText("Android 端包含 lsdriver 标签的内核日志将在这里实时显示。")
        self.syscall_start_button.clicked.connect(self.on_syscall_start)
        self.syscall_stop_button.clicked.connect(self.on_syscall_stop)
        refresh_button.clicked.connect(self.on_syscall_log_refresh)
        clear_button.clicked.connect(self.syscall_log_view.clear)
        for widget in (self.syscall_status_label, self.syscall_start_button, self.syscall_stop_button, refresh_button, clear_button):
            row.addWidget(widget)
        row.addStretch(1)
        card_layout.addLayout(row)
        card_layout.addWidget(self.syscall_log_view, 1)
        self._apply_syscall_state()

    def _log(self, text: str) -> None:
        time_text = datetime.now().strftime("%H:%M:%S")
        self.log_view.append(f"[{time_text}] {text}")

    def _set_status(self, text: str) -> None:
        self.status_label.setText(text)
        self._log(f"状态: {text}")

    def _is_connected(self) -> bool:
        return self.http_bridge.is_connected()

    def _set_feature_gate(self, connected: bool) -> None:
        # 断开后保留所有页面可见，方便继续查看已获取的数据。
        for i in range(self.tabs.count()):
            self.tabs.setTabEnabled(i, True)
        self.pid_input.setEnabled(connected)
        self.sync_pid_button.setEnabled(connected)
        self.env_thread_input.setEnabled(connected)
        self.get_env_button.setEnabled(connected)
        self.memory_dump_input.setEnabled(connected)
        self.dump_memory_button.setEnabled(connected)

    def _discover_lan_devices(self) -> list[tuple[str, str]]:
        return [(device.host, device.mac) for device in discover_lan_devices()]

    def _current_device_host_text(self) -> str:
        host_text = self.device_combo.currentText().strip()
        current_index = self.device_combo.currentIndex()
        if current_index >= 0 and host_text == self.device_combo.itemText(current_index).strip():
            host_data = self.device_combo.itemData(current_index)
            if host_data:
                return str(host_data).strip()
        return host_text

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

        if previous_ip and not any(previous_ip == self.device_combo.itemData(i) for i in range(self.device_combo.count())):
            self.device_combo.setCurrentText(previous_ip)

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
        previous_ip = self._current_device_host_text()
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
        host = self._current_device_host_text()
        if not host:
            QMessageBox.warning(self, "输入提示", "请输入设备 IP、HTTP(S) URL，或先扫描局域网设备。")
            return None

        if "://" in host:
            return host, DEFAULT_ANDROID_PORT

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
        self.test_button.setText("测试通信")
        self.device_combo.setEnabled(True)
        self.scan_device_button.setEnabled(True)
        self.port_input.setEnabled(True)
        self._update_connection_badge(connected)
        self._set_feature_gate(connected)

    def _disconnect_device(self, reason: str | None = None) -> None:
        self.http_bridge.disconnect()
        self._set_connection_ui(False)
        self._invalidate_target_ui_state()
        self.live_refresh_inflight = False
        if reason:
            self._set_status(reason)

    def _invalidate_target_ui_state(self) -> None:
        self.target_generation += 1
        self.memory_info_data = None
        self.memory_view.clear()
        self.global_pid_label.setText("--")
        self.scan_view.clear()
        self.scan_page_start = 0
        self.scan_total_count = 0
        self._reset_scan_session()
        self.scan_total_label.setText("总结果数: 0")
        self.saved_items.clear()
        self.saved_refresh_cursor = 0
        self._refresh_saved_view()
        self.browser_current_addr = 0
        self.browser_addr_input.setText("0x0")
        self.browser_view.clear()
        self.pointer_scan_running = False
        self._apply_pointer_control_state(False)
        self.pointer_status_label.setText("扫描状态: 未开始")
        self.pointer_view.clear()
        self.bp_info_data = None
        self.breakpoint_mode = ""
        self.hwbp_selected_index = None
        self.hwbp_num_brps_label.setText("bp_info.num_brps: 0")
        self.hwbp_num_wrps_label.setText("bp_info.num_wrps: 0")
        self.hwbp_points_label.setText("bp_info.points: []")
        self.hwbp_tree.clear()
        self._apply_hwbp_active_state()
        self.syscall_active = False
        self._apply_syscall_state()
        self.syscall_log_view.clear()
        self.environment_view.clear()

    def _connect_device(self) -> None:
        endpoint = self._parse_endpoint()
        if endpoint is None:
            return

        host, port = endpoint
        try:
            self.http_bridge.connect(host, port)
            ping_response = self.http_bridge.call_operation("bridge.ping", {})
            ping_response.require_ok()
            target_response = self.http_bridge.call_operation("target.get", {})
            target_response.require_ok()
        except BridgeConnectionError as exc:
            self.http_bridge.disconnect()
            self._set_status(str(exc))
            return
        except BridgeError as exc:
            self.http_bridge.disconnect()
            error_text = str(exc).strip()
            if "未知 operation" in error_text or "unknown operation" in error_text.lower():
                error_text = "Android 端 HTTP 程序版本过旧，请重新编译并部署当前源码中的可执行程序。"
            QMessageBox.warning(self, "协议不兼容", error_text)
            self._set_status(f"通信失败：{error_text}")
            return

        if not self.live_refresh_timer.isActive():
            self.live_refresh_timer.start()
        self._set_connection_ui(True)
        self._set_status(f"通信成功：{self.http_bridge.url}")

    def _send_operation(self, operation: str, params: dict | None = None, *, log_enabled: bool = True) -> dict | None:
        operation_name = operation.strip()
        request_params = params or {}
        if log_enabled:
            self._log(f"发送操作: {operation_name} params={json.dumps(request_params, ensure_ascii=False)}")
        if not self.http_bridge.is_connected():
            self._set_status("尚未建立通信，请先点击“测试通信”")
            return None

        try:
            bridge_response = self.http_bridge.call_operation(operation_name, request_params)
            response_payload = bridge_response.to_dict()
        except BridgeConnectionError as exc:
            self._set_status(f"请求失败：{exc}")
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

    def _collect_hwbp_points(self) -> list[dict[str, object]]:
        points: list[dict[str, object]] = []
        for row_index, row in enumerate(self.hwbp_point_rows):
            addr_input = row.get("addr")
            type_combo = row.get("type")
            scope_combo = row.get("scope")
            len_combo = row.get("length")
            if not isinstance(addr_input, QLineEdit) or not isinstance(type_combo, QComboBox) or not isinstance(scope_combo, QComboBox) or not isinstance(len_combo, QComboBox):
                continue
            addr_text = addr_input.text().strip()
            try:
                address = int(addr_text, 0)
            except ValueError as exc:
                raise ValueError(f"P{row_index} 地址格式无效") from exc
            if address <= 0:
                raise ValueError(f"P{row_index} 地址必须大于 0")
            length = int(len_combo.currentData())
            points.append(
                {
                    "address": f"0x{address:X}",
                    "bp_type": str(type_combo.currentData()),
                    "bp_scope": str(scope_combo.currentData()),
                    "length": length,
                }
            )
        if not points:
            raise ValueError("至少需要 1 个 point")
        if len(points) > 16:
            raise ValueError("points 最多 16 个")
        return points

    @staticmethod
    def _format_addr(value: object) -> str:
        addr = HttpBridgeWindow._safe_int(value, 0)
        return f"0x{addr:016X}"

    @staticmethod
    def _format_prot(prot_value: object) -> str:
        prot = HttpBridgeWindow._safe_int(prot_value, 0)
        return f"{'r' if (prot & 1) else '-'}{'w' if (prot & 2) else '-'}{'x' if (prot & 4) else '-'}({prot})"

    @staticmethod
    def _format_size(byte_count: int) -> str:
        size = max(0, byte_count)
        value = float(size)
        for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
            if value < 1024.0 or unit == "TiB":
                return f"{value:.0f} {unit}" if unit == "B" else f"{value:.2f} {unit}"
            value /= 1024.0
        return f"{size} B"

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

        lines: list[str] = ["MEMORY INFO", "==========="]
        lines.append(f"STATUS   {status}")
        lines.append(f"MODULES  {len(modules)} / {source_module_count if filter_keyword else module_count}")
        lines.append(f"REGIONS  {len(regions)} / {source_region_count if filter_keyword else region_count}")
        if filter_keyword:
            lines.append(f"FILTER   {filter_keyword}")

        lines.extend(["", "MODULES", "-------"])
        if not modules:
            lines.append("(none)")
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

                lines.append(f"[{idx:03d}] {name if name else '(unnamed)'}  segments={seg_count}")
                if not segs:
                    lines.append("      (no segments)")
                    continue

                lines.append("      SEG  PERM    START               END                 SIZE")
                lines.append("      ---  ------- ------------------- ------------------- ----------")
                for seg_idx, seg in enumerate(segs, start=1):
                    if not isinstance(seg, dict):
                        lines.append(f"      {seg_idx:>3}  (invalid)")
                        continue
                    seg_index = self._safe_int(seg.get("index"), -999)
                    prot_text = self._format_prot(seg.get("prot"))
                    start_value = self._safe_int(seg.get("start"), 0)
                    end_value = self._safe_int(seg.get("end"), 0)
                    start_text = self._format_addr(start_value)
                    end_text = self._format_addr(end_value)
                    size_text = self._format_size(end_value - start_value)
                    lines.append(
                        f"      {seg_index:>3}  {prot_text:<7} {start_text}  {end_text}  {size_text:>10}"
                    )

        lines.extend(["", "SCAN REGIONS", "------------"])
        if not regions:
            lines.append("(none)")
        else:
            lines.append("  #   START               END                 SIZE")
            lines.append("----  ------------------- ------------------- ----------")
            for idx, region in enumerate(regions, start=1):
                if not isinstance(region, dict):
                    lines.append(f"{idx:>4}  (invalid)")
                    continue
                start_value = self._safe_int(region.get("start"), 0)
                end_value = self._safe_int(region.get("end"), 0)
                start_text = self._format_addr(start_value)
                end_text = self._format_addr(end_value)
                size_text = self._format_size(end_value - start_value)
                lines.append(f"{idx:>4}  {start_text}  {end_text}  {size_text:>10}")

        return "\n".join(lines)

    def _render_memory_info(self) -> None:
        if self.memory_info_data is None:
            self.memory_view.setPlainText("暂无内存信息，请先点击“刷新内存信息”。")
            return

        keyword = self.memory_filter_input.text().strip()
        filtered_info = self._filter_memory_info(self.memory_info_data, keyword)
        self.memory_view.setPlainText(self._format_memory_info_text(filtered_info))

    def on_test_communication(self) -> None:
        self._connect_device()

    def _build_scan_request(self, is_first: bool) -> tuple[str, dict] | None:
        mode = self._scan_mode_token()
        value = self.scan_value_input.text().strip()
        range_text = self.scan_range_input.text().strip()

        if is_first and mode in SCAN_HISTORY_MODES:
            QMessageBox.warning(self, "输入提示", "首次扫描不能使用增加、减少、已变化或未变化模式。")
            return None
        if not is_first and self.scan_session_type is None:
            QMessageBox.warning(self, "输入提示", "请先完成首次扫描。")
            return None

        operation = "scan.start" if is_first else "scan.refine"
        params: dict[str, str] = {"mode": mode}
        if mode == "pointer":
            params["value_type"] = "I64"
        elif mode != "string":
            params["value_type"] = self.scan_session_type or self._selected_scan_type_token()

        if mode == "unknown":
            return operation, params

        if mode in SCAN_VALUE_MODES and not value:
            QMessageBox.warning(self, "输入提示", "当前扫描模式需要输入“值”。")
            return None

        if mode in SCAN_VALUE_MODES:
            params["value"] = value
        if mode == "range":
            if not range_text:
                QMessageBox.warning(self, "输入提示", "range 模式需要输入“范围”。")
                return None
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

    def _extract_hwbp_point_index_from_tree_item(self, item: QTreeWidgetItem | None) -> int | None:
        current = item
        while current is not None:
            data = current.data(0, Qt.UserRole + 1)
            if data is not None:
                try:
                    point_index = int(str(data), 10)
                except (TypeError, ValueError):
                    point_index = -1
                if point_index >= 0:
                    return point_index
            current = current.parent()
        return None

    def _bp_info_struct(self) -> dict | None:
        if not isinstance(self.bp_info_data, dict):
            return None
        bp_info = self.bp_info_data.get("bp_info")
        return bp_info if isinstance(bp_info, dict) else None

    def _hwbp_points_raw(self) -> list[object]:
        bp_info = self._bp_info_struct()
        if bp_info is None:
            return []
        points_raw = bp_info.get("points")
        return points_raw if isinstance(points_raw, list) else []

    @staticmethod
    def _hwbp_point_records(point: dict) -> list[dict]:
        records_raw = point.get("records")
        if not isinstance(records_raw, list):
            return []
        return [record for record in records_raw if isinstance(record, dict)]

    def _hwbp_point_record_count(self, point: dict) -> int:
        records = self._hwbp_point_records(point)
        record_count = self._safe_int(point.get("record_count"), -1)
        if record_count < 0:
            record_count = len(records)
        if record_count <= 0 and records:
            record_count = len(records)
        return min(len(records), record_count)

    def _build_hwbp_point_payload(self, point_index: int) -> dict | None:
        points_raw = self._hwbp_points_raw()
        if point_index < 0 or point_index >= len(points_raw):
            return None
        point_info = points_raw[point_index]
        if not isinstance(point_info, dict):
            return None

        matched_records = self._hwbp_point_records(point_info)[: self._hwbp_point_record_count(point_info)]
        total_hit = sum(self._safe_int(record.get("hit_count"), 0) for record in matched_records)
        return {
            "point_index": point_index,
            "point": dict(point_info),
            "record_count": len(matched_records),
            "total_hit_count": total_hit,
            "records": matched_records,
            "hit_addr": self._safe_int(point_info.get("hit_addr"), 0),
        }

    def _hwbp_record_index_exists(self, index: int) -> bool:
        if index < 0:
            return False
        return self._get_hwbp_record_by_index(index) is not None

    def _get_hwbp_record_by_index(self, index: int) -> dict | None:
        if index < 0:
            return None
        points_raw = self._hwbp_points_raw()
        flat_index = 0
        for point in points_raw:
            if not isinstance(point, dict):
                continue
            records = self._hwbp_point_records(point)
            record_count = self._hwbp_point_record_count(point)
            if index < flat_index + record_count:
                local_index = index - flat_index
                if 0 <= local_index < len(records):
                    return records[local_index]
                return None
            flat_index += record_count
        return None

    def _hwbp_record_indices_for_point(self, point_index: int) -> list[int]:
        if point_index < 0:
            return []
        points_raw = self._hwbp_points_raw()
        flat_index = 0
        for idx, point in enumerate(points_raw):
            if not isinstance(point, dict):
                continue
            records = self._hwbp_point_records(point)
            record_count = self._hwbp_point_record_count(point)
            if idx == point_index:
                return list(range(flat_index + record_count - 1, flat_index - 1, -1))
            flat_index += record_count
        return []

    def _remove_hwbp_records(self, indices: list[int]) -> list[int]:
        valid_indices = sorted({idx for idx in indices if idx >= 0}, reverse=True)
        deleted_indices: list[int] = []
        failed_indices: list[int] = []
        for idx in valid_indices:
            response = self._request_ok(
                "breakpoint_record.remove",
                {"index": idx},
                error_title="删除失败",
                warn=False,
            )
            if response is None:
                failed_indices.append(idx)
                continue
            deleted_indices.append(idx)
        self.on_hwbp_refresh(silent=True)
        if failed_indices:
            failed_text = ", ".join(str(idx) for idx in failed_indices)
            QMessageBox.warning(self, "删除失败", f"部分 record 删除失败: {failed_text}")
        return deleted_indices

    def _remove_hwbp_point(self, point_payload: dict) -> None:
        point_index = self._safe_int(point_payload.get("point_index"), -1)
        indices = self._hwbp_record_indices_for_point(point_index)
        if not indices:
            QMessageBox.warning(self, "删除失败", "当前 point 下没有可删除的 bp_record。")
            return

        deleted_indices = self._remove_hwbp_records(indices)
        point = point_payload.get("point")
        point_data = point if isinstance(point, dict) else {}
        hit_addr = self._safe_int(point_data.get("hit_addr"), 0)
        self._set_status(
            f"已删除 point[{point_index}] 0x{hit_addr:X} 下的 {len(deleted_indices)} 条 record"
        )

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
        byte_value = HttpBridgeWindow._safe_int(mask_raw[byte_idx], 0) & 0xFF
        bit_offset = (reg_idx & 0x3) << 1
        return (byte_value >> bit_offset) & 0x3

    @staticmethod
    def _hwbp_mask_op_value(rec: dict, field_name: str) -> int | None:
        reg_idx = HttpBridgeWindow._hwbp_reg_index(field_name)
        if reg_idx is None:
            return None
        return HttpBridgeWindow._hwbp_mask_op_at_index(rec, reg_idx)

    @staticmethod
    def _hwbp_mask_counts(rec: dict) -> tuple[int, int]:
        read_count = 0
        write_count = 0
        for reg_idx in range(HWBP_MAX_REG_COUNT):
            op_value = HttpBridgeWindow._hwbp_mask_op_at_index(rec, reg_idx)
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
                HttpBridgeWindow._safe_int(qreg.get("hi"), 0),
                HttpBridgeWindow._safe_int(qreg.get("lo"), 0),
            )
        value = HttpBridgeWindow._safe_int(qreg, 0)
        return ((value >> 64) & ((1 << 64) - 1), value & ((1 << 64) - 1))

    @staticmethod
    def _hwbp_point_type_label(point: dict) -> str:
        return HWBP_POINT_TYPE_LABELS.get(HttpBridgeWindow._safe_int(point.get("bt"), 0), "未知")

    @staticmethod
    def _hwbp_point_scope_label(point: dict) -> str:
        return HWBP_SCOPE_LABELS.get(HttpBridgeWindow._safe_int(point.get("bs"), 0), "未知")

    @staticmethod
    def _hwbp_point_length_text(point: dict) -> str:
        length = HttpBridgeWindow._safe_int(point.get("bl"), 0)
        return f"{length}字节" if length > 0 else "未知"

    def _decode_hwbp_rw_text(self, rec: dict) -> str:
        read_count, write_count = self._hwbp_mask_counts(rec)
        if write_count and read_count:
            return "读/写"
        if write_count:
            return "写入"
        if read_count:
            return "读取"
        return "未知"

    def _render_hwbp_tree(self, bp_info: dict) -> None:
        prev_expanded_points: set[int] = set()
        prev_expanded_records_dirs: set[int] = set()
        prev_expanded_records: set[int] = set()
        for i in range(self.hwbp_tree.topLevelItemCount()):
            point_item = self.hwbp_tree.topLevelItem(i)
            if point_item is None or not point_item.isExpanded():
                continue
            point_index = self._safe_int(point_item.data(0, Qt.UserRole + 1), -1)
            if point_index >= 0:
                prev_expanded_points.add(point_index)
            for j in range(point_item.childCount()):
                records_dir = point_item.child(j)
                if records_dir is None or not records_dir.isExpanded():
                    continue
                records_point_index = self._safe_int(records_dir.data(0, Qt.UserRole + 1), -1)
                if records_point_index >= 0:
                    prev_expanded_records_dirs.add(records_point_index)
                for k in range(records_dir.childCount()):
                    record_item = records_dir.child(k)
                    if record_item is None or not record_item.isExpanded():
                        continue
                    record_index = self._safe_int(record_item.data(0, Qt.UserRole), -1)
                    if record_index >= 0:
                        prev_expanded_records.add(record_index)
        had_previous_items = self.hwbp_tree.topLevelItemCount() > 0
        old_scroll = self.hwbp_tree.verticalScrollBar().value()

        self.hwbp_tree.clear()
        points_raw = bp_info.get("points")
        if not isinstance(points_raw, list):
            points_raw = []
        point_infos = [point for point in points_raw if isinstance(point, dict)]
        if not point_infos:
            empty_item = QTreeWidgetItem(["暂无 hit_addr 目录"])
            self.hwbp_tree.addTopLevelItem(empty_item)
            return

        point_flat_starts: dict[int, int] = {}
        flat_index = 0
        for point_index, point in enumerate(points_raw):
            if not isinstance(point, dict):
                continue
            point_flat_starts[point_index] = flat_index
            flat_index += self._hwbp_point_record_count(point)

        visible_points: list[tuple[int, dict, list[dict]]] = []
        for point_index, point in enumerate(points_raw):
            if not isinstance(point, dict):
                continue
            hit_addr = self._safe_int(point.get("hit_addr"), 0)
            if point_index < 0 or hit_addr <= 0:
                continue
            rec_list = self._hwbp_point_records(point)[: self._hwbp_point_record_count(point)]
            visible_points.append((point_index, point, rec_list))

        visible_points.sort(
            key=lambda item: (
                self._safe_int(item[1].get("hit_addr"), 0),
                self._safe_int(item[0], 0),
            )
        )

        if not visible_points:
            empty_item = QTreeWidgetItem(["暂无 hit_addr 目录"])
            self.hwbp_tree.addTopLevelItem(empty_item)
            return

        for point_index, point, rec_list in visible_points:
            hit_addr = self._safe_int(point.get("hit_addr"), 0)
            point_type = self._hwbp_point_type_label(point)
            point_scope = self._hwbp_point_scope_label(point)
            point_len = self._hwbp_point_length_text(point)
            point_hit_total = sum(self._safe_int(rec.get("hit_count"), 0) for rec in rec_list)
            point_record_count = len(rec_list)
            point_flat_start = point_flat_starts.get(point_index, 0)

            top = QTreeWidgetItem(
                [
                    f"0x{hit_addr:X}  |  point[{point_index}]  |  records {point_record_count}  |  总命中 {point_hit_total}  |  {point_type}/{point_scope}/{point_len}"
                ]
            )
            top.setData(0, Qt.UserRole + 1, point_index)
            self.hwbp_tree.addTopLevelItem(top)
            top.setExpanded((not had_previous_items) or (point_index in prev_expanded_points))

            records_dir = QTreeWidgetItem([f"records  |  {point_record_count} 条"])
            records_dir.setData(0, Qt.UserRole + 1, point_index)
            top.addChild(records_dir)
            records_dir.setExpanded((not had_previous_items) or (point_index in prev_expanded_records_dirs))

            for point_record_index, rec in enumerate(rec_list):
                record_flat_index = point_flat_start + point_record_index
                hit_count = self._safe_int(rec.get("hit_count"), 0)
                pc = self._safe_int(rec.get("pc"), 0)
                rw_text = self._decode_hwbp_rw_text(rec)
                ops_summary = self._hwbp_ops_summary(rec)

                record_item = QTreeWidgetItem(
                    [
                        f"PC 0x{pc:X}  |  point[{point_index}:{point_record_index}]  |  命中 {hit_count} 次  |  类型 {rw_text}  |  掩码 {ops_summary}"
                    ]
                )
                record_item.setData(0, Qt.UserRole, record_flat_index)
                record_item.setData(0, Qt.UserRole + 1, point_index)
                records_dir.addChild(record_item)
                record_item.setExpanded((not had_previous_items) or (record_flat_index in prev_expanded_records))

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
                    record_item.addChild(
                        self._make_hwbp_field_item(
                            record_flat_index,
                            field_name,
                            value_text,
                            f"  {label}: {value_text}  [{self._hwbp_reg_op(rec, field_name)}]",
                        )
                    )

                mask_raw = rec.get("mask")
                if isinstance(mask_raw, list) and mask_raw:
                    mask_text = " ".join(f"{self._safe_int(byte, 0) & 0xFF:02X}" for byte in mask_raw[:18])
                    mask_item = QTreeWidgetItem([f"  MASK: {mask_text}"])
                    mask_item.setData(0, Qt.UserRole, record_flat_index)
                    mask_item.setData(0, Qt.UserRole + 1, point_index)
                    record_item.addChild(mask_item)

                regs = self._hwbp_xregs(rec)
                regs_title = QTreeWidgetItem(["  寄存器快照 X0~X29"])
                regs_title.setData(0, Qt.UserRole, record_flat_index)
                regs_title.setData(0, Qt.UserRole + 1, point_index)
                record_item.addChild(regs_title)
                for reg_idx, reg_val in enumerate(regs):
                    reg_hex = self._safe_int(reg_val, 0)
                    field_name = f"x{reg_idx}"
                    value_text = f"0x{reg_hex:X}"
                    record_item.addChild(
                        self._make_hwbp_field_item(
                            record_flat_index,
                            field_name,
                            value_text,
                            f"    X{reg_idx}: {value_text}  [{self._hwbp_reg_op(rec, field_name)}]",
                        )
                    )

                qregs = self._hwbp_qregs(rec)
                if qregs:
                    qregs_title = QTreeWidgetItem(["  SIMD 寄存器快照 Q0~Q31"])
                    qregs_title.setData(0, Qt.UserRole, record_flat_index)
                    qregs_title.setData(0, Qt.UserRole + 1, point_index)
                    record_item.addChild(qregs_title)
                    for reg_idx, qreg_val in enumerate(qregs):
                        hi, lo = self._hwbp_qreg_parts(qreg_val)
                        field_name = f"q{reg_idx}"
                        qreg_hex = f"0x{hi:016X}{lo:016X}"
                        qreg_display = f"0x{hi:016X}_{lo:016X}"
                        record_item.addChild(
                            self._make_hwbp_field_item(
                                record_flat_index,
                                field_name,
                                qreg_hex,
                                f"    Q{reg_idx}: {qreg_display}  [{self._hwbp_reg_op(rec, field_name)}]",
                            )
                        )

                if self._hwbp_mask_counts(rec)[1] > 0:
                    write_title = QTreeWidgetItem(["  写入寄存器候选"])
                    write_title.setData(0, Qt.UserRole, record_flat_index)
                    write_title.setData(0, Qt.UserRole + 1, point_index)
                    record_item.addChild(write_title)
                    x0_val = self._safe_int(regs[0], 0) if len(regs) > 0 else 0
                    x1_val = self._safe_int(regs[1], 0) if len(regs) > 1 else 0
                    x0_text = f"0x{x0_val:X}"
                    x1_text = f"0x{x1_val:X}"
                    record_item.addChild(
                        self._make_hwbp_field_item(record_flat_index, "x0", x0_text, f"    候选写入值(X0): {x0_text}")
                    )
                    record_item.addChild(
                        self._make_hwbp_field_item(record_flat_index, "x1", x1_text, f"    候选写入地址(X1): {x1_text}")
                    )

                separator = QTreeWidgetItem([""])
                separator.setData(0, Qt.UserRole, record_flat_index)
                separator.setData(0, Qt.UserRole + 1, point_index)
                record_item.addChild(separator)

        self.hwbp_tree.resizeColumnToContents(0)
        self.hwbp_tree.verticalScrollBar().setValue(
            min(old_scroll, self.hwbp_tree.verticalScrollBar().maximum())
        )

    def _render_bp_info(self, info: dict) -> None:
        bp_info = info.get("bp_info")
        if not isinstance(bp_info, dict):
            bp_info = {}
        self.breakpoint_mode = str(info.get("mode") or "").strip().lower()
        num_brps = self._safe_int(bp_info.get("num_brps"), 0)
        num_wrps = self._safe_int(bp_info.get("num_wrps"), 0)
        self.hwbp_num_brps_label.setText(f"bp_info.num_brps: {num_brps}")
        self.hwbp_num_wrps_label.setText(f"bp_info.num_wrps: {num_wrps}")
        points_raw = bp_info.get("points")
        points = points_raw if isinstance(points_raw, list) else []
        point_parts: list[str] = []
        for point_index, point in enumerate(points):
            if not isinstance(point, dict):
                continue
            hit_addr = self._safe_int(point.get("hit_addr"), 0)
            if hit_addr <= 0:
                continue
            point_type = self._hwbp_point_type_label(point)
            point_scope = self._hwbp_point_scope_label(point)
            point_len = self._hwbp_point_length_text(point)
            point_records = self._hwbp_point_record_count(point)
            point_parts.append(
                f"[{point_index}] 0x{hit_addr:X} {point_type}/{point_scope}/{point_len}/records{point_records}"
            )
        points_text = "; ".join(point_parts) if point_parts else "[]"
        if self.breakpoint_mode:
            mode_label = self.breakpoint_mode.upper()
            self.hwbp_points_label.setText(f"bp_info.points: {points_text}  mode: {mode_label}")
        else:
            self.hwbp_points_label.setText(f"bp_info.points: {points_text}  mode: none")
        self._apply_hwbp_active_state()
        if self.hwbp_selected_index is not None and not self._hwbp_record_index_exists(self.hwbp_selected_index):
            self.hwbp_selected_index = None
        self._render_hwbp_tree(bp_info)

    def _apply_hwbp_active_state(self) -> None:
        active = bool(self.breakpoint_mode)
        if hasattr(self, "hwbp_set_button"):
            self.hwbp_set_button.setEnabled(not active)
        if hasattr(self, "hwbp_remove_button"):
            self.hwbp_remove_button.setEnabled(self.breakpoint_mode == "hwbp")
        if hasattr(self, "ptebp_set_button"):
            self.ptebp_set_button.setEnabled(not active)
        if hasattr(self, "ptebp_remove_button"):
            self.ptebp_remove_button.setEnabled(self.breakpoint_mode == "ptebp")
        if hasattr(self, "stepbp_set_button"):
            self.stepbp_set_button.setEnabled(not active)
        if hasattr(self, "stepbp_remove_button"):
            self.stepbp_remove_button.setEnabled(self.breakpoint_mode == "stepbp")
        if hasattr(self, "hwbp_add_point_button"):
            self.hwbp_add_point_button.setEnabled((not active) and len(self.hwbp_point_rows) < 16)
        if hasattr(self, "hwbp_remove_point_button"):
            self.hwbp_remove_point_button.setEnabled((not active) and len(self.hwbp_point_rows) > 1)
        for row in getattr(self, "hwbp_point_rows", []):
            for key in ("addr", "type", "scope", "length"):
                widget = row.get(key)
                if isinstance(widget, QWidget):
                    widget.setEnabled(not active)
            remove_button = row.get("remove")
            if isinstance(remove_button, QPushButton):
                remove_button.setEnabled((not active) and len(self.hwbp_point_rows) > 1)

    @staticmethod
    def _format_sig_result(data: dict) -> str:
        lines: list[str] = []
        count = HttpBridgeWindow._safe_int(data.get("count"), 0)
        returned_count = HttpBridgeWindow._safe_int(data.get("returned_count"), 0)
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
            "I8": 1,
            "I16": 2,
            "I32": 4,
            "I64": 8,
            "Float": 4,
            "Double": 8,
        }
        size = mapping.get(type_token)
        if size is None:
            return None
        return "memory.read", {"address": addr, "size": size}

    @staticmethod
    def _extract_value_field(response: dict | None, type_token: str) -> str | None:
        data = HttpBridgeWindow._response_data_dict(response)
        data_hex = str(data.get("data_hex") or "")
        try:
            raw = bytes.fromhex(data_hex)
            if type_token in {"I8", "I16", "I32", "I64"}:
                return str(int.from_bytes(raw, "little", signed=True))
            if type_token == "Float" and len(raw) == 4:
                return str(struct.unpack("<f", raw)[0])
            if type_token == "Double" and len(raw) == 8:
                return str(struct.unpack("<d", raw)[0])
        except (ValueError, struct.error):
            pass
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
        value = self._extract_value_field(response, type_token)
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
                "lock.remove",
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
            "memory.write",
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

        type_token = self._scan_result_type_token()

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

        type_token = self._scan_result_type_token()

        data = self._request_data_dict(
            "scan.results",
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
        self.scan_live_refresh_enabled = False
        if not silent:
            total = self.scan_total_count
            self._set_status(f"扫描结果已刷新：start={self.scan_page_start}, total={total}")
        return True

    def _run_scan(self, is_first: bool) -> None:
        request = self._build_scan_request(is_first=is_first)
        if request is None:
            return
        operation, params = request
        label = "首次" if is_first else "再次"
        response = self._request_ok(operation, params, error_title="扫描失败", status_on_error=f"{label}扫描失败")
        if response is None:
            return
        self._adopt_scan_state(
            self._response_data_dict(response),
            fallback_type="string" if params["mode"] == "string" else params.get("value_type"),
            fallback_mode=params["mode"],
        )
        self.scan_page_start = 0
        self.scan_total_count = 0
        self.scan_total_label.setText("总结果数: 0")
        self.scan_live_refresh_enabled = True
        self._apply_scan_control_state()
        self._set_status(f"{label}扫描已启动")
        self.on_live_refresh_tick()

    def on_scan_first(self) -> None:
        self._run_scan(True)

    def on_scan_next(self) -> None:
        self._run_scan(False)

    def on_scan_clear(self) -> None:
        response = self._request_ok("scan.clear", error_title="清空失败")
        if response is None:
            return
        self.scan_view.clear()
        self.scan_page_start = 0
        self.scan_total_count = 0
        self._reset_scan_session()
        self.scan_total_label.setText("总结果数: 0")
        self._set_status("扫描结果已清空")

    def on_scan_status(self) -> None:
        data = self._request_data_dict("scan.get", error_title="状态失败")
        if data is None:
            return
        scanning = bool(data.get("scanning", False))
        progress = data.get("progress", 0)
        count = data.get("count", 0)
        self.scan_live_refresh_enabled = scanning
        self._adopt_scan_state(data)
        self.scan_total_label.setText(f"总结果数: {count}")
        self._set_status(f"扫描状态: scanning={1 if scanning else 0}, progress={progress}, count={count}")

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
    def _hex_to_bytes(hex_text: str) -> bytes | None:
        compact = re.sub(r"[^0-9A-Fa-f]", "", hex_text)
        if not compact or len(compact) % 2 != 0:
            return None
        try:
            return bytes.fromhex(compact)
        except ValueError:
            return None

    def _current_browser_view_mode(self) -> str:
        mode_data = self.browser_view_combo.currentData()
        return str(mode_data).strip() if mode_data is not None else "hexadecimal"

    @staticmethod
    def _browser_scroll_unit(view_mode: str) -> int:
        mapping = {
            "hexadecimal": 8,
            "hex": 16,
            "i8": 1,
            "i16": 2,
            "i32": 4,
            "i64": 8,
            "f32": 4,
            "f64": 8,
            "disasm": 4,
        }
        return mapping.get(view_mode, 16)

    def on_browser_wheel_navigate(self, delta_y: int) -> bool:
        step_count = max(1, abs(delta_y) // 120) if abs(delta_y) >= 120 else 1
        self._move_browser_view(-step_count if delta_y > 0 else step_count)
        return True

    @staticmethod
    def _render_hex_dump(addr: int, data: bytes) -> str:
        lines = [
            "OFFSET  ADDRESS             BYTES                                            ASCII",
            "------  ------------------  -----------------------------------------------  ----------------",
        ]
        for offset in range(0, len(data), 16):
            chunk = data[offset : offset + 16]
            hex_part = " ".join(f"{b:02X}" for b in chunk)
            ascii_part = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
            lines.append(f"+{offset:04X}   0x{addr + offset:016X}  {hex_part:<47}  {ascii_part:<16}")
        return "\n".join(lines)

    @staticmethod
    def _render_hexadecimal_dump(addr: int, data: bytes) -> str:
        lines = [
            "OFFSET  ADDRESS             VALUE (U64 LE)",
            "------  ------------------  ------------------",
        ]
        for offset in range(0, len(data), 8):
            chunk = data[offset : offset + 8]
            if len(chunk) == 8:
                value = struct.unpack("<Q", chunk)[0]
                lines.append(f"+{offset:04X}   0x{addr + offset:016X}  0x{value:016X}")
            else:
                raw_hex = " ".join(f"{b:02X}" for b in chunk)
                lines.append(f"+{offset:04X}   0x{addr + offset:016X}  {raw_hex}")
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
            return HttpBridgeWindow._render_hex_dump(addr, data)

        unpack_fmt, unit = mapping[fmt]
        type_label = {
            "i8": "I8",
            "i16": "I16",
            "i32": "I32",
            "i64": "I64",
            "f32": "FLOAT",
            "f64": "DOUBLE",
        }.get(fmt.lower(), fmt.upper())
        lines = [
            f"OFFSET  ADDRESS             VALUE ({type_label})",
            "------  ------------------  ------------------------",
        ]
        for offset in range(0, len(data) - (len(data) % unit), unit):
            chunk = data[offset : offset + unit]
            try:
                value = struct.unpack(unpack_fmt, chunk)[0]
            except struct.error:
                continue
            if isinstance(value, float):
                value_text = f"{value:.9g}" if unit == 4 else f"{value:.17g}"
            else:
                value_text = str(value)
            lines.append(f"+{offset:04X}   0x{addr + offset:016X}  {value_text}")
        if len(lines) == 2:
            lines.append("(no data)")
        return "\n".join(lines)

    @staticmethod
    def _extract_disasm_window(snapshot: dict) -> tuple[list[dict], int]:
        base_addr = HttpBridgeWindow._safe_int(snapshot.get("base"), 0)
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

        visible_addr = HttpBridgeWindow._safe_int(window_items[0].get("address"), base_addr)
        return window_items, visible_addr

    @staticmethod
    def _render_disasm_dump(snapshot: dict) -> str:
        lines = [
            "ADDRESS             BYTES                    INSTRUCTION",
            "------------------  -----------------------  ----------------------------------------",
        ]
        window_items, _visible_addr = HttpBridgeWindow._extract_disasm_window(snapshot)
        if not window_items:
            return "\n".join(lines + ["(no data)"])

        for item in window_items:
            address_hex = str(item.get("address_hex", "0x0"))
            bytes_hex = str(item.get("bytes_hex", ""))
            mnemonic = str(item.get("mnemonic", "")).strip()
            op_str = str(item.get("op_str", "")).strip()
            if op_str:
                lines.append(f"{address_hex:<18}  {bytes_hex:<23}  {mnemonic} {op_str}")
            else:
                lines.append(f"{address_hex:<18}  {bytes_hex:<23}  {mnemonic}")

        return "\n".join(lines)

    def _open_viewer_snapshot(self, addr: int, mode_token: str) -> dict | None:
        snapshot = self._request_data_dict(
            "viewer.open",
            {"address": f"0x{addr:X}", "view_format": mode_token},
            error_title="读取失败",
            error_prefix="打开浏览器失败: ",
            parse_title="解析失败",
            parse_error_text="浏览器数据格式异常。",
            log_enabled=False,
        )
        if snapshot is None:
            return None
        if not bool(snapshot.get("read_success", False)):
            QMessageBox.warning(self, "读取失败", "MemViewer 读取失败。")
            return None
        return snapshot

    def _apply_viewer_snapshot(self, snapshot: dict) -> None:
        base_addr = self._safe_int(snapshot.get("base"), 0)
        data = self._hex_to_bytes(str(snapshot.get("data_hex", "")))
        if data is None:
            QMessageBox.warning(self, "读取失败", "MemViewer HEX 数据解析失败。")
            return
        view_mode = self._current_browser_view_mode()
        if view_mode == "disasm":
            visible_addr = self._extract_disasm_window(snapshot)[1]
            self.browser_current_addr = visible_addr
            self.browser_addr_input.setText(f"0x{visible_addr:X}")
            self.browser_view.setPlainText(self._render_disasm_dump(snapshot))
            return
        if view_mode == "hex":
            text = self._render_hex_dump(base_addr, data)
        elif view_mode == "hexadecimal":
            text = self._render_hexadecimal_dump(base_addr, data)
        else:
            text = self._render_typed_dump(base_addr, data, view_mode)
        self.browser_current_addr = base_addr
        self.browser_addr_input.setText(f"0x{base_addr:X}")
        self.browser_view.setPlainText(text)

    def _move_browser_view(self, lines: int) -> None:
        byte_offset = lines * self._browser_scroll_unit(self._current_browser_view_mode())
        snapshot = self._request_data_dict(
            "viewer.seek",
            {"offset": f"{byte_offset:+#x}"},
            error_title="移动失败",
            error_prefix="内存浏览移动失败: ",
            parse_title="解析失败",
            parse_error_text="浏览器数据格式异常。",
            log_enabled=False,
        )
        if snapshot is not None:
            self._apply_viewer_snapshot(snapshot)

    def _read_browser(self) -> None:
        addr = self._parse_browser_addr()
        if addr is None:
            return
        snapshot = self._open_viewer_snapshot(addr, self._current_browser_view_mode())
        if snapshot is not None:
            self._apply_viewer_snapshot(snapshot)

    def on_browser_read(self) -> None:
        self._read_browser()

    def on_browser_refresh_cache(self) -> None:
        snapshot = self._request_data_dict(
            "viewer.refresh",
            error_title="刷新失败",
            error_prefix="刷新快照失败: ",
            parse_title="解析失败",
            parse_error_text="浏览器数据格式异常。",
            log_enabled=False,
        )
        if snapshot is not None:
            self._apply_viewer_snapshot(snapshot)

    def on_browser_format_changed(self) -> None:
        if not self._is_connected() or self.browser_current_addr <= 0:
            return
        snapshot = self._request_data_dict(
            "viewer.format",
            {"view_format": self._current_browser_view_mode()},
            error_title="格式切换失败",
            error_prefix="格式切换失败: ",
            parse_title="解析失败",
            parse_error_text="浏览器数据格式异常。",
            log_enabled=False,
        )
        if snapshot is not None:
            self._apply_viewer_snapshot(snapshot)

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

        if target <= 0 or max_offset <= 0 or not 1 <= depth <= 16:
            QMessageBox.warning(self, "输入提示", "目标地址和最大偏移必须大于 0，深度范围为 1-16。")
            return None

        mode = self._current_pointer_mode()
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
            if array_base <= 0 or not 1 <= array_count <= 1_000_000:
                QMessageBox.warning(self, "输入提示", "数组基址必须大于 0，数组数量范围为 1-1000000。")
                return None
            params["array_base"] = f"0x{array_base:X}"
            params["array_count"] = str(array_count)

        if filter_text:
            params["module_filter"] = filter_text
        return "pointer.scan", params

    def _current_pointer_mode(self) -> str:
        if self.pointer_manual_mode_button.isChecked():
            return "manual"
        if self.pointer_array_mode_button.isChecked():
            return "array"
        return "module"

    def _update_pointer_mode_inputs(self) -> None:
        mode = self._current_pointer_mode()
        self.pointer_filter_input.setEnabled(mode == "module")
        self.pointer_manual_base_input.setEnabled(mode == "manual")
        self.pointer_array_base_input.setEnabled(mode == "array")
        self.pointer_array_count_input.setEnabled(mode == "array")

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
        self._apply_pointer_control_state(True)
        self._set_status("指针扫描任务已启动")
        self.on_pointer_status()

    def _apply_pointer_control_state(self, busy: bool) -> None:
        self.pointer_scan_button.setEnabled(not busy)
        self.pointer_merge_button.setEnabled(not busy)
        self.pointer_export_button.setEnabled(not busy)

    def _apply_pointer_status_data(self, data: dict, *, silent: bool = False) -> None:
        busy = bool(data.get("busy", False))
        operation = str(data.get("operation") or "idle")
        completed = bool(data.get("completed", False))
        success = bool(data.get("success", False))
        error = str(data.get("error") or "")
        progress = data.get("progress", 0)
        count = data.get("count", 0)
        self.pointer_scan_running = busy
        self._apply_pointer_control_state(busy)
        if busy:
            status_text = f"指针操作: {operation}, progress={progress}, count={count}"
        elif completed and success:
            status_text = f"指针操作完成: count={count}"
        elif completed:
            status_text = f"指针操作失败: {error or '未知错误'}"
        else:
            status_text = "指针状态: 未开始"
        self.pointer_status_label.setText(status_text)
        if not silent:
            self.pointer_view.append(status_text)
            self._set_status("指针状态已刷新")

    def on_pointer_status(self) -> None:
        data = self._request_data_dict("pointer.get", error_title="状态失败")
        if data is None:
            return
        self._apply_pointer_status_data(data, silent=False)

    def on_pointer_merge(self) -> None:
        response = self._request_ok("pointer.merge", error_title="合并失败")
        if response is None:
            return
        self.pointer_view.append("已触发 Pointer.bin 合并任务。")
        self.pointer_scan_running = True
        self._apply_pointer_control_state(True)
        self._set_status("已触发合并任务")

    def on_pointer_export(self) -> None:
        response = self._request_ok("pointer.export", error_title="导出失败")
        if response is None:
            return
        self.pointer_view.append("已触发指针链文本导出。")
        self.pointer_scan_running = True
        self._apply_pointer_control_state(True)
        self._set_status("已触发导出任务")

    def on_live_refresh_tick(self) -> None:
        if not self._is_connected() or self.live_refresh_inflight or self.live_refresh_closing:
            return

        task: dict[str, object] | None = None
        if self._is_scan_tab_active() and self.scan_live_refresh_enabled:
            page_count = self._get_scan_page_size(silent=True)
            if page_count is not None:
                type_token = self._scan_result_type_token()
                task = {"kind": "scan", "start": self.scan_page_start, "count": page_count, "value_type": type_token}
        elif self._is_save_tab_active() and self.saved_items:
            batch_size = min(20, len(self.saved_items))
            entries: list[dict[str, object]] = []
            for offset in range(batch_size):
                index = (self.saved_refresh_cursor + offset) % len(self.saved_items)
                item = self.saved_items[index]
                request = self._build_read_operation_for_type(item.get("type", ""), item.get("addr", ""))
                if request is not None:
                    operation, params = request
                    entries.append({"index": index, "addr": item.get("addr", ""), "type": item.get("type", ""), "operation": operation, "params": params})
            self.saved_refresh_cursor = (self.saved_refresh_cursor + batch_size) % len(self.saved_items)
            if entries:
                task = {"kind": "saved", "entries": entries}
        elif self._is_pointer_tab_active():
            task = {"kind": "pointer"}
        elif self._is_breakpoint_tab_active():
            task = {"kind": "breakpoint"}
        elif self._is_syscall_tab_active():
            task = {"kind": "syscall"}

        if task is None:
            return
        task["generation"] = self.target_generation
        self.live_refresh_inflight = True
        threading.Thread(target=self._run_live_refresh_task, args=(task,), daemon=True).start()

    def _run_live_refresh_task(self, task: dict[str, object]) -> None:
        result: dict[str, object] = {"kind": task.get("kind"), "generation": task.get("generation")}
        try:
            kind = task.get("kind")
            if kind == "scan":
                status = self.http_bridge.call_operation("scan.get").to_dict()
                result["status"] = status
                status_data = self._response_data_dict(status)
                if self._response_ok(status) and not bool(status_data.get("scanning", False)):
                    result["page"] = self.http_bridge.call_operation(
                        "scan.results",
                        {"start": task["start"], "count": task["count"], "value_type": task["value_type"]},
                    ).to_dict()
            elif kind == "saved":
                values: list[dict[str, object]] = []
                for entry in task.get("entries", []):
                    if not isinstance(entry, dict):
                        continue
                    response = self.http_bridge.call_operation(str(entry["operation"]), entry["params"]).to_dict()
                    values.append({**entry, "value": self._extract_value_field(response, str(entry["type"]))})
                result["values"] = values
            elif kind == "pointer":
                result["response"] = self.http_bridge.call_operation("pointer.get").to_dict()
            elif kind == "breakpoint":
                result["response"] = self.http_bridge.call_operation("breakpoint.get").to_dict()
            elif kind == "syscall":
                result["response"] = self.http_bridge.call_operation("syscall.read").to_dict()
        except BridgeConnectionError as exc:
            result["connection_error"] = str(exc)
        except BridgeError as exc:
            result["error"] = str(exc)
        if not self.live_refresh_closing:
            self.live_refresh_finished.emit(result)

    def _apply_live_refresh_result(self, result_obj: object) -> None:
        if not isinstance(result_obj, dict) or self.live_refresh_closing:
            return
        if result_obj.get("generation") != self.target_generation:
            self.live_refresh_inflight = False
            return
        self.live_refresh_inflight = False
        connection_error = str(result_obj.get("connection_error") or "")
        if connection_error:
            self.live_refresh_timer.stop()
            self._set_status(f"后台刷新失败：{connection_error}")
            return

        kind = result_obj.get("kind")
        if kind == "scan":
            status = result_obj.get("status")
            if not isinstance(status, dict) or not self._response_ok(status):
                return
            status_data = self._response_data_dict(status)
            scanning = bool(status_data.get("scanning", False))
            count = self._safe_int(status_data.get("count"), 0)
            self._adopt_scan_state(status_data, fallback_type=self.scan_session_type)
            self.scan_total_label.setText(f"总结果数: {count}")
            if scanning:
                return
            page = result_obj.get("page")
            if isinstance(page, dict) and self._response_ok(page):
                data = self._response_data_dict(page)
                self._render_scan_page(data)
                self.scan_page_start = self._safe_int(data.get("start"), self.scan_page_start)
                self.scan_total_count = self._safe_int(data.get("total_count"), count)
                self.scan_total_label.setText(f"总结果数: {self.scan_total_count}")
            self.scan_live_refresh_enabled = False
            self._apply_scan_control_state()
            self._set_status(f"扫描完成：共 {count} 项")
        elif kind == "saved":
            changed = False
            for entry in result_obj.get("values", []):
                if not isinstance(entry, dict) or entry.get("value") is None:
                    continue
                index = self._safe_int(entry.get("index"), -1)
                if index < 0 or index >= len(self.saved_items):
                    continue
                item = self.saved_items[index]
                if item.get("addr") != entry.get("addr") or item.get("type") != entry.get("type"):
                    continue
                value = str(entry["value"])
                if item.get("value") != value:
                    item["value"] = value
                    changed = True
            if changed:
                self._refresh_saved_view()
        elif kind == "pointer":
            response = result_obj.get("response")
            if isinstance(response, dict) and self._response_ok(response):
                self._apply_pointer_status_data(self._response_data_dict(response), silent=True)
        elif kind == "breakpoint":
            response = result_obj.get("response")
            if isinstance(response, dict) and self._response_ok(response):
                data = self._response_data_dict(response)
                self.bp_info_data = data
                self._render_bp_info(data)
        elif kind == "syscall":
            response = result_obj.get("response")
            if isinstance(response, dict) and self._response_ok(response):
                data = self._response_data_dict(response)
                self._set_text_preserve_interaction(self.syscall_log_view, str(data.get("log", "")))

    def _apply_syscall_state(self) -> None:
        self.syscall_status_label.setText("已监听" if self.syscall_active else "未监听")
        self.syscall_start_button.setEnabled(not self.syscall_active)
        self.syscall_stop_button.setEnabled(self.syscall_active)

    def on_syscall_start(self) -> None:
        data = self._request_data_dict("syscall.start", error_title="监听失败", error_prefix="启动监听失败: ")
        if data is None:
            return
        self.syscall_active = True
        self._apply_syscall_state()
        self._set_status(f"已监听 PID {self._safe_int(data.get('pid'), 0)} 的系统调用")
        self.on_syscall_log_refresh(silent=True)

    def on_syscall_stop(self) -> None:
        data = self._request_data_dict("syscall.stop", error_title="停止失败", error_prefix="停止监听失败: ")
        if data is None:
            return
        self.syscall_active = False
        self._apply_syscall_state()
        self._set_status("系统调用监听已停止")

    def on_syscall_log_refresh(self, silent: bool = False) -> None:
        data = self._request_data_dict(
            "syscall.read", error_title="日志刷新失败",
            log_enabled=not silent, warn=not silent,
        )
        if data is None:
            return
        self._set_text_preserve_interaction(self.syscall_log_view, str(data.get("log", "")))
        if not silent:
            self._set_status(f"系统调用日志已刷新，共 {self._safe_int(data.get('line_count'), 0)} 行")

    def on_sync_pid(self) -> None:
        input_text = self.pid_input.text().strip()
        if not input_text:
            QMessageBox.warning(self, "输入提示", "请输入 PID 或包名。")
            return

        if input_text.isdigit():
            pid_value = int(input_text, 10)
            if pid_value <= 0:
                QMessageBox.warning(self, "输入提示", "PID 必须大于 0。")
                return
            data = self._request_data_dict(
                "target.select",
                {"pid": pid_value},
                error_title="同步失败",
                error_prefix="设置全局 PID 失败：",
                status_on_error="同步失败：请查看服务端返回的具体原因",
            )
        else:
            data = self._request_data_dict(
                "target.attach",
                {"package_name": input_text},
                error_title="同步失败",
                error_prefix="包名附加目标失败：",
                parse_title="同步失败",
                parse_error_text="包名附加目标响应异常。",
                status_on_error="同步失败：请查看服务端返回的具体原因",
            )
        current_pid = self._safe_int(data.get("pid"), 0) if isinstance(data, dict) else 0
        if current_pid <= 0:
            return

        self._invalidate_target_ui_state()
        self.global_pid_label.setText(str(current_pid))
        self._set_status(f"同步成功：全局PID={current_pid}")

    def on_get_environment_params(self) -> None:
        thread_name = self.env_thread_input.text().strip()
        data = self._request_data_dict(
            "env.read",
            {"thread_name": thread_name},
            error_title="环境参数获取失败",
            error_prefix="获取环境参数失败：",
            parse_title="环境参数获取失败",
            parse_error_text="环境参数响应格式异常。",
            status_on_error="环境参数获取失败",
        )
        if data is None:
            return

        lines = [
            f"PID: {self._safe_int(data.get('pid'), 0)}",
            f"线程名: {str(data.get('thread_name') or '(未指定)')}",
            f"TPIDR_EL0: {str(data.get('tpidr_el0_hex') or '0x0')}",
            f"PACGA_LO: {str(data.get('pacga_lo_hex') or '0x0')}",
            f"PACGA_HI: {str(data.get('pacga_hi_hex') or '0x0')}",
            f"TLS 状态: {self._safe_int(data.get('tls_status'), 0)}",
            f"PACGA 状态: {self._safe_int(data.get('pacga_status'), 0)}",
        ]
        self.environment_view.setPlainText("\n".join(lines))
        self._set_status("环境参数获取成功")

    def on_refresh_memory_info(self) -> None:
        response = self._send_operation("memory.map")
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

    def on_dump_memory(self) -> None:
        target = self.memory_dump_input.text().strip()
        if not target:
            QMessageBox.warning(self, "输入提示", "请输入模块名或地址范围。")
            return

        self.dump_memory_button.setEnabled(False)
        self._set_status(f"正在 Dump：{target}")
        try:
            data = self._request_data_dict(
                "memory.dump",
                {"target": target},
                error_title="内存 Dump 失败",
                error_prefix="内存 Dump 失败：",
                parse_title="内存 Dump 失败",
                parse_error_text="内存 Dump 响应格式异常。",
                status_on_error="内存 Dump 失败",
            )
            if data is None:
                return
            path = str(data.get("path") or "").strip()
            self._set_status(f"Dump 完成：{path or target}")
            QMessageBox.information(self, "Dump 完成", f"设备端输出路径：\n{path or '(未返回路径)'}")
        finally:
            self.dump_memory_button.setEnabled(self._is_connected())

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
            "breakpoint.get",
            error_title="刷新失败",
            parse_title="刷新失败",
            parse_error_text="断点信息响应异常。",
            status_on_error="" if silent else "断点信息刷新失败",
            log_enabled=not silent,
            warn=not silent,
        )
        if data is None:
            return
        self.bp_info_data = data
        self._render_bp_info(data)
        if not silent:
            self._set_status("断点信息已刷新")

    def _set_breakpoint_mode(self, mode: str, label: str) -> None:
        if self.breakpoint_mode:
            QMessageBox.information(self, "提示", "断点已激活，请先移除当前断点。")
            return
        try:
            points = self._collect_hwbp_points()
        except ValueError as exc:
            QMessageBox.warning(self, "输入提示", str(exc))
            return
        if self._request_ok("breakpoint.set", {"mode": mode, "points": points}, error_title="设置失败", status_on_error=f"设置 {label} 失败") is None:
            return
        self.breakpoint_mode = mode
        self._apply_hwbp_active_state()
        self._set_status(f"设置 {label} 成功: {len(points)} 个 points")
        self.on_hwbp_refresh(silent=True)

    def _remove_breakpoint_mode(self, mode: str, label: str) -> None:
        if self.breakpoint_mode != mode:
            self._set_status(f"{label} 未激活，无需移除")
            return
        if self._request_ok("breakpoint.clear", error_title="移除失败") is None:
            return
        self.breakpoint_mode = ""
        self._apply_hwbp_active_state()
        self._set_status(f"已移除进程 {label}")
        self.on_hwbp_refresh(silent=True)

    def on_hwbp_set(self) -> None:
        self._set_breakpoint_mode("hwbp", "HWBP")

    def on_hwbp_remove_all(self) -> None:
        self._remove_breakpoint_mode("hwbp", "HWBP")

    def on_ptebp_set(self) -> None:
        self._set_breakpoint_mode("ptebp", "PTEBP")

    def on_ptebp_remove_all(self) -> None:
        self._remove_breakpoint_mode("ptebp", "PTEBP")

    def on_stepbp_set(self) -> None:
        self._set_breakpoint_mode("stepbp", "STEPBP")

    def on_stepbp_remove_all(self) -> None:
        self._remove_breakpoint_mode("stepbp", "STEPBP")

    def on_hwbp_tree_current_item_changed(self, current: QTreeWidgetItem | None, _previous: QTreeWidgetItem | None) -> None:
        if current is None:
            return
        index = self._extract_hwbp_index_from_tree_item(current)
        if index is not None:
            self.hwbp_selected_index = index
        elif self._extract_hwbp_point_index_from_tree_item(current) is not None:
            self.hwbp_selected_index = None

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
            QMessageBox.warning(self, "输入提示", "请先在断点树里选择一条 bp_record。")
            return
        if index < 0:
            QMessageBox.warning(self, "输入提示", "记录索引不能小于 0。")
            return
        if not self._hwbp_record_index_exists(index):
            QMessageBox.warning(self, "输入提示", "选中的 bp_record 已不存在，请刷新后重试。")
            return

        current_value = str(item.data(0, Qt.UserRole + 3) or "0x0")
        value_text, accepted = QInputDialog.getText(
            self,
            "修改寄存器",
            f"bp_record[{index}].{field_name} =",
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
        response = self._request_ok(
            "breakpoint_record.update",
            {"index": index, "field": field_name, "value": formatted_value},
            error_title="写入失败",
        )
        if response is None:
            return
        self.hwbp_selected_index = index
        self._set_status(f"已修改 bp_record[{index}].{field_name} = {formatted_value}")
        self.on_hwbp_refresh(silent=True)

    def on_hwbp_tree_context_menu(self, pos) -> None:
        item = self.hwbp_tree.itemAt(pos)
        if item is None:
            item = self.hwbp_tree.currentItem()
        if item is not None:
            self.hwbp_tree.setCurrentItem(item)
        item_index = self._extract_hwbp_index_from_tree_item(item)
        point_index = self._extract_hwbp_point_index_from_tree_item(item)
        item_field = str(item.data(0, Qt.UserRole + 2)).strip() if item is not None and item.data(0, Qt.UserRole + 2) is not None else ""
        point_payload = None
        record_payload = None
        if item_index is not None:
            record_payload = self._get_hwbp_record_by_index(item_index)
        if record_payload is None and point_index is not None:
            point_payload = self._build_hwbp_point_payload(point_index)

        menu = QMenu(self.hwbp_tree)
        edit_value_action = None
        copy_json_action = None
        delete_action = None
        if item_field:
            edit_value_action = menu.addAction("修改寄存器值")
        if record_payload is not None:
            copy_json_action = menu.addAction("复制当前记录完整JSON")
            delete_action = menu.addAction("删除当前 record")
        elif point_payload is not None:
            copy_json_action = menu.addAction("复制当前 point 完整JSON")
            delete_action = menu.addAction("删除当前 point")
        if edit_value_action is not None or copy_json_action is not None or delete_action is not None:
            menu.addSeparator()

        action = menu.exec(self.hwbp_tree.mapToGlobal(pos))
        if action is None:
            return

        if action == edit_value_action:
            self._edit_hwbp_tree_item_value(item)
            return
        if action == copy_json_action:
            if record_payload is not None:
                QApplication.clipboard().setText(json.dumps(record_payload, ensure_ascii=False, indent=2))
                self._set_status(f"已复制 record[{item_index}] JSON")
            elif point_payload is not None:
                QApplication.clipboard().setText(json.dumps(point_payload, ensure_ascii=False, indent=2))
                self._set_status(f"已复制 point[{point_index}] JSON")
            return
        if action != delete_action:
            return

        if record_payload is not None and item_index is not None:
            deleted_indices = self._remove_hwbp_records([item_index])
            if item_index in deleted_indices:
                self._set_status(f"已删除 record[{item_index}]")
            else:
                self._set_status(f"record[{item_index}] 删除失败")
            return
        if point_payload is not None:
            self._remove_hwbp_point(point_payload)

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
            "signature.create",
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
            "signature.scan",
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
            "signature.match",
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
        self.live_refresh_closing = True
        self.live_refresh_timer.stop()
        self._disconnect_device()
        super().closeEvent(event)


def main() -> int:
    _set_windows_app_id()
    app = QApplication(sys.argv)
    app.setApplicationName(APPLICATION_NAME)
    app.setApplicationDisplayName(APPLICATION_NAME)
    app.setOrganizationName(APPLICATION_NAME)
    app.setWindowIcon(QIcon(str(_resource_path("icon.png"))))
    window = HttpBridgeWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
