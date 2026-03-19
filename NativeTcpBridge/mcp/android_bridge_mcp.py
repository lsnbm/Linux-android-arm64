#!/usr/bin/env python3
from __future__ import annotations

import argparse
import concurrent.futures
import ipaddress
import json
import os
import re
import socket
import subprocess
import sys
import threading
from dataclasses import asdict, dataclass, field
from html import escape
from typing import Any

from mcp.server.fastmcp import FastMCP


DEFAULT_ANDROID_HOST = os.getenv("ANDROID_TCP_HOST", "auto").strip() or "auto"
DEFAULT_ANDROID_PORT = int(os.getenv("ANDROID_TCP_PORT", "9494"))
DEFAULT_ANDROID_TIMEOUT_SECONDS = float(os.getenv("ANDROID_TCP_TIMEOUT", "8"))
DEFAULT_MCP_BIND_HOST = os.getenv("ANDROID_MCP_BIND_HOST", "127.0.0.1").strip() or "127.0.0.1"
DEFAULT_MCP_BIND_PORT = int(os.getenv("ANDROID_MCP_BIND_PORT", "13337"))
DEFAULT_MCP_PATH = os.getenv("ANDROID_MCP_PATH", "/mcp")
DEFAULT_MCP_CONFIG_PATH = os.getenv("ANDROID_MCP_CONFIG_PATH", "/config.html")
MAX_RESPONSE_BYTES = 8 * 1024 * 1024
LAN_DISCOVERY_MAX_WORKERS = 24
LAN_DISCOVERY_PING_TIMEOUT_MS = 150
AUTO_HOST_TOKENS = {"", "auto", "*"}
VIEWER_FORMAT_TOKENS = {"hex", "hex16", "hex64", "i8", "i16", "i32", "i64", "f32", "f64", "disasm"}


def _parse_message_pairs(message: str) -> dict[str, str]:
    pairs: dict[str, str] = {}
    for token in message.split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        pairs[key.strip()] = value.strip()
    return pairs


def _normalize_arg(value: Any) -> str:
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return str(value)
    return str(value).strip()


def _format_address(value: int | str) -> str:
    if isinstance(value, int):
        return f"0x{value:X}"
    return str(value).strip()


def _normalize_view_format(view_format: str) -> str:
    token = str(view_format).strip().lower()
    if token not in VIEWER_FORMAT_TOKENS:
        raise ValueError("view_format must be one of: hex, hex16, hex64, i8, i16, i32, i64, f32, f64, disasm")
    return token


def _normalize_bridge_host(host: str | None) -> str:
    normalized = "" if host is None else str(host).strip()
    return normalized or "auto"


def _is_auto_bridge_host(host: str) -> bool:
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


def _discover_lan_hosts() -> list[str]:
    local_ips = _collect_local_ipv4()
    if not local_ips:
        return []

    targets, local_set = _collect_subnet_targets(local_ips)
    if targets:
        with concurrent.futures.ThreadPoolExecutor(max_workers=LAN_DISCOVERY_MAX_WORKERS) as pool:
            list(pool.map(_ping_host, sorted(targets)))

    hosts: list[str] = []
    for ip_text in _read_arp_table():
        if ip_text in local_set:
            continue
        if targets and ip_text not in targets:
            continue
        hosts.append(ip_text)

    hosts.sort(key=lambda item: int(ipaddress.IPv4Address(item)))
    return hosts


@dataclass
class BridgeConfig:
    host: str = DEFAULT_ANDROID_HOST
    port: int = DEFAULT_ANDROID_PORT
    timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS
    last_connected_host: str = ""
    last_discovered_hosts: list[str] = field(default_factory=list)


