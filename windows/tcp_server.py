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
MAX_RESPONSE_BYTES = 8 * 1024 * 1024
LAN_DISCOVERY_MAX_WORKERS = 24
LAN_DISCOVERY_PING_TIMEOUT_MS = 150
AUTO_HOST_TOKENS = {"", "auto", "*"}
VIEWER_FORMAT_TOKENS = {"hex", "hex16", "hex64", "i8", "i16", "i32", "i64", "f32", "f64", "disasm"}


class BridgeError(RuntimeError):
    """Base error for bridge transport and protocol failures."""


class BridgeConnectionError(BridgeError):
    """Raised when the Android TCP bridge cannot be reached."""


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
class BridgeResponse:
    ok: bool
    operation: str = ""
    command: str = ""
    message: str = ""
    pairs: dict[str, str] = field(default_factory=dict)
    data: Any = None
    error: str = ""
    raw: dict[str, Any] = field(default_factory=dict)
    connection: dict[str, Any] = field(default_factory=dict)

    def require_ok(self) -> "BridgeResponse":
        if not self.ok:
            raise BridgeError(self.error or "unknown bridge error")
        return self

    def to_dict(self) -> dict[str, Any]:
        payload: dict[str, Any] = {
            "ok": self.ok,
            "operation": self.operation,
            "command": self.command,
            "message": self.message,
            "pairs": dict(self.pairs),
            "connection": dict(self.connection),
            "raw": dict(self.raw),
        }
        if self.data is not None:
            payload["data"] = self.data
        if self.error:
            payload["error"] = self.error
        return payload

def parse_message_pairs(message: str) -> dict[str, str]:
    pairs: dict[str, str] = {}
    for token in str(message).split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        pairs[key.strip()] = value.strip()
    return pairs


def format_address(value: int | str) -> str:
    if isinstance(value, int):
        return f"0x{value:X}"
    return str(value).strip()


def normalize_view_format(view_format: str) -> str:
    token = str(view_format).strip().lower()
    if token not in VIEWER_FORMAT_TOKENS:
        raise ValueError("view_format must be one of: hex, hex16, hex64, i8, i16, i32, i64, f32, f64, disasm")
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
    message = str(response_obj.get("message", ""))
    error = str(response_obj.get("error", "")) if not ok else ""
    return BridgeResponse(
        ok=ok,
        operation=operation,
        command=str(response_obj.get("command", "")),
        message=message,
        pairs=parse_message_pairs(message),
        data=response_obj.get("data"),
        error=error,
        raw=response_obj,
        connection=connection or {},
    )


class AndroidBridgeSession:
    """Persistent socket session used by interactive clients such as the Windows UI."""

    def __init__(
        self,
        *,
        timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS,
        max_response_bytes: int = MAX_RESPONSE_BYTES,
    ) -> None:
        self.timeout_seconds = float(timeout_seconds)
        self.max_response_bytes = int(max_response_bytes)
        self._io_lock = threading.Lock()
        self._sock: socket.socket | None = None
        self._rx_buffer = b""
        self.host = ""
        self.port = DEFAULT_ANDROID_PORT

    def is_connected(self) -> bool:
        return self._sock is not None

    def connect(self, host: str, port: int) -> None:
        if not host.strip():
            raise BridgeConnectionError("host must not be empty")
        if port <= 0 or port > 65535:
            raise BridgeConnectionError("port must be in 1..65535")

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

        self._sock = sock
        self._rx_buffer = b""
        self.host = host
        self.port = port

    def disconnect(self) -> None:
        if self._sock is not None:
            try:
                self._sock.close()
            except OSError:
                pass
        self._sock = None
        self._rx_buffer = b""
        self.host = ""

    def request(self, request_obj: dict[str, Any]) -> BridgeResponse:
        with self._io_lock:
            if self._sock is None:
                raise BridgeConnectionError("bridge session is not connected")

            request_text = json.dumps(request_obj, ensure_ascii=False) + "\n"
            try:
                self._sock.sendall(request_text.encode("utf-8"))
            except OSError as exc:
                self.disconnect()
                if exc.errno is not None:
                    raise BridgeConnectionError(f"send failed (errno {exc.errno})") from exc
                raise BridgeConnectionError("send failed") from exc

            response_obj = self._read_response_object()
            return _coerce_bridge_response(
                response_obj,
                fallback_operation=str(request_obj.get("operation") or request_obj.get("command") or ""),
                connection={
                    "host": self.host,
                    "resolved_host": self.host,
                    "port": self.port,
                    "timeout_seconds": self.timeout_seconds,
                    "persistent": True,
                },
            )

    def call_operation(self, operation: str, params: dict[str, Any] | None = None) -> BridgeResponse:
        return self.request({"operation": operation.strip(), "params": params or {}})

    def _read_response_object(self) -> dict[str, Any]:
        if self._sock is None:
            raise BridgeConnectionError("bridge session is not connected")

        while True:
            split_index = self._rx_buffer.find(b"\n")
            if split_index != -1:
                payload = self._rx_buffer[:split_index].decode("utf-8", errors="replace").strip()
                self._rx_buffer = self._rx_buffer[split_index + 1 :]
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
                chunk = self._sock.recv(4096)
            except socket.timeout as exc:
                self.disconnect()
                raise BridgeConnectionError("wait for response timed out") from exc
            except OSError as exc:
                self.disconnect()
                if exc.errno is not None:
                    raise BridgeConnectionError(f"receive failed (errno {exc.errno})") from exc
                raise BridgeConnectionError("receive failed") from exc

            if not chunk:
                self.disconnect()
                raise BridgeConnectionError("android tcp server closed the connection")

            self._rx_buffer += chunk
            if len(self._rx_buffer) > self.max_response_bytes:
                self.disconnect()
                raise BridgeProtocolError("android tcp response is too large")


