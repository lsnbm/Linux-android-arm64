#!/usr/bin/env python3
from __future__ import annotations

"""Unified Windows-side bridge facade used by MCP and the desktop UI."""

import concurrent.futures
import ipaddress
import json
import os
import re
import socket
import subprocess
import threading
from dataclasses import asdict, dataclass, field
from typing import Any

DEFAULT_ANDROID_HOST = os.getenv("ANDROID_TCP_HOST", "auto").strip() or "auto"
DEFAULT_ANDROID_PORT = int(os.getenv("ANDROID_TCP_PORT", "9494"))
DEFAULT_ANDROID_TIMEOUT_SECONDS = float(os.getenv("ANDROID_TCP_TIMEOUT", "8"))
LAN_DISCOVERY_MAX_WORKERS = 24
LAN_DISCOVERY_PING_TIMEOUT_MS = 150
MAX_NDJSON_FRAME_BYTES = 16 * 1024 * 1024
AUTO_HOST_TOKENS = {"", "auto", "*"}
VIEWER_FORMAT_TOKENS = {"hex", "hex64", "i8", "i16", "i32", "i64", "f32", "f64", "disasm"}


class BridgeError(RuntimeError):
    """Base error for bridge transport and protocol failures."""


class BridgeConnectionError(BridgeError):
    """Raised when the Android TCP bridge cannot be reached."""


class BridgeRequestOutcomeUnknown(BridgeConnectionError):
    """Raised when a sent request may have executed but no response was received."""


class BridgeProtocolError(BridgeError):
    """Raised when the Android TCP bridge returns an invalid payload."""


@dataclass(frozen=True)
class LanDevice:
    host: str
    mac: str

    def to_dict(self) -> dict[str, str]:
        return {"host": self.host, "mac": self.mac}


@dataclass
class BridgeConfig:
    host: str = DEFAULT_ANDROID_HOST
    port: int = DEFAULT_ANDROID_PORT
    timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS
    last_connected_host: str = ""
    last_discovered_devices: list[LanDevice] = field(default_factory=list)


@dataclass
class _BridgeConnection:
    sock: socket.socket
    host: str
    port: int
    rx_buffer: bytes = b""


@dataclass
class BridgeResponse:
    ok: bool
    operation: str = ""
    data: Any = None
    error: str = ""
    connection: dict[str, Any] = field(default_factory=dict)

    def require_ok(self) -> "BridgeResponse":
        if not self.ok:
            raise BridgeError(self.error or "unknown bridge error")
        return self

    def to_dict(self) -> dict[str, Any]:
        payload: dict[str, Any] = {
            "ok": self.ok,
            "operation": self.operation,
            "connection": dict(self.connection),
        }
        if self.data is not None:
            payload["data"] = self.data
        if self.error:
            payload["error"] = self.error
        return payload


def format_address(value: int | str) -> str:
    if isinstance(value, int):
        return f"0x{value:X}"
    return str(value).strip()


def normalize_view_format(view_format: str) -> str:
    token = str(view_format).strip().lower()
    if token not in VIEWER_FORMAT_TOKENS:
        raise ValueError("view_format must be one of: hex, hex64, i8, i16, i32, i64, f32, f64, disasm")
    return token


def normalize_bridge_host(host: str | None) -> str:
    normalized = "" if host is None else str(host).strip()
    return normalized or "auto"


def is_auto_bridge_host(host: str) -> bool:
    return str(host).strip().lower() in AUTO_HOST_TOKENS


def _unique_hosts(*groups: list[str]) -> list[str]:
    seen: set[str] = set()
    ordered: list[str] = []
    for group in groups:
        for host in group:
            host_text = str(host).strip()
            if not host_text or host_text in seen:
                continue
            seen.add(host_text)
            ordered.append(host_text)
    return ordered


def _collect_local_ipv4() -> list[str]:
    ips: set[str] = set()

    try:
        infos = socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET)
        for info in infos:
            ip_text = info[4][0]
            if ip_text and not ip_text.startswith("127."):
                ips.add(ip_text)
    except OSError:
        pass

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.connect(("114.114.114.114", 53))
            ip_text = probe.getsockname()[0]
            if ip_text and not ip_text.startswith("127."):
                ips.add(ip_text)
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