class AndroidTcpBridge:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._config = BridgeConfig()

    def configure(
        self,
        *,
        host: str | None = None,
        port: int | None = None,
        timeout_seconds: float | None = None,
    ) -> dict[str, Any]:
        with self._lock:
            if host is not None:
                self._config.host = _normalize_bridge_host(host)
                self._config.last_connected_host = ""
                self._config.last_discovered_hosts = []
            if port is not None:
                if port <= 0 or port > 65535:
                    raise ValueError("port must be in 1..65535")
                self._config.port = int(port)
            if timeout_seconds is not None:
                if timeout_seconds <= 0:
                    raise ValueError("timeout_seconds must be > 0")
                self._config.timeout_seconds = float(timeout_seconds)
            return self._snapshot_locked()

    def current_config(self) -> dict[str, Any]:
        with self._lock:
            return self._snapshot_locked()

    def discover(self) -> dict[str, Any]:
        with self._lock:
            config = BridgeConfig(
                host=self._config.host,
                port=self._config.port,
                timeout_seconds=self._config.timeout_seconds,
                last_connected_host=self._config.last_connected_host,
                last_discovered_hosts=list(self._config.last_discovered_hosts),
            )

        lan_hosts = _discover_lan_hosts() if _is_auto_bridge_host(config.host) else []
        preferred_hosts = _unique_hosts([config.last_connected_host] if _is_auto_bridge_host(config.host) else [])
        candidates = _unique_hosts(preferred_hosts, lan_hosts if _is_auto_bridge_host(config.host) else [config.host])

        with self._lock:
            self._config.last_discovered_hosts = list(lan_hosts)
            snapshot = self._snapshot_locked()

        snapshot["candidates"] = candidates
        return snapshot

    def call(self, command: str, *args: Any) -> dict[str, Any]:
        if not command.strip():
            raise ValueError("command must not be empty")

        request = {
            "command": command.strip(),
            "args": [_normalize_arg(arg) for arg in args if _normalize_arg(arg)],
        }

        with self._lock:
            config = BridgeConfig(
                host=self._config.host,
                port=self._config.port,
                timeout_seconds=self._config.timeout_seconds,
                last_connected_host=self._config.last_connected_host,
                last_discovered_hosts=list(self._config.last_discovered_hosts),
            )

        response, resolved_host = self._call_with_discovery(config, request)
        if not response.get("ok", False):
            error_text = str(response.get("error", "unknown bridge error"))
            raise RuntimeError(error_text)

        message = str(response.get("message", ""))
        result: dict[str, Any] = {
            "ok": True,
            "command": str(response.get("command", command)),
            "message": message,
            "pairs": _parse_message_pairs(message),
            "data": response.get("data"),
            "raw": response,
            "connection": {
                "host": config.host,
                "resolved_host": resolved_host,
                "port": config.port,
                "timeout_seconds": config.timeout_seconds,
                "auto_discover": _is_auto_bridge_host(config.host),
            },
        }
        return result

    def _snapshot_locked(self) -> dict[str, Any]:
        snapshot = asdict(self._config)
        snapshot["auto_discover"] = _is_auto_bridge_host(self._config.host)
        snapshot["resolved_host"] = self._config.last_connected_host or None
        return snapshot

    def _call_with_discovery(
        self,
        config: BridgeConfig,
        request: dict[str, Any],
    ) -> tuple[dict[str, Any], str]:
        errors: list[str] = []
        immediate_hosts = [config.host]
        if _is_auto_bridge_host(config.host):
            immediate_hosts = _unique_hosts([config.last_connected_host])

        response, resolved_host = self._try_hosts(immediate_hosts, config.port, config.timeout_seconds, request, errors)
        if response is not None and resolved_host is not None:
            self._remember_success(resolved_host)
            return response, resolved_host

        lan_hosts: list[str] = []
        if _is_auto_bridge_host(config.host):
            lan_hosts = _discover_lan_hosts()
            self._remember_discovery(lan_hosts)
            remaining_hosts = [host for host in lan_hosts if host not in set(immediate_hosts)]
            response, resolved_host = self._try_hosts(
                remaining_hosts,
                config.port,
                config.timeout_seconds,
                request,
                errors,
            )
            if response is not None and resolved_host is not None:
                self._remember_success(resolved_host)
                return response, resolved_host

        if errors:
            raise RuntimeError("failed to reach Android tcp_server candidates: " + "; ".join(errors))
        raise RuntimeError("failed to discover any Android tcp_server candidates")

    def _remember_discovery(self, lan_hosts: list[str]) -> None:
        with self._lock:
            self._config.last_discovered_hosts = list(lan_hosts)

    def _remember_success(self, host: str) -> None:
        with self._lock:
            self._config.last_connected_host = host

    def _try_hosts(
        self,
        hosts: list[str],
        port: int,
        timeout_seconds: float,
        request: dict[str, Any],
        errors: list[str],
    ) -> tuple[dict[str, Any] | None, str | None]:
        for host in hosts:
            try:
                return self._call_once(host, port, timeout_seconds, request), host
            except (OSError, RuntimeError, json.JSONDecodeError) as exc:
                errors.append(f"{host}:{port} -> {exc}")
        return None, None

    @staticmethod
    def _call_once(
        host: str,
        port: int,
        timeout_seconds: float,
        request: dict[str, Any],
    ) -> dict[str, Any]:
        with socket.create_connection((host, port), timeout=timeout_seconds) as sock:
            sock.settimeout(timeout_seconds)
            request_text = json.dumps(request, ensure_ascii=False) + "\n"
            sock.sendall(request_text.encode("utf-8"))

            buffer = b""
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    raise RuntimeError("android tcp server closed the connection before replying")
                buffer += chunk
                if len(buffer) > MAX_RESPONSE_BYTES:
                    raise RuntimeError("android tcp response is too large")
                split_index = buffer.find(b"\n")
                if split_index == -1:
                    continue
                payload = buffer[:split_index].decode("utf-8", errors="replace").strip()
                if not payload:
                    buffer = buffer[split_index + 1 :]
                    continue
                response = json.loads(payload)
                if not isinstance(response, dict):
                    raise RuntimeError("android tcp response is not a JSON object")
                return response