class AndroidBridgeClient:
    """Configurable bridge client used by MCP, scripts, and non-interactive flows."""

    def __init__(
        self,
        *,
        host: str = DEFAULT_ANDROID_HOST,
        port: int = DEFAULT_ANDROID_PORT,
        timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS,
        max_response_bytes: int = MAX_RESPONSE_BYTES,
    ) -> None:
        self._lock = threading.Lock()
        self._config = BridgeConfig(
            host=normalize_bridge_host(host),
            port=int(port),
            timeout_seconds=float(timeout_seconds),
        )
        self._max_response_bytes = int(max_response_bytes)
        self._session: AndroidBridgeSession | None = None
        self._session_endpoint: tuple[str, int, float] | None = None

    def _close_session_locked(self) -> None:
        if self._session is not None:
            self._session.disconnect()
        self._session = None
        self._session_endpoint = None

    def configure(
        self,
        *,
        host: str | None = None,
        port: int | None = None,
        timeout_seconds: float | None = None,
    ) -> dict[str, Any]:
        with self._lock:
            should_reset_session = False
            if host is not None:
                self._config.host = normalize_bridge_host(host)
                self._config.last_connected_host = ""
                self._config.last_discovered_devices = []
                should_reset_session = True
            if port is not None:
                if port <= 0 or port > 65535:
                    raise ValueError("port must be in 1..65535")
                self._config.port = int(port)
                should_reset_session = True
            if timeout_seconds is not None:
                if timeout_seconds <= 0:
                    raise ValueError("timeout_seconds must be > 0")
                self._config.timeout_seconds = float(timeout_seconds)
                should_reset_session = True
            if should_reset_session:
                self._close_session_locked()
            snapshot = asdict(self._config)
            snapshot["last_discovered_devices"] = [device.to_dict() for device in self._config.last_discovered_devices]
            snapshot["auto_discover"] = is_auto_bridge_host(self._config.host)
            snapshot["resolved_host"] = self._config.last_connected_host or None
            return snapshot

    def current_config(self) -> dict[str, Any]:
        with self._lock:
            snapshot = asdict(self._config)
            snapshot["last_discovered_devices"] = [device.to_dict() for device in self._config.last_discovered_devices]
            snapshot["auto_discover"] = is_auto_bridge_host(self._config.host)
            snapshot["resolved_host"] = self._config.last_connected_host or None
            return snapshot

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
        request = {"operation": operation.strip(), "params": params or {}}
        return self._call_with_discovery(request, fallback_operation=operation.strip())

    def describe(self) -> dict[str, Any]:
        return self.call_operation("bridge.describe").require_ok().to_dict()

    def ping(self) -> dict[str, Any]:
        return self.call_operation("bridge.ping").require_ok().to_dict()

    def target_set_pid(self, pid: int) -> dict[str, Any]:
        return self.call_operation("target.pid.set", {"pid": pid}).require_ok().to_dict()

    def target_attach_package(self, package_name: str) -> dict[str, Any]:
        return self.call_operation("target.attach.package", {"package_name": package_name}).require_ok().to_dict()

    def target_current(self) -> dict[str, Any]:
        return self.call_operation("target.pid.current").require_ok().to_dict()

    def memory_regions(self) -> dict[str, Any]:
        return self.call_operation("memory.info.full").require_ok().to_dict()

    def module_address(self, module_name: str, segment_index: int = 0, which: str = "start") -> dict[str, Any]:
        return self.call_operation(
            "module.resolve",
            {
                "module_name": module_name,
                "segment_index": segment_index,
                "which": which.strip().lower(),
            },
        ).require_ok().to_dict()

    def memory_scan_start(
        self,
        value_type: str,
        mode: str,
        value: str = "",
        range_max: str = "",
    ) -> dict[str, Any]:
        params: dict[str, Any] = {
            "value_type": value_type.strip().lower(),
            "mode": mode.strip().lower(),
        }
        if str(value).strip():
            params["value"] = value
        if str(range_max).strip():
            params["range_max"] = range_max
        return self.call_operation("scan.start", params).require_ok().to_dict()

    def memory_scan_refine(
        self,
        value_type: str,
        mode: str,
        value: str = "",
        range_max: str = "",
    ) -> dict[str, Any]:
        params: dict[str, Any] = {
            "value_type": value_type.strip().lower(),
            "mode": mode.strip().lower(),
        }
        if str(value).strip():
            params["value"] = value
        if str(range_max).strip():
            params["range_max"] = range_max
        return self.call_operation("scan.refine", params).require_ok().to_dict()

    def memory_scan_results(self, start: int = 0, count: int = 100, value_type: str = "i32") -> dict[str, Any]:
        return self.call_operation(
            "scan.page",
            {
                "start": start,
                "count": count,
                "value_type": value_type.strip().lower(),
            },
        ).require_ok().to_dict()

    def memory_view_open(self, address: int | str, view_format: str = "hex") -> dict[str, Any]:
        return self.call_operation(
            "viewer.open",
            {"address": format_address(address), "view_format": normalize_view_format(view_format)},
        ).require_ok().to_dict()

    def memory_view_move(self, lines: int, step: int | None = None) -> dict[str, Any]:
        params: dict[str, Any] = {"lines": lines}
        if step is not None:
            params["step"] = step
        return self.call_operation("viewer.move", params).require_ok().to_dict()

    def memory_view_offset(self, offset: str) -> dict[str, Any]:
        offset_text = str(offset).strip()
        if not offset_text:
            raise ValueError("offset must not be empty")
        return self.call_operation("viewer.offset", {"offset": offset_text}).require_ok().to_dict()

    def memory_view_set_format(self, view_format: str) -> dict[str, Any]:
        return self.call_operation(
            "viewer.set_format",
            {"view_format": normalize_view_format(view_format)},
        ).require_ok().to_dict()

    def memory_view_read(self) -> dict[str, Any]:
        return self.call_operation("viewer.snapshot").require_ok().to_dict()

    def breakpoint_list(self) -> dict[str, Any]:
        return self.call_operation("breakpoint.info").require_ok().to_dict()

    def breakpoint_set(self, address: int | str, bp_type: int, bp_scope: int, length: int) -> dict[str, Any]:
        return self.call_operation(
            "breakpoint.set",
            {
                "address": format_address(address),
                "bp_type": bp_type,
                "bp_scope": bp_scope,
                "length": length,
            },
        ).require_ok().to_dict()

    def breakpoint_clear_all(self) -> dict[str, Any]:
        return self.call_operation("breakpoint.clear").require_ok().to_dict()

    def breakpoint_record_remove(self, index: int) -> dict[str, Any]:
        return self.call_operation("breakpoint.record.remove", {"index": index}).require_ok().to_dict()

    def breakpoint_record_update(self, index: int, field: str, value: int | str) -> dict[str, Any]:
        normalized_value = format_address(value) if isinstance(value, int) else str(value).strip()
        return self.call_operation(
            "breakpoint.record.update",
            {"index": index, "field": field.strip(), "value": normalized_value},
        ).require_ok().to_dict()

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
            except (OSError, BridgeError, json.JSONDecodeError) as exc:
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
            if self._session is None or self._session_endpoint != endpoint:
                self._close_session_locked()
                self._session = AndroidBridgeSession(
                    timeout_seconds=timeout_seconds,
                    max_response_bytes=self._max_response_bytes,
                )
                self._session_endpoint = endpoint
            session = self._session

        if session is None:
            raise BridgeConnectionError("bridge session initialize failed")

        try:
            if not session.is_connected():
                session.connect(host, port)
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
            return response
        except (OSError, BridgeError, json.JSONDecodeError):
            with self._lock:
                if self._session is session:
                    self._close_session_locked()
            raise
