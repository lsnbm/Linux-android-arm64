#!/usr/bin/env python3
from __future__ import annotations

"""Unified Windows-side bridge facade used by MCP and the desktop UI."""

import concurrent.futures
import http.client
import ipaddress
import json
import os
import re
import socket
import subprocess
import threading
import urllib.parse
from dataclasses import dataclass, field
from typing import Any

DEFAULT_ANDROID_HOST = os.getenv("ANDROID_HTTP_HOST", "auto").strip() or "auto"
DEFAULT_ANDROID_PORT = int(os.getenv("ANDROID_HTTP_PORT", "9494"))
DEFAULT_ANDROID_TIMEOUT_SECONDS = float(os.getenv("ANDROID_HTTP_TIMEOUT", "4"))
MIN_HTTPS_TIMEOUT_SECONDS = 15.0
LAN_DISCOVERY_MAX_WORKERS = 24
LAN_DISCOVERY_PING_TIMEOUT_MS = 150
MAX_HTTP_RESPONSE_BYTES = 16 * 1024 * 1024
AUTO_HOST_TOKENS = {"", "auto", "*"}
VIEWER_FORMAT_TOKENS = {"hexadecimal", "hex", "i8", "i16", "i32", "i64", "f32", "f64", "disasm"}
SCAN_HISTORY_MODES = {"inc", "dec", "changed", "unchanged"}
SCAN_VALUE_MODES = {"eq", "gt", "lt", "range", "pointer", "string"}
SCAN_MODE_ALIASES = {
    "equal": "eq",
    "greater": "gt",
    "less": "lt",
    "increased": "inc",
    "decreased": "dec",
    "chg": "changed",
    "unchg": "unchanged",
    "ptr": "pointer",
    "str": "string",
}
VALUE_TYPE_ALIASES = {
    "i8": "i8",
    "int8": "i8",
    "i16": "i16",
    "int16": "i16",
    "i32": "i32",
    "int32": "i32",
    "i64": "i64",
    "int64": "i64",
    "f32": "f32",
    "float": "f32",
    "f64": "f64",
    "double": "f64",
}
VALUE_TYPE_LABELS = {
    "i8": "I8",
    "i16": "I16",
    "i32": "I32",
    "i64": "I64",
    "f32": "Float",
    "f64": "Double",
}
SAVED_VALUE_KINDS = {"numeric", "pointer", "text"}


class BridgeError(RuntimeError):
    """Base error for bridge transport and protocol failures."""


class BridgeConnectionError(BridgeError):
    """Raised when the Android HTTP bridge cannot be reached."""


class BridgeRequestOutcomeUnknown(BridgeConnectionError):
    """Raised when a sent request may have executed but no response was received."""


class BridgeProtocolError(BridgeError):
    """Raised when the Android HTTP bridge returns an invalid payload."""


@dataclass(frozen=True)
class LanDevice:
    host: str
    mac: str

    def to_dict(self) -> dict[str, str]:
        return {"host": self.host, "mac": self.mac}


@dataclass(frozen=True)
class _BridgeEndpoint:
    scheme: str
    host: str
    port: int
    rpc_path: str = "/api/rpc"

    @property
    def display_url(self) -> str:
        default_port = 443 if self.scheme == "https" else 80
        authority = self.host if self.port == default_port else f"{self.host}:{self.port}"
        return f"{self.scheme}://{authority}"


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
        raise ValueError("view_format must be one of: hexadecimal, hex, i8, i16, i32, i64, f32, f64, disasm")
    return token


def normalize_scan_mode(mode: str) -> str:
    token = str(mode).strip().lower()
    return SCAN_MODE_ALIASES.get(token, token)


def normalize_value_type(value_type: str) -> str:
    token = str(value_type).strip().lower()
    normalized = VALUE_TYPE_ALIASES.get(token)
    if normalized is None:
        raise ValueError("value_type must be one of: i8, i16, i32, i64, f32, f64")
    return normalized


def normalize_saved_kind(value_kind: str) -> str:
    token = str(value_kind or "numeric").strip().lower()
    aliases = {"number": "numeric", "ptr": "pointer", "string": "text"}
    token = aliases.get(token, token)
    if token not in SAVED_VALUE_KINDS:
        raise ValueError("value_kind must be one of: numeric, pointer, text")
    return token