bridge = AndroidTcpBridge()
mcp = FastMCP(
    "NativeTcpBridge Android MCP",
    host=DEFAULT_MCP_BIND_HOST,
    port=DEFAULT_MCP_BIND_PORT,
    streamable_http_path=DEFAULT_MCP_PATH,
)


@mcp.resource("android://connection")
def android_connection() -> dict[str, Any]:
    """Return the current Android TCP connection settings used by this MCP server."""
    return bridge.current_config()


@mcp.tool()
def configure_android_bridge(
    host: str = DEFAULT_ANDROID_HOST,
    timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS,
) -> dict[str, Any]:
    """Configure the Android tcp_server target. Use host='auto' to scan the LAN for a reachable device."""
    return bridge.configure(host=host, timeout_seconds=timeout_seconds)


@mcp.tool()
def discover_android_bridges() -> dict[str, Any]:
    """Discover Android tcp_server candidates on the LAN and show the current TCP bridge state."""
    return bridge.discover()


@mcp.tool()
def android_bridge_ping() -> dict[str, Any]:
    """Check whether the currently configured Android tcp_server is reachable."""
    return bridge.call("ping")


@mcp.tool()
def android_target_set_pid(pid: int) -> dict[str, Any]:
    """Bind all scan, viewer, and breakpoint operations to a known PID."""
    return bridge.call("pid.set", pid)


@mcp.tool()
def android_target_attach_package(package_name: str) -> dict[str, Any]:
    """Resolve a package name to PID and make that process the current target."""
    return bridge.call("pid.attach", package_name)


@mcp.tool()
def android_target_current() -> dict[str, Any]:
    """Read the current target process bound inside the Android tcp_server."""
    return bridge.call("pid.current")


@mcp.tool()
def android_memory_regions() -> dict[str, Any]:
    """Fetch the full module and memory region map for the current target process."""
    return bridge.call("memory.info.full")


@mcp.tool()
def android_module_address(module_name: str, segment_index: int = 0, which: str = "start") -> dict[str, Any]:
    """Resolve a module segment start or end address from the current target process."""
    which_token = which.strip().lower()
    if which_token not in {"start", "end"}:
        raise ValueError("which must be 'start' or 'end'")
    return bridge.call("module.addr", module_name, segment_index, which_token)


@mcp.tool()
def android_memory_scan_start(
    value_type: str,
    mode: str,
    value: str = "",
    range_max: str = "",
) -> dict[str, Any]:
    """Start a new memory scan. Example: value_type='i32', mode='eq', value='1234'."""
    type_token = value_type.strip().lower()
    mode_token = mode.strip().lower()
    args: list[Any] = [type_token, mode_token]
    if mode_token != "unknown":
        if not str(value).strip():
            raise ValueError("value is required unless mode is 'unknown'")
        args.append(value)
    if str(range_max).strip():
        args.append(range_max)
    return bridge.call("scan.first", *args)


