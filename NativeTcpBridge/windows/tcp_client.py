#!/usr/bin/env python3
import concurrent.futures
import ipaddress
import json
import re
import socket
import subprocess
import sys
from datetime import datetime

from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QTabWidget,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

DEFAULT_PORT = 9494
NETWORK_TIMEOUT_SECONDS = 6


class TcpTestWindow(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.device_sock: socket.socket | None = None
        self.rx_buffer = b""
        self.is_scanning = False
        self.memory_info_data: dict | None = None
        self.setWindowTitle("TCP 连通性测试工具")
        self.resize(760, 460)
        self._setup_ui()

    def _setup_ui(self) -> None:
        root = QVBoxLayout(self)

        pid_row = QHBoxLayout()
        pid_row.addWidget(QLabel("当前全局PID:"))
        self.global_pid_label = QLabel("--")
        self.global_pid_label.setMinimumWidth(120)
        pid_row.addWidget(self.global_pid_label)

        pid_row.addWidget(QLabel("输入PID或包名:"))
        self.pid_input = QLineEdit()
        self.pid_input.setPlaceholderText("例如 12345 或 com.example.game")
        self.pid_input.returnPressed.connect(self.on_sync_pid)
        pid_row.addWidget(self.pid_input, 1)

        self.sync_pid_button = QPushButton("同步PID")
        self.sync_pid_button.clicked.connect(self.on_sync_pid)
        pid_row.addWidget(self.sync_pid_button)
        root.addLayout(pid_row)

        self.tabs = QTabWidget()
        root.addWidget(self.tabs)

        self.memory_page = QWidget()
        self.search_page = QWidget()
        self.save_page = QWidget()
        self.log_page = QWidget()
        self.settings_page = QWidget()

        self.tabs.addTab(self.memory_page, "内存信息页")
        self.tabs.addTab(self.search_page, "搜索页")
        self.tabs.addTab(self.save_page, "保存页")
        self.tabs.addTab(self.log_page, "日志页")
        self.tabs.addTab(self.settings_page, "设置页")

        self._build_memory_page()
        self.search_page.setLayout(QVBoxLayout())
        self.save_page.setLayout(QVBoxLayout())
        self._build_log_page()

        self._build_settings_page()
        self._log("客户端已启动。")

    def _build_memory_page(self) -> None:
        layout = QVBoxLayout(self.memory_page)

        row = QHBoxLayout()
        self.refresh_memory_button = QPushButton("刷新内存信息")
        self.refresh_memory_button.clicked.connect(self.on_refresh_memory_info)
        row.addWidget(self.refresh_memory_button)
        row.addStretch(1)
        layout.addLayout(row)

        self.memory_view = QTextEdit()
        self.memory_view.setReadOnly(True)
        self.memory_view.setPlaceholderText("点击“刷新内存信息”后显示可读的 memory_info 结构数据。")
        layout.addWidget(self.memory_view, 1)

    def _build_settings_page(self) -> None:
        layout = QVBoxLayout(self.settings_page)

        device_row = QHBoxLayout()
        device_row.addWidget(QLabel("局域网设备:"))
        self.device_combo = QComboBox()
        self.device_combo.setEditable(False)
        self.device_combo.addItem("请点击“扫描设备”获取列表", "")
        device_row.addWidget(self.device_combo, 1)
        self.scan_device_button = QPushButton("扫描设备")
        self.scan_device_button.clicked.connect(self.on_scan_lan_devices)
        device_row.addWidget(self.scan_device_button)
        layout.addLayout(device_row)

        port_row = QHBoxLayout()
        port_row.addWidget(QLabel("端口:"))
        self.port_input = QLineEdit(str(DEFAULT_PORT))
        self.port_input.setPlaceholderText("请输入目标端口")
        port_row.addWidget(self.port_input, 1)
        layout.addLayout(port_row)

        self.test_button = QPushButton("连接到设备")
        self.test_button.clicked.connect(self.on_toggle_connection)
        layout.addWidget(self.test_button)

        status_title = QLabel("状态:")
        layout.addWidget(status_title)

        self.status_label = QLabel("未测试")
        layout.addWidget(self.status_label)

        layout.addStretch(1)

    def _build_log_page(self) -> None:
        layout = QVBoxLayout(self.log_page)

        self.log_view = QTextEdit()
        self.log_view.setReadOnly(True)
        layout.addWidget(self.log_view, 1)

        clear_row = QHBoxLayout()
        clear_button = QPushButton("清空日志")
        clear_button.clicked.connect(self.log_view.clear)
        clear_row.addWidget(clear_button)
        clear_row.addStretch(1)
        layout.addLayout(clear_row)

    def _log(self, text: str) -> None:
        time_text = datetime.now().strftime("%H:%M:%S")
        self.log_view.append(f"[{time_text}] {text}")

    def _set_status(self, text: str) -> None:
        self.status_label.setText(text)
        self._log(f"状态: {text}")

    def _is_connected(self) -> bool:
        return self.device_sock is not None

    def _collect_local_ipv4(self) -> list[str]:
        ips: set[str] = set()

        try:
            infos = socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET)
            for info in infos:
                ip = info[4][0]
                if ip and not ip.startswith("127."):
                    ips.add(ip)
        except OSError:
            pass

        # 通过路由出口补充本机网卡 IPv4。
        try:
            probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            probe.connect(("114.114.114.114", 53))
            ip = probe.getsockname()[0]
            if ip and not ip.startswith("127."):
                ips.add(ip)
            probe.close()
        except OSError:
            pass

        valid_ips: list[str] = []
        for ip_text in ips:
            try:
                ip_obj = ipaddress.IPv4Address(ip_text)
            except ipaddress.AddressValueError:
                continue
            if ip_obj.is_private and not ip_obj.is_loopback:
                valid_ips.append(ip_text)
        return sorted(valid_ips)

    def _collect_subnet_targets(self, local_ips: list[str]) -> tuple[set[str], set[str]]:
        targets: set[str] = set()
        local_set = set(local_ips)
        for ip_text in local_ips:
            try:
                network = ipaddress.ip_network(f"{ip_text}/24", strict=False)
            except ValueError:
                continue
            for host in network.hosts():
                host_text = str(host)
                if host_text not in local_set:
                    targets.add(host_text)
        return targets, local_set

    def _ping_host(self, ip_text: str) -> bool:
        create_no_window = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        try:
            result = subprocess.run(
                ["ping", "-n", "1", "-w", "150", ip_text],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
                timeout=2,
                creationflags=create_no_window,
            )
        except (OSError, subprocess.TimeoutExpired):
            return False
        return result.returncode == 0

    def _read_arp_table(self) -> dict[str, str]:
        arp_map: dict[str, str] = {}
        try:
            result = subprocess.run(
                ["arp", "-a"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="ignore",
                timeout=5,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired):
            return arp_map

        if result.returncode != 0:
            return arp_map

        pattern = re.compile(r"(\d+\.\d+\.\d+\.\d+)\s+([0-9A-Fa-f-]{17})\s+\S+")
        for ip_text, mac_text in pattern.findall(result.stdout):
            arp_map[ip_text] = mac_text.lower()
        return arp_map

    def _discover_lan_devices(self) -> list[tuple[str, str]]:
        local_ips = self._collect_local_ipv4()
        if not local_ips:
            return []

        targets, local_set = self._collect_subnet_targets(local_ips)
        if targets:
            with concurrent.futures.ThreadPoolExecutor(max_workers=24) as pool:
                list(pool.map(self._ping_host, sorted(targets)))

        arp_map = self._read_arp_table()
        devices: list[tuple[str, str]] = []
        for ip_text, mac_text in arp_map.items():
            if ip_text in local_set:
                continue
            if targets and ip_text not in targets:
                continue
            devices.append((ip_text, mac_text))

        devices.sort(key=lambda item: int(ipaddress.IPv4Address(item[0])))
        return devices

    def on_scan_lan_devices(self) -> None:
        if self.is_scanning:
            return

        self.is_scanning = True
        self.scan_device_button.setEnabled(False)
        previous_ip = self.device_combo.currentData() if self.device_combo.count() > 0 else ""
        self._set_status("正在扫描局域网设备，请稍候...")
        QApplication.processEvents()

        try:
            devices = self._discover_lan_devices()
            self.device_combo.clear()
            if not devices:
                self.device_combo.addItem("未发现设备，请确认同网段后重试", "")
                self._set_status("扫描完成：未发现设备")
                return

            selected_index = 0
            for idx, (ip_text, mac_text) in enumerate(devices):
                self.device_combo.addItem(f"{ip_text}    [{mac_text}]", ip_text)
                if previous_ip and previous_ip == ip_text:
                    selected_index = idx

            self.device_combo.setCurrentIndex(selected_index)
            self._set_status(f"扫描完成：发现 {len(devices)} 台设备")
        finally:
            self.is_scanning = False
            self.scan_device_button.setEnabled(True)

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

    def _disconnect_device(self, reason: str | None = None) -> None:
        if self.device_sock is not None:
            try:
                self.device_sock.close()
            except OSError:
                pass
        self.device_sock = None
        self.rx_buffer = b""
        self._set_connection_ui(False)
        self.global_pid_label.setText("--")
        if reason:
            self._set_status(reason)

    def _connect_device(self) -> None:
        endpoint = self._parse_endpoint()
        if endpoint is None:
            return

        host, port = endpoint
        try:
            sock = socket.create_connection((host, port), timeout=NETWORK_TIMEOUT_SECONDS)
            sock.settimeout(NETWORK_TIMEOUT_SECONDS)
        except (ConnectionRefusedError, TimeoutError):
            self._set_status(f"连接失败：无法连接到 {host}:{port}")
            return
        except socket.timeout:
            self._set_status("连接失败：套接字超时")
            return
        except OSError as exc:
            if exc.errno is not None:
                self._set_status(f"连接失败：网络异常（错误码 {exc.errno}）")
            else:
                self._set_status("连接失败：网络异常")
            return

        self.device_sock = sock
        self.rx_buffer = b""
        self._set_connection_ui(True)
        self._set_status(f"已连接到设备：{host}:{port}")

    def _read_response_line(self) -> str | None:
        if self.device_sock is None:
            return None

        while True:
            split_idx = self.rx_buffer.find(b"\n")
            if split_idx != -1:
                line = self.rx_buffer[:split_idx]
                self.rx_buffer = self.rx_buffer[split_idx + 1 :]
                text = line.decode("utf-8", errors="replace").strip()
                if not text:
                    continue
                return text

            try:
                data = self.device_sock.recv(4096)
            except socket.timeout:
                self._disconnect_device("连接已断开：等待响应超时")
                return None
            except OSError as exc:
                if exc.errno is not None:
                    self._disconnect_device(f"连接已断开：网络异常（错误码 {exc.errno}）")
                else:
                    self._disconnect_device("连接已断开：网络异常")
                return None

            if not data:
                self._disconnect_device("连接已断开：服务端关闭连接")
                return None

            self.rx_buffer += data
            if len(self.rx_buffer) > 65536:
                self._disconnect_device("连接已断开：响应数据异常")
                return None

    def _read_exact_bytes(self, size: int) -> bytes | None:
        if self.device_sock is None:
            return None
        if size < 0 or size > 32 * 1024 * 1024:
            self._disconnect_device("连接已断开：响应数据大小异常")
            return None

        while len(self.rx_buffer) < size:
            try:
                data = self.device_sock.recv(4096)
            except socket.timeout:
                self._disconnect_device("连接已断开：等待数据超时")
                return None
            except OSError as exc:
                if exc.errno is not None:
                    self._disconnect_device(f"连接已断开：网络异常（错误码 {exc.errno}）")
                else:
                    self._disconnect_device("连接已断开：网络异常")
                return None

            if not data:
                self._disconnect_device("连接已断开：服务端关闭连接")
                return None
            self.rx_buffer += data

        payload = self.rx_buffer[:size]
        self.rx_buffer = self.rx_buffer[size:]
        return payload

    def _send_tcp_command(self, command: str) -> str | None:
        self._log(f"发送命令: {command}")
        if self.device_sock is None:
            self._set_status("未连接设备，请先点击“连接到设备”")
            return None

        try:
            self.device_sock.sendall((command.strip() + "\n").encode("utf-8"))
        except OSError as exc:
            if exc.errno is not None:
                self._disconnect_device(f"连接已断开：发送失败（错误码 {exc.errno}）")
            else:
                self._disconnect_device("连接已断开：发送失败")
            return None

        text = self._read_response_line()
        if text is None:
            return None
        self._log(f"收到响应: {text}")
        return text

    def _send_memory_info_command(self) -> str | None:
        self._log("发送命令: memory.info.full")
        if self.device_sock is None:
            self._set_status("未连接设备，请先点击“连接到设备”")
            return None

        try:
            self.device_sock.sendall(b"memory.info.full\n")
        except OSError as exc:
            if exc.errno is not None:
                self._disconnect_device(f"连接已断开：发送失败（错误码 {exc.errno}）")
            else:
                self._disconnect_device("连接已断开：发送失败")
            return None

        header = self._read_response_line()
        if header is None:
            return None
        self._log(f"收到响应头: {header}")

        if header.startswith("err "):
            return header

        prefix = "ok memory.info.full size="
        if not header.startswith(prefix):
            return f"err 响应头格式异常: {header}"

        size_text = header[len(prefix) :].strip()
        try:
            payload_size = int(size_text, 10)
        except ValueError:
            return f"err 响应头大小无效: {size_text}"

        payload_bytes = self._read_exact_bytes(payload_size)
        if payload_bytes is None:
            return None

        self._log(f"收到内存信息数据: {payload_size} 字节")
        payload_text = payload_bytes.decode("utf-8", errors="replace")
        return f"ok {payload_text}"

    @staticmethod
    def _extract_pid(response: str | None) -> int | None:
        if not response or "pid=" not in response:
            return None
        try:
            pid_text = response.split("pid=", 1)[1].split()[0].strip()
            pid = int(pid_text, 10)
        except (ValueError, IndexError):
            return None
        return pid if pid > 0 else None

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

    def _format_memory_info_text(self, info: dict) -> str:
        status = self._safe_int(info.get("status"), 0)
        module_count = self._safe_int(info.get("module_count"), 0)
        region_count = self._safe_int(info.get("region_count"), 0)

        modules_raw = info.get("modules")
        regions_raw = info.get("regions")
        modules = modules_raw if isinstance(modules_raw, list) else []
        regions = regions_raw if isinstance(regions_raw, list) else []

        lines: list[str] = []
        lines.append("【内存信息概要】")
        lines.append(f"状态: {status}")
        lines.append(f"模块数量(头部): {module_count}")
        lines.append(f"内存区域数量(头部): {region_count}")
        lines.append(f"模块数量(实际): {len(modules)}")
        lines.append(f"内存区域数量(实际): {len(regions)}")
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

    def on_toggle_connection(self) -> None:
        if self._is_connected():
            self._disconnect_device("已断开连接")
            return
        self._connect_device()

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
            get_response = self._send_tcp_command(f"pid.get {input_text}")
            pid_value = self._extract_pid(get_response)
            if pid_value is None:
                QMessageBox.warning(self, "同步失败", "包名获取 PID 失败。")
                self._set_status("同步失败：包名获取 PID 失败")
                return

        set_response = self._send_tcp_command(f"pid.set {pid_value}")
        if set_response is None or not set_response.startswith("ok"):
            QMessageBox.warning(self, "同步失败", "设置全局 PID 失败。")
            self._set_status("同步失败：设置全局 PID 失败")
            return

        current_response = self._send_tcp_command("pid.current")
        current_pid = self._extract_pid(current_response)
        if current_pid is None:
            self.global_pid_label.setText("--")
            self._set_status("同步后读取全局 PID 失败")
            return

        self.global_pid_label.setText(str(current_pid))
        self._set_status(f"同步成功：全局PID={current_pid}")

    def on_refresh_memory_info(self) -> None:
        response = self._send_memory_info_command()
        if response is None:
            return

        if not response.startswith("ok "):
            self.memory_view.setPlainText(f"刷新失败：\n{response}")
            self._set_status("刷新内存信息失败")
            QMessageBox.warning(self, "刷新失败", f"内存信息刷新失败：{response}")
            return

        payload = response[3:]
        try:
            info = json.loads(payload)
        except json.JSONDecodeError as exc:
            self.memory_view.setPlainText(f"JSON 解析失败：{exc}\n\n原始内容：\n{payload[:4000]}")
            self._set_status("刷新内存信息失败：JSON解析失败")
            return

        self.memory_info_data = info
        self.memory_view.setPlainText(self._format_memory_info_text(info))

        module_count = info.get("module_count", "未知")
        region_count = info.get("region_count", "未知")
        self._set_status(f"内存信息刷新成功：模块={module_count}，区域={region_count}")

    def closeEvent(self, event) -> None:  # type: ignore[override]
        self._disconnect_device()
        super().closeEvent(event)


def main() -> int:
    app = QApplication(sys.argv)
    window = TcpTestWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