def parse_bool(value: Any) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    token = str(value).strip().lower()
    if token in {"true", "1", "yes", "on"}:
        return True
    if token in {"false", "0", "no", "off", ""}:
        return False
    raise ValueError(f"invalid boolean value: {value}")


def build_saved_add_params(
    address: int | str,
    value_type: str,
    *,
    value_kind: str = "numeric",
    text_length: int = 64,
    note: str = "",
) -> dict[str, Any]:
    kind = normalize_saved_kind(value_kind)
    if kind == "pointer":
        type_token = "i64"
    elif kind == "text":
        type_token = "i8"
    else:
        type_token = normalize_value_type(value_type)
    if text_length < 1 or text_length > 256:
        raise ValueError("text_length must be in 1..256")
    return {
        "address": format_address(address),
        "value_type": type_token,
        "value_kind": kind,
        "text_length": text_length,
        "note": str(note),
    }


def build_scan_params(value_type: str, mode: str, value: str = "", range_max: str = "", *, is_first: bool) -> dict[str, Any]:
    mode_token = normalize_scan_mode(mode)
    value_token = str(value).strip()
    range_token = str(range_max).strip()
    if is_first and mode_token in SCAN_HISTORY_MODES:
        raise ValueError("first scan cannot use inc, dec, changed, or unchanged")
    if not is_first and mode_token == "unknown":
        raise ValueError("unknown initial value can only be used for the first scan")
    if mode_token in SCAN_VALUE_MODES and not value_token:
        raise ValueError(f"value is required for mode '{mode_token}'")
    if mode_token == "range" and not range_token:
        raise ValueError("range_max is required for mode 'range'")

    params: dict[str, Any] = {"mode": mode_token}
    if mode_token == "pointer":
        params["value_type"] = "i64"
    elif mode_token != "string":
        params["value_type"] = normalize_value_type(value_type)
    if mode_token in SCAN_VALUE_MODES:
        params["value"] = value_token
    if mode_token == "range":
        params["range_max"] = range_token
    return params


def normalize_breakpoint_points(points: list[dict[str, Any]]) -> list[dict[str, Any]]:
    normalized: list[dict[str, Any]] = []
    for point in points:
        if not isinstance(point, dict):
            raise ValueError("points must contain objects with address, bp_type, bp_scope, length")
        length = int(point["length"])
        if length < 1 or length > 8:
            raise ValueError("breakpoint length must be in 1..8")
        normalized.append(
            {
                "address": format_address(point["address"]),
                "bp_type": point["bp_type"],
                "bp_scope": point["bp_scope"],
                "length": length,
            }
        )
    if not normalized:
        raise ValueError("points must not be empty")
    return normalized


@dataclass(frozen=True)
class SavedAddressState:
    address: int
    address_hex: str
    value_type: str
    value_type_label: str
    value_kind: str
    text_length: int
    note: str
    value: str
    locked: bool
    lock_value: str

    @classmethod
    def from_dict(cls, payload: dict[str, Any]) -> "SavedAddressState":
        value_type = normalize_value_type(str(payload.get("value_type", "")))
        value_kind = normalize_saved_kind(str(payload.get("value_kind", "numeric")))
        address = int(payload.get("address", 0))
        address_hex = str(payload.get("address_hex") or format_address(address))
        return cls(
            address=address,
            address_hex=address_hex,
            value_type=value_type,
            value_type_label=str(payload.get("value_type_label") or VALUE_TYPE_LABELS[value_type]),
            value_kind=value_kind,
            text_length=max(1, min(256, int(payload.get("text_length", 64)))),
            note=str(payload.get("note", "")),
            value=str(payload.get("value", "")),
            locked=parse_bool(payload.get("locked", False)),
            lock_value=str(payload.get("lock_value", "")),
        )


def parse_saved_states(data: dict[str, Any]) -> list[SavedAddressState]:
    raw_items = data.get("items", [])
    if not isinstance(raw_items, list):
        raise ValueError("saved state response items must be a list")
    return [SavedAddressState.from_dict(item) for item in raw_items if isinstance(item, dict)]