@mcp.tool()
def android_memory_scan_refine(
    value_type: str,
    mode: str,
    value: str = "",
    range_max: str = "",
) -> dict[str, Any]:
    """Refine the current memory scan result set."""
    type_token = value_type.strip().lower()
    mode_token = mode.strip().lower()
    args: list[Any] = [type_token, mode_token]
    if mode_token != "unknown":
        if not str(value).strip():
            raise ValueError("value is required unless mode is 'unknown'")
        args.append(value)
    if str(range_max).strip():
        args.append(range_max)
    return bridge.call("scan.next", *args)


@mcp.tool()
def android_memory_scan_results(start: int = 0, count: int = 100, value_type: str = "i32") -> dict[str, Any]:
    """Read one page of the current memory scan results."""
    if count <= 0 or count > 2000:
        raise ValueError("count must be in 1..2000")
    return bridge.call("scan.page", start, count, value_type.strip().lower())


@mcp.tool()
def android_memory_view_open(address: int | str, view_format: str = "hex") -> dict[str, Any]:
    """Open the memory viewer at an address. Use view_format='disasm' to request disassembly instead of raw hex bytes."""
    return bridge.call("viewer.open", _format_address(address), _normalize_view_format(view_format))


@mcp.tool()
def android_memory_view_move(lines: int, step: int | None = None) -> dict[str, Any]:
    """Move the current memory viewer window by lines."""
    if step is None:
        return bridge.call("viewer.move", lines)
    return bridge.call("viewer.move", lines, step)


@mcp.tool()
def android_memory_view_offset(offset: str) -> dict[str, Any]:
    """Move the current viewer base by an offset such as '+0x20' or '-0x10'."""
    offset_text = str(offset).strip()
    if not offset_text:
        raise ValueError("offset must not be empty")
    return bridge.call("viewer.offset", offset_text)


@mcp.tool()
def android_memory_view_set_format(view_format: str) -> dict[str, Any]:
    """Change the current viewer format. Use 'disasm' for disassembly, otherwise the viewer returns formatted memory values."""
    return bridge.call("viewer.format", _normalize_view_format(view_format))


@mcp.tool()
def android_memory_view_read() -> dict[str, Any]:
    """Read the current viewer snapshot. In disasm mode, the result is in data.disasm; in other modes, raw bytes remain in data.data_hex."""
    return bridge.call("viewer.get")


@mcp.tool()
def android_breakpoint_list() -> dict[str, Any]:
    """List the current hardware breakpoint state and saved breakpoint records."""
    return bridge.call("hwbp.info")


@mcp.tool()
def android_breakpoint_set(address: int | str, bp_type: int, bp_scope: int, length: int) -> dict[str, Any]:
    """Create a hardware breakpoint on the current target process."""
    return bridge.call("hwbp.set", _format_address(address), bp_type, bp_scope, length)


@mcp.tool()
def android_breakpoint_clear_all() -> dict[str, Any]:
    """Remove all active hardware breakpoints from the current target process."""
    return bridge.call("hwbp.remove")


@mcp.tool()
def android_breakpoint_record_remove(index: int) -> dict[str, Any]:
    """Remove one saved hardware breakpoint record by index."""
    return bridge.call("hwbp.record.remove", index)


@mcp.tool()
def android_breakpoint_record_update(index: int, field: str, value: int | str) -> dict[str, Any]:
    """Patch one field inside a saved hardware breakpoint record."""
    return bridge.call("hwbp.record.set", index, field.strip(), _format_address(value) if isinstance(value, int) else value)