def _collect_subnet_targets(local_ips: list[str]) -> tuple[set[str], set[str]]:
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


def _ping_host(ip_text: str) -> bool:
    create_no_window = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    try:
        result = subprocess.run(
            ["ping", "-n", "1", "-w", str(LAN_DISCOVERY_PING_TIMEOUT_MS), ip_text],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
            timeout=2,
            creationflags=create_no_window,
        )
    except (OSError, subprocess.TimeoutExpired):
        return False
    return result.returncode == 0


def _read_arp_table() -> dict[str, str]:
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


def discover_lan_devices() -> list[LanDevice]:
    local_ips = _collect_local_ipv4()
    if not local_ips:
        return []

    targets, local_set = _collect_subnet_targets(local_ips)
    if targets:
        with concurrent.futures.ThreadPoolExecutor(max_workers=LAN_DISCOVERY_MAX_WORKERS) as pool:
            list(pool.map(_ping_host, sorted(targets)))

    devices: list[LanDevice] = []
    for ip_text, mac_text in _read_arp_table().items():
        if ip_text in local_set:
            continue
        if targets and ip_text not in targets:
            continue
        devices.append(LanDevice(host=ip_text, mac=mac_text))

    devices.sort(key=lambda item: int(ipaddress.IPv4Address(item.host)))
    return devices


def _coerce_bridge_response(
    response_obj: dict[str, Any],
    *,
    fallback_operation: str = "",
    connection: dict[str, Any] | None = None,
) -> BridgeResponse:
    if not isinstance(response_obj, dict):
        raise BridgeProtocolError("android tcp response is not a JSON object")

    ok = bool(response_obj.get("ok", False))
    operation = str(response_obj.get("operation") or fallback_operation)
    error = str(response_obj.get("error", "")) if not ok else ""
    return BridgeResponse(
        ok=ok,
        operation=operation,
        data=response_obj.get("data"),
        error=error,
        connection=connection or {},
    )