class AndroidProtocolMixin:
    def scan_start(self, value_type: str, mode: str, value: str = "", range_max: str = "") -> BridgeResponse:
        return self.call_operation("scan.start", build_scan_params(value_type, mode, value, range_max, is_first=True))

    def scan_refine(self, value_type: str, mode: str, value: str = "", range_max: str = "") -> BridgeResponse:
        return self.call_operation("scan.refine", build_scan_params(value_type, mode, value, range_max, is_first=False))

    def breakpoint_set(self, mode: str, points: list[dict[str, Any]]) -> BridgeResponse:
        mode_token = str(mode).strip().lower()
        if mode_token not in {"hwbp", "ptebp", "stepbp"}:
            raise ValueError("mode must be one of: hwbp, ptebp, stepbp")
        return self.call_operation("breakpoint.set", {"mode": mode_token, "points": normalize_breakpoint_points(points)})

    def saved_list(self) -> BridgeResponse:
        return self.call_operation("saved.list")

    def saved_add(
        self,
        address: int | str,
        value_type: str,
        *,
        value_kind: str = "numeric",
        text_length: int = 64,
        note: str = "",
    ) -> BridgeResponse:
        return self.call_operation(
            "saved.add",
            build_saved_add_params(
                address,
                value_type,
                value_kind=value_kind,
                text_length=text_length,
                note=note,
            ),
        )

    def saved_remove(self, address: int | str) -> BridgeResponse:
        return self.call_operation("saved.remove", {"address": format_address(address)})

    def saved_write(self, address: int | str, value: str) -> BridgeResponse:
        return self.call_operation("saved.write", {"address": format_address(address), "value": str(value)})

    def saved_set_note(self, address: int | str, note: str) -> BridgeResponse:
        return self.call_operation("saved.note.set", {"address": format_address(address), "note": str(note)})

    def saved_set_locked(self, address: int | str, locked: bool, value: str = "") -> BridgeResponse:
        params: dict[str, Any] = {
            "address": format_address(address),
            "locked": bool(locked),
            "value": str(value).strip() if locked else "",
        }
        return self.call_operation("saved.lock.set", params)

    def saved_offset(self, offset: str) -> BridgeResponse:
        return self.call_operation("saved.offset", {"offset": str(offset).strip()})

    def saved_clear(self) -> BridgeResponse:
        return self.call_operation("saved.clear")

    def viewer_open(self, address: int | str, view_format: str = "hexadecimal") -> BridgeResponse:
        return self.call_operation(
            "viewer.open",
            {"address": format_address(address), "view_format": normalize_view_format(view_format)},
        )


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
        raise BridgeProtocolError("android HTTP response is not a JSON object")

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