def _normalize_http_path(path: str) -> str:
    normalized = str(path).strip() or "/mcp"
    if not normalized.startswith("/"):
        normalized = "/" + normalized
    if len(normalized) > 1:
        normalized = normalized.rstrip("/")
    return normalized or "/mcp"


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Expose the NativeTcpBridge Android bridge as an MCP server.",
    )
    parser.add_argument(
        "--mcp-host",
        default=DEFAULT_MCP_BIND_HOST,
        help="Bind host for the local MCP web server.",
    )
    parser.add_argument(
        "--mcp-port",
        type=int,
        default=DEFAULT_MCP_BIND_PORT,
        help="Bind port for the local MCP web server.",
    )
    parser.add_argument(
        "--mcp-path",
        default=DEFAULT_MCP_PATH,
        help="HTTP endpoint path for streamable-http clients.",
    )
    parser.add_argument(
        "--mcp-config-path",
        default=DEFAULT_MCP_CONFIG_PATH,
        help="Config page path for the local browser UI.",
    )
    parser.add_argument(
        "--android-host",
        default=DEFAULT_ANDROID_HOST,
        help="Target Android tcp_server host. Default is 'auto' for LAN discovery; the Android port is fixed at 9494 and this bridge uses raw TCP sockets only.",
    )
    parser.add_argument(
        "--android-timeout",
        type=float,
        default=DEFAULT_ANDROID_TIMEOUT_SECONDS,
        help="Timeout in seconds for Android tcp_server requests.",
    )
    return parser


def _format_http_endpoint(host: str, port: int, path: str) -> str:
    display_host = "127.0.0.1" if host == "0.0.0.0" else host
    return f"http://{display_host}:{port}{path}"

TOOL_META: dict[str, tuple[str, str, str]] = {
    "configure_android_bridge": ("Bridge Setup", "Set or update the Android tcp_server host.", '{"host":"auto","timeout_seconds":8}'),
    "discover_android_bridges": ("Bridge Setup", "Scan the LAN for Android tcp_server candidates when the host is unknown.", "{}"),
    "android_bridge_ping": ("Bridge Setup", "Check whether the current Android TCP target is reachable.", "{}"),
    "android_target_set_pid": ("Target Selection", "Bind the bridge to a known PID before scanning, viewing, or breakpoints.", '{"pid":1234}'),
    "android_target_attach_package": ("Target Selection", "Resolve a package name to PID and make it the current target process.", '{"package_name":"com.example.app"}'),
    "android_target_current": ("Target Selection", "Read the currently bound PID.", "{}"),
    "android_memory_regions": ("Target Selection", "Fetch the full memory map so you can enumerate module base addresses.", "{}"),
    "android_module_address": ("Target Selection", "Resolve a specific module segment start or end address directly.", '{"module_name":"libgame.so","segment_index":0,"which":"start"}'),
    "android_memory_scan_start": ("Memory Scan", "Start a fresh memory scan.", '{"value_type":"i32","mode":"eq","value":"1234"}'),
    "android_memory_scan_refine": ("Memory Scan", "Narrow an existing memory scan result set.", '{"value_type":"i32","mode":"eq","value":"1234"}'),
    "android_memory_scan_results": ("Memory Scan", "Page through scan hits after a first/next scan.", '{"start":0,"count":100,"value_type":"i32"}'),
    "android_memory_view_open": ("Memory View", "Open the memory viewer at an address. For assembly, set view_format to disasm.", '{"address":"0x12345678","view_format":"disasm"}'),
    "android_memory_view_move": ("Memory View", "Move the current viewer window.", '{"lines":16}'),
    "android_memory_view_offset": ("Memory View", "Move the current viewer base by a relative offset.", '{"offset":"+0x20"}'),
    "android_memory_view_set_format": ("Memory View", "Switch the viewer between hex, integer, float, and disassembly formats.", '{"view_format":"disasm"}'),
    "android_memory_view_read": ("Memory View", "Read the current viewer snapshot. In disasm mode, use data.disasm instead of data.data_hex.", "{}"),
    "android_breakpoint_list": ("Breakpoints", "Inspect current hardware breakpoint state.", "{}"),
    "android_breakpoint_set": ("Breakpoints", "Create a hardware breakpoint on the current target.", '{"address":"0x12345678","bp_type":1,"bp_scope":0,"length":4}'),
    "android_breakpoint_clear_all": ("Breakpoints", "Remove all active hardware breakpoints.", "{}"),
    "android_breakpoint_record_remove": ("Breakpoints", "Delete one saved hardware breakpoint record.", '{"index":0}'),
    "android_breakpoint_record_update": ("Breakpoints", "Patch one field inside a saved hardware breakpoint record.", '{"index":0,"field":"addr","value":"0x12345678"}'),
}