class AndroidBridgeSession:
    """Persistent socket session used by interactive clients such as the Windows UI."""

    def __init__(
        self,
        *,
        timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS,
    ) -> None:
        self.timeout_seconds = float(timeout_seconds)
        self._io_lock = threading.Lock()
        self._lifecycle_lock = threading.RLock()
        self._state_lock = threading.Lock()
        self._connection: _BridgeConnection | None = None

    @property
    def host(self) -> str:
        connection = self._current_connection()
        return connection.host if connection is not None else ""

    @property
    def port(self) -> int:
        connection = self._current_connection()
        return connection.port if connection is not None else DEFAULT_ANDROID_PORT

    def _current_connection(self) -> _BridgeConnection | None:
        with self._state_lock:
            return self._connection

    @staticmethod
    def _close_connection(connection: _BridgeConnection | None) -> None:
        if connection is None:
            return
        try:
            connection.sock.close()
        except OSError:
            pass

    def _drop_connection(self, connection: _BridgeConnection) -> None:
        with self._state_lock:
            if self._connection is connection:
                self._connection = None
        self._close_connection(connection)

    def is_connected(self) -> bool:
        return self._current_connection() is not None

    def connect(self, host: str, port: int) -> None:
        if not host.strip():
            raise BridgeConnectionError("host must not be empty")
        if port <= 0 or port > 65535:
            raise BridgeConnectionError("port must be in 1..65535")

        with self._lifecycle_lock:
            self.disconnect()
            try:
                sock = socket.create_connection((host, port), timeout=self.timeout_seconds)
                sock.settimeout(self.timeout_seconds)
            except ConnectionRefusedError as exc:
                raise BridgeConnectionError(f"failed to connect to {host}:{port} (connection refused)") from exc
            except (socket.timeout, TimeoutError) as exc:
                raise BridgeConnectionError(f"connect to {host}:{port} timed out") from exc
            except OSError as exc:
                if exc.errno is not None:
                    raise BridgeConnectionError(f"connect to {host}:{port} failed (errno {exc.errno})") from exc
                raise BridgeConnectionError(f"connect to {host}:{port} failed") from exc

            with self._state_lock:
                self._connection = _BridgeConnection(sock=sock, host=host, port=port)

    def disconnect(self) -> None:
        with self._lifecycle_lock:
            with self._state_lock:
                connection = self._connection
                self._connection = None
            self._close_connection(connection)

    def request(self, request_obj: dict[str, Any]) -> BridgeResponse:
        with self._io_lock:
            connection = self._current_connection()
            if connection is None:
                raise BridgeConnectionError("bridge session is not connected")

            request_text = json.dumps(request_obj, ensure_ascii=False) + "\n"
            try:
                connection.sock.sendall(request_text.encode("utf-8"))
            except OSError as exc:
                self._drop_connection(connection)
                if exc.errno is not None:
                    raise BridgeRequestOutcomeUnknown(f"send failed; request outcome is unknown (errno {exc.errno})") from exc
                raise BridgeRequestOutcomeUnknown("send failed; request outcome is unknown") from exc

            response_obj = self._read_response_object(connection)
            return _coerce_bridge_response(
                response_obj,
                fallback_operation=str(request_obj.get("operation") or ""),
                connection={
                    "host": connection.host,
                    "resolved_host": connection.host,
                    "port": connection.port,
                    "timeout_seconds": self.timeout_seconds,
                    "persistent": True,
                },
            )

    def call_operation(self, operation: str, params: dict[str, Any] | None = None) -> BridgeResponse:
        return self.request({"operation": operation.strip(), "params": params or {}})

    def _read_response_object(self, connection: _BridgeConnection) -> dict[str, Any]:
        while True:
            split_index = connection.rx_buffer.find(b"\n")
            if split_index != -1:
                if split_index > MAX_NDJSON_FRAME_BYTES:
                    self._drop_connection(connection)
                    raise BridgeProtocolError("android tcp response exceeds the maximum frame size")
                payload = connection.rx_buffer[:split_index].decode("utf-8", errors="replace").strip()
                connection.rx_buffer = connection.rx_buffer[split_index + 1 :]
                if not payload:
                    continue
                try:
                    response_obj = json.loads(payload)
                except json.JSONDecodeError as exc:
                    raise BridgeProtocolError(f"android tcp response is not valid JSON: {payload}") from exc
                if not isinstance(response_obj, dict):
                    raise BridgeProtocolError("android tcp response is not a JSON object")
                return response_obj

            try:
                chunk = connection.sock.recv(4096)
            except socket.timeout as exc:
                self._drop_connection(connection)
                raise BridgeRequestOutcomeUnknown("wait for response timed out; request outcome is unknown") from exc
            except OSError as exc:
                self._drop_connection(connection)
                if exc.errno is not None:
                    raise BridgeRequestOutcomeUnknown(f"receive failed; request outcome is unknown (errno {exc.errno})") from exc
                raise BridgeRequestOutcomeUnknown("receive failed; request outcome is unknown") from exc

            if not chunk:
                self._drop_connection(connection)
                raise BridgeRequestOutcomeUnknown("android tcp server closed the connection; request outcome is unknown")

            connection.rx_buffer += chunk
            if b"\n" not in connection.rx_buffer and len(connection.rx_buffer) > MAX_NDJSON_FRAME_BYTES:
                self._drop_connection(connection)
                raise BridgeProtocolError("android tcp response exceeds the maximum frame size")