class AndroidHttpBridge(AndroidProtocolMixin):
    """HTTP endpoint used by interactive clients such as the Windows UI."""

    def __init__(
        self,
        *,
        timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS,
    ) -> None:
        self.timeout_seconds = float(timeout_seconds)
        self._io_lock = threading.Lock()
        self._state_lock = threading.RLock()
        self._endpoint: _BridgeEndpoint | None = None

    @property
    def host(self) -> str:
        endpoint = self._current_endpoint()
        return endpoint.host if endpoint is not None else ""

    @property
    def port(self) -> int:
        endpoint = self._current_endpoint()
        return endpoint.port if endpoint is not None else DEFAULT_ANDROID_PORT

    @property
    def url(self) -> str:
        endpoint = self._current_endpoint()
        return endpoint.display_url if endpoint is not None else ""

    def _current_endpoint(self) -> _BridgeEndpoint | None:
        with self._state_lock:
            return self._endpoint

    @staticmethod
    def _parse_endpoint(host: str, port: int) -> _BridgeEndpoint:
        host_text = host.strip()
        if not host_text:
            raise BridgeConnectionError("host must not be empty")

        if "://" not in host_text:
            if port <= 0 or port > 65535:
                raise BridgeConnectionError("port must be in 1..65535")
            return _BridgeEndpoint(scheme="http", host=host_text, port=port)

        parsed = urllib.parse.urlsplit(host_text)
        scheme = parsed.scheme.lower()
        if scheme not in {"http", "https"} or not parsed.hostname:
            raise BridgeConnectionError("public endpoint must be an http:// or https:// URL")
        if parsed.query or parsed.fragment:
            raise BridgeConnectionError("public endpoint must not contain query or fragment")

        endpoint_port = parsed.port or (443 if scheme == "https" else 80)
        path = parsed.path.rstrip("/")
        rpc_path = path if path.endswith("/api/rpc") else f"{path}/api/rpc"
        return _BridgeEndpoint(scheme=scheme, host=parsed.hostname, port=endpoint_port, rpc_path=rpc_path)

    def is_connected(self) -> bool:
        return self._current_endpoint() is not None

    def connect(self, host: str, port: int) -> None:
        with self._state_lock:
            self._endpoint = self._parse_endpoint(host, port)

    def disconnect(self) -> None:
        with self._state_lock:
            self._endpoint = None

    def request(self, request_obj: dict[str, Any]) -> BridgeResponse:
        with self._io_lock:
            endpoint = self._current_endpoint()
            if endpoint is None:
                raise BridgeConnectionError("bridge endpoint is not configured")

            request_body = json.dumps(request_obj, ensure_ascii=False).encode("utf-8")
            operation = str(request_obj.get("operation") or "")
            timeout_seconds = max(self.timeout_seconds, MIN_HTTPS_TIMEOUT_SECONDS) if endpoint.scheme == "https" else self.timeout_seconds
            connection_class = http.client.HTTPSConnection if endpoint.scheme == "https" else http.client.HTTPConnection
            connection = connection_class(endpoint.host, endpoint.port, timeout=timeout_seconds)
            try:
                connection.request(
                    "POST",
                    endpoint.rpc_path,
                    body=request_body,
                    headers={
                        "Content-Type": "application/json",
                        "Accept": "application/json",
                        "User-Agent": "LS-KTool-Windows-Bridge/1",
                    },
                )
                response = connection.getresponse()
                content_length_text = response.getheader("Content-Length")
                if content_length_text:
                    try:
                        if int(content_length_text) > MAX_HTTP_RESPONSE_BYTES:
                            raise BridgeProtocolError("android HTTP response exceeds the maximum size")
                    except ValueError as exc:
                        raise BridgeProtocolError("android HTTP Content-Length is invalid") from exc
                payload = response.read(MAX_HTTP_RESPONSE_BYTES + 1)
                if len(payload) > MAX_HTTP_RESPONSE_BYTES:
                    raise BridgeProtocolError("android HTTP response exceeds the maximum size")
            except (socket.timeout, TimeoutError) as exc:
                hint = "；Quick Tunnel 重启后域名会变化，请核对 /sdcard/log.txt 中最新公网地址" if endpoint.scheme == "https" else ""
                raise BridgeRequestOutcomeUnknown(
                    f"请求 {operation or '/api/rpc'} 等待 {timeout_seconds:g} 秒仍未收到响应，"
                    f"当前地址：{endpoint.display_url}{hint}；请求可能已在 Android 端执行"
                ) from exc
            except (ConnectionError, OSError, http.client.HTTPException) as exc:
                raise BridgeConnectionError(f"HTTP request to {endpoint.display_url} failed: {exc}") from exc
            finally:
                connection.close()

            payload_text = payload.decode("utf-8", errors="replace").strip()
            try:
                response_obj = json.loads(payload_text)
            except json.JSONDecodeError as exc:
                raise BridgeProtocolError(
                    f"android HTTP response is not valid JSON (status {response.status}): {payload_text}"
                ) from exc
            if response.status < 200 or response.status >= 300:
                error_text = str(response_obj.get("error", response.reason)) if isinstance(response_obj, dict) else response.reason
                raise BridgeConnectionError(f"android HTTP request failed with status {response.status}: {error_text}")

            return _coerce_bridge_response(
                response_obj,
                fallback_operation=str(request_obj.get("operation") or ""),
                connection={
                    "host": endpoint.display_url,
                    "resolved_host": endpoint.host,
                    "port": endpoint.port,
                    "scheme": endpoint.scheme,
                    "timeout_seconds": timeout_seconds,
                },
            )

    def call_operation(self, operation: str, params: dict[str, Any] | None = None) -> BridgeResponse:
        return self.request({"operation": operation.strip(), "params": params or {}})