def _format_tool_parameters(parameters: dict[str, Any] | None) -> str:
    if not parameters:
        return "none"

    properties = parameters.get("properties", {})
    required = set(parameters.get("required", []))
    parts: list[str] = []
    for name, schema in properties.items():
        type_name = schema.get("type", "any")
        fragment = f"{name}: {type_name}"
        if name not in required:
            if "default" in schema:
                fragment += f" = {schema['default']!r}"
            else:
                fragment += " (optional)"
        parts.append(fragment)
    return ", ".join(parts) if parts else "none"


def _build_tool_catalog() -> list[dict[str, str]]:
    return [
        {
            "name": tool.name,
            "description": tool.description or "",
            "when": TOOL_META.get(tool.name, ("Bridge Setup", "Use this tool according to its description.", "{}"))[1],
            "parameters": _format_tool_parameters(tool.parameters),
            "example": TOOL_META.get(tool.name, ("Bridge Setup", "", "{}"))[2],
        }
        for tool in mcp._tool_manager.list_tools()
    ]


def _group_tool_catalog(tool_catalog: list[dict[str, str]]) -> dict[str, list[dict[str, str]]]:
    by_group: dict[str, list[dict[str, str]]] = {}
    for tool in tool_catalog:
        group = TOOL_META.get(tool["name"], ("Bridge Setup", "", ""))[0]
        by_group.setdefault(group, []).append(tool)
    return by_group


def _render_tool_guide(tool_catalog: list[dict[str, str]]) -> str:
    sections: list[str] = []
    for group_name, items in _group_tool_catalog(tool_catalog).items():
        sections.append(group_name)
        for tool in items:
            sections.extend(
                [
                    f"- {tool['name']}",
                    f"  Purpose: {tool['description']}",
                    f"  Use when: {tool['when']}",
                    f"  Parameters: {tool['parameters']}",
                    f"  Example args: {tool['example']}",
                ]
            )
        sections.append("")
    return "\n".join(sections).strip()


def _build_connection_steps(runtime: dict[str, Any]) -> str:
    return "\n".join(
        [
            "1. Connect to this MCP server using Streamable HTTP.",
            f"2. Use this URL: {runtime['streamable_http_url']}",
            "3. Initialize MCP, call tools/list, then call tools by their exact names.",
            "4. Start with configure_android_bridge, android_bridge_ping, and android_target_attach_package.",
            "5. For module base addresses use android_memory_regions or android_module_address.",
            "6. For disassembly use android_memory_view_open(view_format='disasm') and then read data.disasm from android_memory_view_read.",
        ]
    )


def _build_curl_example(runtime: dict[str, Any]) -> str:
    body = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2025-03-26",
            "capabilities": {},
            "clientInfo": {"name": "curl-client", "version": "1.0"},
        },
    }
    return (
        "curl -X POST "
        f"\"{runtime['streamable_http_url']}\" "
        "-H \"Content-Type: application/json\" "
        "-H \"Accept: application/json, text/event-stream\" "
        f"-d '{json.dumps(body, ensure_ascii=False)}'"
    )


def _build_python_example(runtime: dict[str, Any]) -> str:
    return "\n".join(
        [
            "import requests",
            f"url = {runtime['streamable_http_url']!r}",
            "payload = {",
            "    'jsonrpc': '2.0',",
            "    'id': 1,",
            "    'method': 'initialize',",
            "    'params': {",
            "        'protocolVersion': '2025-03-26',",
            "        'capabilities': {},",
            "        'clientInfo': {'name': 'python-client', 'version': '1.0'},",
            "    },",
            "}",
            "resp = requests.post(url, json=payload, headers={'Accept': 'application/json, text/event-stream'})",
            "print(resp.status_code)",
            "print(resp.text)",
            "# Then call tools/list, then tools/call.",
        ]
    )