class AndroidBridgeClient:
    """Configurable bridge client used by MCP, scripts, and non-interactive flows."""

    def __init__(
        self,
        *,
        host: str = DEFAULT_ANDROID_HOST,
        port: int = DEFAULT_ANDROID_PORT,
        timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS,
    ) -> None:
        self._lock = threading.RLock()
        self._config = BridgeConfig(
            host=normalize_bridge_host(host),
            port=int(port),
            timeout_seconds=float(timeout_seconds),
        )
        self._session: AndroidBridgeSession | None = None
        self._session_endpoint: tuple[str, int, float] | None = None
        self._target_pid: int | None = None
        self._target_host: str | None = None

    def _close_session_locked(self) -> None:
        if self._session is not None:
            self._session.disconnect()
        self._session = None
        self._session_endpoint = None

    def disconnect(self, *, reset_target: bool = True) -> None:
        with self._lock:
            self._close_session_locked()
            if reset_target:
                self._target_pid = None
                self._target_host = None

    def connect(self) -> dict[str, Any]:
        self.call_operation("bridge.ping").require_ok()
        return self.connection_state()

    def connection_state(self) -> dict[str, Any]:
        with self._lock:
            snapshot = self._config_snapshot_locked()
            snapshot["session_connected"] = self._session is not None and self._session.is_connected()
            snapshot["session_host"] = self._session.host if self._session is not None and self._session.host else None
            snapshot["session_port"] = self._session.port if self._session is not None else self._config.port
            snapshot["session_timeout_seconds"] = self._session.timeout_seconds if self._session is not None else self._config.timeout_seconds
            snapshot["session_pid"] = self._target_pid
            snapshot["session_pid_host"] = self._target_host
            return snapshot

    def _config_snapshot_locked(self) -> dict[str, Any]:
        snapshot = asdict(self._config)
        snapshot["last_discovered_devices"] = [device.to_dict() for device in self._config.last_discovered_devices]
        snapshot["auto_discover"] = is_auto_bridge_host(self._config.host)
        snapshot["resolved_host"] = self._config.last_connected_host or None
        return snapshot

    def configure(
        self,
        *,
        host: str | None = None,
        port: int | None = None,
        timeout_seconds: float | None = None,
    ) -> dict[str, Any]:
        with self._lock:
            should_reset_session = False
            should_reset_target = False
            if host is not None:
                normalized_host = normalize_bridge_host(host)
                if normalized_host != self._config.host:
                    self._config.host = normalized_host
                    self._config.last_connected_host = ""
                    self._config.last_discovered_devices = []
                    should_reset_session = True
                    should_reset_target = True
            if port is not None:
                if port <= 0 or port > 65535:
                    raise ValueError("port must be in 1..65535")
                if int(port) != self._config.port:
                    self._config.port = int(port)
                    should_reset_session = True
                    should_reset_target = True
            if timeout_seconds is not None:
                if timeout_seconds <= 0:
                    raise ValueError("timeout_seconds must be > 0")
                if float(timeout_seconds) != self._config.timeout_seconds:
                    self._config.timeout_seconds = float(timeout_seconds)
                    should_reset_session = True
            if should_reset_session:
                self._close_session_locked()
            if should_reset_target:
                self._target_pid = None
                self._target_host = None
            return self._config_snapshot_locked()

    def current_config(self) -> dict[str, Any]:
        with self._lock:
            return self._config_snapshot_locked()

    def discover(self) -> dict[str, Any]:
        with self._lock:
            config = BridgeConfig(
                host=self._config.host,
                port=self._config.port,
                timeout_seconds=self._config.timeout_seconds,
                last_connected_host=self._config.last_connected_host,
                last_discovered_devices=list(self._config.last_discovered_devices),
            )

        devices = discover_lan_devices() if is_auto_bridge_host(config.host) else []
        candidates = _unique_hosts(
            [config.last_connected_host] if is_auto_bridge_host(config.host) else [],
            [device.host for device in devices] if is_auto_bridge_host(config.host) else [config.host],
        )

        with self._lock:
            self._config.last_discovered_devices = list(devices)

        snapshot = self.current_config()
        snapshot["candidates"] = candidates
        return snapshot

    def call_operation(self, operation: str, params: dict[str, Any] | None = None) -> BridgeResponse:
        op = operation.strip()
        request = {"operation": op, "params": params or {}}
        return self._call_with_discovery(request, fallback_operation=op)

    def _call_with_discovery(self, request: dict[str, Any], *, fallback_operation: str) -> BridgeResponse:
        with self._lock:
            config = BridgeConfig(
                host=self._config.host,
                port=self._config.port,
                timeout_seconds=self._config.timeout_seconds,
                last_connected_host=self._config.last_connected_host,
                last_discovered_devices=list(self._config.last_discovered_devices),
            )

        errors: list[str] = []
        immediate_hosts = [config.host]
        if is_auto_bridge_host(config.host):
            immediate_hosts = _unique_hosts([config.last_connected_host])

        response = self._try_hosts(
            immediate_hosts,
            config.port,
            config.timeout_seconds,
            request,
            fallback_operation,
            errors,
            configured_host=config.host,
        )
        if response is not None:
            self._remember_success(str(response.connection.get("resolved_host", "")))
            return response

        if is_auto_bridge_host(config.host):
            devices = discover_lan_devices()
            self._remember_discovery(devices)
            remaining_hosts = [device.host for device in devices if device.host not in set(immediate_hosts)]
            response = self._try_hosts(
                remaining_hosts,
                config.port,
                config.timeout_seconds,
                request,
                fallback_operation,
                errors,
                configured_host=config.host,
            )
            if response is not None:
                self._remember_success(str(response.connection.get("resolved_host", "")))
                return response

        if errors:
            raise BridgeConnectionError("failed to reach Android tcp_server candidates: " + "; ".join(errors))
        raise BridgeConnectionError("failed to discover any Android tcp_server candidates")

    def _remember_discovery(self, devices: list[LanDevice]) -> None:
        with self._lock:
            self._config.last_discovered_devices = list(devices)

    def _remember_success(self, host: str) -> None:
        if not host:
            return
        with self._lock:
            self._config.last_connected_host = host

    def _try_hosts(
        self,
        hosts: list[str],
        port: int,
        timeout_seconds: float,
        request: dict[str, Any],
        fallback_operation: str,
        errors: list[str],
        *,
        configured_host: str,
    ) -> BridgeResponse | None:
        for host in hosts:
            try:
                return self._call_once(host, port, timeout_seconds, request, fallback_operation, configured_host)
            except BridgeRequestOutcomeUnknown:
                raise
            except BridgeConnectionError as exc:
                errors.append(f"{host}:{port} -> {exc}")
        return None

    def _call_once(
        self,
        host: str,
        port: int,
        timeout_seconds: float,
        request: dict[str, Any],
        fallback_operation: str,
        configured_host: str,
    ) -> BridgeResponse:
        endpoint = (host, int(port), float(timeout_seconds))
        with self._lock:
            operation = str(request.get("operation") or "")
            if self._target_pid is not None and self._target_host not in {None, host}:
                previous_host = self._target_host
                self._target_pid = None
                self._target_host = None
                if operation not in {"bridge.ping", "target.find", "target.select", "target.attach", "target.get"}:
                    raise BridgeError(
                        f"resolved Android device changed from {previous_host} to {host}; "
                        "please set or attach the target again"
                    )

            reconnected = False
            if self._session is None or self._session_endpoint != endpoint:
                self._close_session_locked()
                self._session = AndroidBridgeSession(
                    timeout_seconds=timeout_seconds,
                )
                self._session_endpoint = endpoint
            session = self._session
            if session is None:
                raise BridgeConnectionError("bridge session initialize failed")

            try:
                if not session.is_connected():
                    session.connect(host, port)
                    reconnected = True
                if reconnected and self._target_pid is not None and self._target_host == host and operation not in {"target.select", "target.attach"}:
                    cached_pid = self._target_pid
                    try:
                        session.call_operation("target.select", {"pid": cached_pid}).require_ok()
                    except BridgeError as exc:
                        self._target_pid = None
                        self._target_host = None
                        raise BridgeError(
                            f"cached pid {cached_pid} restore failed after reconnect; "
                            "please set or attach the target again"
                        ) from exc

                response = session.request(request)
                response.connection = {
                    "host": configured_host,
                    "resolved_host": host,
                    "port": port,
                    "timeout_seconds": timeout_seconds,
                    "auto_discover": is_auto_bridge_host(configured_host),
                    "persistent": True,
                }
                if not response.operation:
                    response.operation = fallback_operation
                if response.ok and operation in {"target.select", "target.attach", "target.get"} and isinstance(response.data, dict) and "pid" in response.data:
                    try:
                        self._target_pid = int(str(response.data["pid"]), 0)
                        self._target_host = host
                    except (TypeError, ValueError):
                        pass
                return response
            except (OSError, BridgeError, json.JSONDecodeError):
                if self._session is session:
                    self._close_session_locked()
                raise