class AndroidHttpClient(AndroidProtocolMixin):
    """Configurable HTTP client used by MCP, scripts, and non-interactive flows."""

    def __init__(
        self,
        *,
        host: str = DEFAULT_ANDROID_HOST,
        port: int = DEFAULT_ANDROID_PORT,
        timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS,
    ) -> None:
        self._lock = threading.RLock()
        self._host = normalize_bridge_host(host)
        self._port = int(port)
        self._timeout_seconds = float(timeout_seconds)
        self._last_host = ""
        self._devices: list[LanDevice] = []
        self._target_pid: int | None = None
        self._target_host: str | None = None

    def connection_state(self) -> dict[str, Any]:
        with self._lock:
            return self._snapshot_locked()

    def _snapshot_locked(self) -> dict[str, Any]:
        return {
            "host": self._host,
            "port": self._port,
            "timeout_seconds": self._timeout_seconds,
            "last_connected_host": self._last_host,
            "last_discovered_devices": [device.to_dict() for device in self._devices],
            "auto_discover": is_auto_bridge_host(self._host),
            "resolved_host": self._last_host or None,
            "target_pid": self._target_pid,
            "target_host": self._target_host,
        }

    def configure(
        self,
        *,
        host: str | None = None,
        port: int | None = None,
        timeout_seconds: float | None = None,
    ) -> dict[str, Any]:
        with self._lock:
            if host is not None:
                normalized_host = normalize_bridge_host(host)
                if normalized_host != self._host:
                    self._host = normalized_host
                    self._last_host = ""
                    self._devices = []
                    self._target_pid = None
                    self._target_host = None
            if port is not None:
                if port <= 0 or port > 65535:
                    raise ValueError("port must be in 1..65535")
                if int(port) != self._port:
                    self._port = int(port)
                    self._target_pid = None
                    self._target_host = None
            if timeout_seconds is not None:
                if timeout_seconds <= 0:
                    raise ValueError("timeout_seconds must be > 0")
                self._timeout_seconds = float(timeout_seconds)
            return self._snapshot_locked()

    def discover(self) -> dict[str, Any]:
        with self._lock:
            host = self._host
            last_host = self._last_host
        auto_discover = is_auto_bridge_host(host)
        devices = discover_lan_devices() if auto_discover else []
        with self._lock:
            self._devices = devices
            snapshot = self._snapshot_locked()
        snapshot["candidates"] = _unique_hosts([last_host], [device.host for device in devices]) if auto_discover else [host]
        return snapshot

    def call_operation(self, operation: str, params: dict[str, Any] | None = None) -> BridgeResponse:
        op = operation.strip()
        request = {"operation": op, "params": params or {}}
        with self._lock:
            configured_host = self._host
            port = self._port
            timeout_seconds = self._timeout_seconds
            last_host = self._last_host

        auto_discover = is_auto_bridge_host(configured_host)
        hosts = _unique_hosts([last_host]) if auto_discover else [configured_host]
        errors: list[str] = []

        def try_hosts(candidates: list[str]) -> BridgeResponse | None:
            for candidate in candidates:
                try:
                    response = self._request_host(candidate, port, timeout_seconds, configured_host, request)
                    with self._lock:
                        self._last_host = candidate
                    return response
                except BridgeRequestOutcomeUnknown:
                    raise
                except BridgeConnectionError as exc:
                    errors.append(f"{candidate}:{port} -> {exc}")
            return None

        response = try_hosts(hosts)
        if response is not None:
            return response

        if auto_discover:
            devices = discover_lan_devices()
            with self._lock:
                self._devices = devices
            response = try_hosts([device.host for device in devices if device.host not in set(hosts)])
            if response is not None:
                return response

        if errors:
            raise BridgeConnectionError("failed to reach Android HTTP bridge candidates: " + "; ".join(errors))
        raise BridgeConnectionError("failed to discover any Android HTTP bridge candidates")

    def _request_host(
        self,
        host: str,
        port: int,
        timeout_seconds: float,
        configured_host: str,
        request: dict[str, Any],
    ) -> BridgeResponse:
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

            bridge = AndroidHttpBridge(timeout_seconds=timeout_seconds)
            bridge.connect(host, port)
            response = bridge.request(request)
            response.connection.update(host=configured_host, url=bridge.url, auto_discover=is_auto_bridge_host(configured_host))
            if response.ok and operation in {"target.select", "target.attach", "target.get"} and isinstance(response.data, dict) and "pid" in response.data:
                try:
                    self._target_pid = int(str(response.data["pid"]), 0)
                    self._target_host = host
                except (TypeError, ValueError):
                    pass
            response.connection["target_pid"] = self._target_pid
            return response