def _build_startup_handoff(runtime: dict[str, Any]) -> str:
    return "\n".join(
        [
            "[MCP] AI connection guide:",
            *[f"  {line}" for line in _build_connection_steps(runtime).splitlines()],
            "[MCP] curl initialize example:",
            f"  {_build_curl_example(runtime)}",
            "[MCP] python initialize example:",
            *[f"  {line}" for line in _build_python_example(runtime).splitlines()],
        ]
    )


def _build_config_html(runtime: dict[str, Any]) -> str:
    tool_catalog = _build_tool_catalog()
    guide_text = "\n\n".join(
        [
            "NativeTcpBridge MCP",
            f"MCP URL\n{runtime['streamable_http_url']}",
            f"Connection Steps\n{_build_connection_steps(runtime)}",
            f"curl Initialize Example\n{_build_curl_example(runtime)}",
            f"Python Initialize Example\n{_build_python_example(runtime)}",
            f"Tools\n{_render_tool_guide(tool_catalog)}",
        ]
    )

    return "\n".join(
        [
            "<!doctype html>",
            "<html lang='en'>",
            "<head>",
            "  <meta charset='utf-8'>",
            "  <meta name='viewport' content='width=device-width, initial-scale=1'>",
            "  <title>NativeTcpBridge MCP Config</title>",
            "  <style>body{margin:0;padding:24px;font:14px/1.6 Consolas,'Courier New',monospace;background:#faf8f2;color:#1f2328}pre{white-space:pre-wrap;word-break:break-word}</style>",
            "</head>",
            "<body>",
            f"  <pre>{escape(guide_text)}</pre>",
            "</body>",
            "</html>",
        ]
    )


def _run_http_suite(runtime: dict[str, Any]) -> None:
    import uvicorn
    from starlette.applications import Starlette
    from starlette.responses import HTMLResponse, RedirectResponse
    from starlette.routing import Route

    streamable_app = mcp.streamable_http_app()

    async def config_page(_) -> HTMLResponse:
        return HTMLResponse(_build_config_html(runtime))

    async def root_page(_) -> RedirectResponse:
        return RedirectResponse(url=runtime["config_path"], status_code=307)

    middleware = list(streamable_app.user_middleware)
    routes = list(streamable_app.routes)
    routes.append(Route(runtime["config_path"], endpoint=config_page, methods=["GET"]))
    routes.append(Route("/", endpoint=root_page, methods=["GET"]))

    app = Starlette(
        debug=mcp.settings.debug,
        routes=routes,
        middleware=middleware,
        lifespan=streamable_app.router.lifespan_context,
    )

    config = uvicorn.Config(
        app,
        host=mcp.settings.host,
        port=mcp.settings.port,
        log_level=mcp.settings.log_level.lower(),
    )
    server = uvicorn.Server(config)
    server.run()


def _emit_startup_info(runtime: dict[str, Any]) -> None:
    print("[MCP] Server started:", file=sys.stderr, flush=True)
    print(f"  Streamable HTTP: {runtime['streamable_http_url']}", file=sys.stderr, flush=True)
    print(f"  Config: {runtime['config_url']}", file=sys.stderr, flush=True)
    print(_build_startup_handoff(runtime), file=sys.stderr, flush=True)


def _configure_runtime(args: argparse.Namespace) -> dict[str, Any]:
    bridge.configure(
        host=args.android_host,
        timeout_seconds=args.android_timeout,
    )

    mcp.settings.host = args.mcp_host.strip() or DEFAULT_MCP_BIND_HOST
    mcp.settings.port = int(args.mcp_port)
    mcp.settings.streamable_http_path = _normalize_http_path(args.mcp_path)
    config_path = _normalize_http_path(args.mcp_config_path)

    runtime: dict[str, Any] = {
        "config_path": config_path,
        "streamable_http_url": _format_http_endpoint(
            mcp.settings.host,
            mcp.settings.port,
            mcp.settings.streamable_http_path,
        ),
        "config_url": _format_http_endpoint(
            mcp.settings.host,
            mcp.settings.port,
            config_path,
        ),
    }
    return runtime


def main() -> int:
    parser = _build_arg_parser()
    args = parser.parse_args()
    runtime = _configure_runtime(args)
    _emit_startup_info(runtime)
    _run_http_suite(runtime)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
