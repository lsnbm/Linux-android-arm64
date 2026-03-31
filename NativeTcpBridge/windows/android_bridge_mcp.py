#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import sys
import threading
from html import escape
from pathlib import Path
from typing import Any

from mcp.server.fastmcp import FastMCP

PROJECT_WINDOWS_DIR = Path(__file__).resolve().parents[1] / "windows"
if str(PROJECT_WINDOWS_DIR) not in sys.path:
    sys.path.insert(0, str(PROJECT_WINDOWS_DIR))

from tcp_server import (  # noqa: E402
    AndroidBridgeClient,
    AndroidBridgeSession,
    BridgeConnectionError,
    BridgeError,
    DEFAULT_ANDROID_HOST,
    DEFAULT_ANDROID_PORT,
    DEFAULT_ANDROID_TIMEOUT_SECONDS,
    format_address,
    is_auto_bridge_host,
    normalize_view_format,
)

DEFAULT_MCP_BIND_HOST = os.getenv("ANDROID_MCP_BIND_HOST", "127.0.0.1").strip() or "127.0.0.1"
DEFAULT_MCP_BIND_PORT = int(os.getenv("ANDROID_MCP_BIND_PORT", "13337"))
DEFAULT_MCP_PATH = os.getenv("ANDROID_MCP_PATH", "/mcp")
DEFAULT_MCP_CONFIG_PATH = os.getenv("ANDROID_MCP_CONFIG_PATH", "/config.html")

bridge = AndroidBridgeClient(
    host=DEFAULT_ANDROID_HOST,
    port=DEFAULT_ANDROID_PORT,
    timeout_seconds=DEFAULT_ANDROID_TIMEOUT_SECONDS,
)
_bridge_session_lock = threading.RLock()
_bridge_session: AndroidBridgeSession | None = None
_bridge_session_host = ""
_bridge_session_port = DEFAULT_ANDROID_PORT
_bridge_session_timeout = DEFAULT_ANDROID_TIMEOUT_SECONDS
_bridge_session_pid: int | None = None


def _extract_pid_from_response(payload: dict[str, Any]) -> int | None:
    pairs = payload.get("pairs")
    if isinstance(pairs, dict) and "pid" in pairs:
        try:
            return int(str(pairs.get("pid", "0")), 0)
        except ValueError:
            return None

    data = payload.get("data")
    if isinstance(data, dict) and "pid" in data:
        try:
            return int(str(data.get("pid", "0")), 0)
        except ValueError:
            return None
    return None


def _disconnect_bridge_session() -> None:
    global _bridge_session, _bridge_session_host, _bridge_session_port, _bridge_session_timeout, _bridge_session_pid
    with _bridge_session_lock:
        if _bridge_session is not None:
            _bridge_session.disconnect()
        _bridge_session = None
        _bridge_session_host = ""
        _bridge_session_port = DEFAULT_ANDROID_PORT
        _bridge_session_timeout = DEFAULT_ANDROID_TIMEOUT_SECONDS
        _bridge_session_pid = None


def _resolve_host_for_session(config: dict[str, Any]) -> str:
    host = str(config.get("host", "")).strip() or "auto"
    if not is_auto_bridge_host(host):
        return host

    resolved = str(config.get("resolved_host") or "").strip()
    if resolved:
        return resolved

    discovered = bridge.discover()
    candidates = discovered.get("candidates")
    if isinstance(candidates, list):
        for item in candidates:
            candidate = str(item).strip()
            if candidate:
                return candidate

    raise BridgeConnectionError("auto host discovery returned no reachable Android tcp_server")


def _ensure_bridge_session(*, restore_pid: bool = True) -> AndroidBridgeSession:
    global _bridge_session, _bridge_session_host, _bridge_session_port, _bridge_session_timeout, _bridge_session_pid
    config = bridge.current_config()
    host = _resolve_host_for_session(config)
    port = int(config.get("port", DEFAULT_ANDROID_PORT))
    timeout_seconds = float(config.get("timeout_seconds", DEFAULT_ANDROID_TIMEOUT_SECONDS))

    with _bridge_session_lock:
        reconnected = False
        need_new = (
            _bridge_session is None
            or _bridge_session_host != host
            or _bridge_session_port != port
            or abs(_bridge_session_timeout - timeout_seconds) > 1e-9
        )
        if need_new:
            if _bridge_session is not None:
                _bridge_session.disconnect()
            _bridge_session = AndroidBridgeSession(
                timeout_seconds=timeout_seconds,
            )
            _bridge_session.connect(host, port)
            _bridge_session_host = host
            _bridge_session_port = port
            _bridge_session_timeout = timeout_seconds
            reconnected = True
        elif not _bridge_session.is_connected():
            _bridge_session.connect(host, port)
            reconnected = True

        if reconnected and restore_pid and _bridge_session_pid is not None:
            cached_pid = _bridge_session_pid
            try:
                _bridge_session.call_operation("target.pid.set", {"pid": cached_pid}).require_ok()
            except BridgeError as exc:
                # Cached PID may be stale after target app restart; clear it to avoid blocking new binds.
                _bridge_session_pid = None
                raise BridgeError(
                    f"cached pid {cached_pid} restore failed after reconnect; "
                    "please call android_target_set_pid or android_target_attach_package again"
                ) from exc
        return _bridge_session


def _call_bridge_operation(operation: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
    global _bridge_session_pid
    op = operation.strip()
    req_params = params or {}
    last_exc: Exception | None = None

    for _ in range(2):
        try:
            session = _ensure_bridge_session(restore_pid=op not in {"target.pid.set", "target.attach.package"})
            response = session.call_operation(op, req_params).require_ok().to_dict()
            if op in {"target.pid.set", "target.attach.package", "target.pid.current"}:
                maybe_pid = _extract_pid_from_response(response)
                if maybe_pid is not None:
                    _bridge_session_pid = maybe_pid
            return response
        except BridgeConnectionError as exc:
            last_exc = exc
            _disconnect_bridge_session()

    if last_exc is not None:
        raise last_exc
    raise BridgeConnectionError(f"bridge operation failed: {op}")
mcp = FastMCP(
    "NativeTcpBridge Android MCP",
    host=DEFAULT_MCP_BIND_HOST,
    port=DEFAULT_MCP_BIND_PORT,
    streamable_http_path=DEFAULT_MCP_PATH,
)


@mcp.resource("android://connection")
def android_connection() -> dict[str, Any]:
    """Return the current Android TCP connection settings used by this MCP server."""
    snapshot = bridge.current_config()
    snapshot["session_connected"] = _bridge_session is not None and _bridge_session.is_connected()
    snapshot["session_host"] = _bridge_session_host or None
    snapshot["session_port"] = _bridge_session_port
    snapshot["session_timeout_seconds"] = _bridge_session_timeout
    snapshot["session_pid"] = _bridge_session_pid
    return snapshot


@mcp.resource("android://protocol")
def android_protocol() -> dict[str, Any]:
    """Return the structured bridge protocol description exposed by the Android tcp_server."""
    return bridge.describe()


@mcp.resource("android://guide")
def android_guide() -> dict[str, Any]:
    """Return minimal AI tool guide: purpose, parameters, and examples only."""
    return _build_ai_guide_payload()


@mcp.resource("android://tool_catalog")
def android_tool_catalog() -> dict[str, Any]:
    """Return minimal tool catalog: purpose, parameters, and examples only."""
    return _build_ai_guide_payload()


@mcp.tool()
def configure_android_bridge(
    host: str = DEFAULT_ANDROID_HOST,
    timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS,
) -> dict[str, Any]:
    """Configure the Android tcp_server target. Use host='auto' to scan the LAN for a reachable device."""
    before = bridge.current_config()
    snapshot = bridge.configure(host=host, timeout_seconds=timeout_seconds)
    config_changed = (
        str(before.get("host", "")).strip() != str(snapshot.get("host", "")).strip()
        or int(before.get("port", DEFAULT_ANDROID_PORT)) != int(snapshot.get("port", DEFAULT_ANDROID_PORT))
        or abs(
            float(before.get("timeout_seconds", DEFAULT_ANDROID_TIMEOUT_SECONDS))
            - float(snapshot.get("timeout_seconds", DEFAULT_ANDROID_TIMEOUT_SECONDS))
        )
        > 1e-9
    )
    if config_changed:
        _disconnect_bridge_session()
    snapshot["session_connected"] = _bridge_session is not None and _bridge_session.is_connected()
    return snapshot


@mcp.tool()
def discover_android_bridges() -> dict[str, Any]:
    """Discover Android tcp_server candidates on the LAN and show the current TCP bridge state."""
    return bridge.discover()


@mcp.tool()
def android_bridge_ping() -> dict[str, Any]:
    """Check whether the currently configured Android tcp_server is reachable."""
    return _call_bridge_operation("bridge.ping")


@mcp.tool()
def connect_android_bridge() -> dict[str, Any]:
    """Actively connect and keep session/PID state across tool calls until disconnect_android_bridge is called."""
    _ensure_bridge_session()
    return {
        "ok": True,
        "operation": "bridge.connect",
        "message": "connected",
        "connection": {
            "host": _bridge_session_host,
            "port": _bridge_session_port,
            "timeout_seconds": _bridge_session_timeout,
            "persistent": True,
        },
        "pid": _bridge_session_pid,
    }


@mcp.tool()
def disconnect_android_bridge() -> dict[str, Any]:
    """Disconnect current persistent MCP bridge session. PID/session state will be reset."""
    _disconnect_bridge_session()
    return {
        "ok": True,
        "operation": "bridge.disconnect",
        "message": "disconnected",
    }


@mcp.tool()
def android_target_set_pid(pid: int) -> dict[str, Any]:
    """Bind all scan, viewer, and breakpoint operations to a known PID."""
    return _call_bridge_operation("target.pid.set", {"pid": pid})


@mcp.tool()
def android_target_attach_package(package_name: str) -> dict[str, Any]:
    """Resolve a package name to PID and make that process the current target."""
    return _call_bridge_operation("target.attach.package", {"package_name": package_name})


@mcp.tool()
def android_target_current() -> dict[str, Any]:
    """Read the current target process bound inside the Android tcp_server."""
    return _call_bridge_operation("target.pid.current")


@mcp.tool()
def android_memory_regions() -> dict[str, Any]:
    """Fetch the full module and memory region map for the current target process."""
    return _call_bridge_operation("memory.info.full")


@mcp.tool()
def android_module_address(module_name: str, segment_index: int = 0, which: str = "start") -> dict[str, Any]:
    """Resolve a module segment start or end address from the current target process."""
    which_token = which.strip().lower()
    if which_token not in {"start", "end"}:
        raise ValueError("which must be 'start' or 'end'")
    return _call_bridge_operation(
        "module.resolve",
        {"module_name": module_name, "segment_index": segment_index, "which": which_token},
    )


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
    if mode_token != "unknown" and not str(value).strip():
        raise ValueError("value is required unless mode is 'unknown'")
    params: dict[str, Any] = {"value_type": type_token, "mode": mode_token}
    if str(value).strip():
        params["value"] = value
    if str(range_max).strip():
        params["range_max"] = range_max
    return _call_bridge_operation("scan.start", params)


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
    if mode_token != "unknown" and not str(value).strip():
        raise ValueError("value is required unless mode is 'unknown'")
    params: dict[str, Any] = {"value_type": type_token, "mode": mode_token}
    if str(value).strip():
        params["value"] = value
    if str(range_max).strip():
        params["range_max"] = range_max
    return _call_bridge_operation("scan.refine", params)


@mcp.tool()
def android_memory_scan_results(start: int = 0, count: int = 100, value_type: str = "i32") -> dict[str, Any]:
    """Read one page of the current memory scan results."""
    if count <= 0 or count > 2000:
        raise ValueError("count must be in 1..2000")
    return _call_bridge_operation(
        "scan.page",
        {"start": start, "count": count, "value_type": value_type.strip().lower()},
    )


@mcp.tool()
def android_memory_view_open(address: int | str, view_format: str = "hex") -> dict[str, Any]:
    """Open the memory viewer at an address. Use view_format='disasm' to request disassembly instead of raw hex bytes."""
    return _call_bridge_operation(
        "viewer.open",
        {"address": format_address(address), "view_format": normalize_view_format(view_format)},
    )


@mcp.tool()
def android_memory_view_move(lines: int, step: int | None = None) -> dict[str, Any]:
    """Move the current memory viewer window by lines."""
    params: dict[str, Any] = {"lines": lines}
    if step is not None:
        params["step"] = step
    return _call_bridge_operation("viewer.move", params)


@mcp.tool()
def android_memory_view_offset(offset: str) -> dict[str, Any]:
    """Move the current viewer base by an offset such as '+0x20' or '-0x10'."""
    return _call_bridge_operation("viewer.offset", {"offset": offset})


@mcp.tool()
def android_memory_view_set_format(view_format: str) -> dict[str, Any]:
    """Change the current viewer format. Use 'disasm' for disassembly, otherwise the viewer returns formatted memory values."""
    return _call_bridge_operation("viewer.set_format", {"view_format": normalize_view_format(view_format)})


@mcp.tool()
def android_memory_view_read() -> dict[str, Any]:
    """Read the current viewer snapshot. In disasm mode, the result is in data.disasm; in other modes, raw bytes remain in data.data_hex."""
    return _call_bridge_operation("viewer.snapshot")


@mcp.tool()
def android_breakpoint_list() -> dict[str, Any]:
    """List the current hardware breakpoint state and saved breakpoint records."""
    return _call_bridge_operation("breakpoint.info")


@mcp.tool()
def android_breakpoint_set(address: int | str, bp_type: int, bp_scope: int, length: int) -> dict[str, Any]:
    """Create a hardware breakpoint on the current target process."""
    return _call_bridge_operation(
        "breakpoint.set",
        {
            "address": format_address(address),
            "bp_type": bp_type,
            "bp_scope": bp_scope,
            "length": length,
        },
    )


@mcp.tool()
def android_breakpoint_clear_all() -> dict[str, Any]:
    """Remove all active hardware breakpoints from the current target process."""
    return _call_bridge_operation("breakpoint.clear")


@mcp.tool()
def android_breakpoint_record_remove(index: int) -> dict[str, Any]:
    """Remove one saved hardware breakpoint record by index."""
    return _call_bridge_operation("breakpoint.record.remove", {"index": index})


@mcp.tool()
def android_breakpoint_record_update(index: int, field: str, value: int | str) -> dict[str, Any]:
    """Patch one field inside a saved hardware breakpoint record."""
    normalized = f"0x{value:X}" if isinstance(value, int) else str(value).strip()
    return _call_bridge_operation("breakpoint.record.update", {"index": index, "field": field, "value": normalized})


@mcp.tool()
def android_breakpoint_record_set_float(index: int, register_name: str, float_value: float, precision: str = "f32") -> dict[str, Any]:
    """Set a SIMD/float register (V0~V31) in a breakpoint record to a float value. Enables register modification on next breakpoint hit."""
    reg = str(register_name).strip().lower()
    if not reg.startswith("v"):
        raise ValueError("register_name must be v0~v31")
    return _call_bridge_operation("breakpoint.record.set_float", {
        "index": index,
        "field": reg,
        "value": str(float_value),
        "precision": precision,
    })


@mcp.tool()
def android_help(topic: str = "all") -> dict[str, Any]:
    """Return minimal tool docs for AI clients. topic can be all/<tool_name>."""
    topic_token = str(topic).strip().lower() or "all"
    guide = _build_ai_guide_payload()
    if topic_token == "all":
        return guide
    selected = [item for item in guide["tools"] if str(item.get("name", "")).lower() == topic_token]
    if selected:
        return {
            "topic": "tool",
            "tool": selected[0],
        }
    raise ValueError(f"unknown topic: {topic}. use all/<tool_name>")


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
        help=f"Target Android tcp_server host. Default is 'auto'; the Android port defaults to {DEFAULT_ANDROID_PORT}.",
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


TOOL_META: dict[str, dict[str, Any]] = {
    "configure_android_bridge": {
        "group": "Bridge Setup",
        "use_when": "Set or update the Android tcp_server host and timeout before any business operations.",
        "example": {"host": "auto", "timeout_seconds": 8},
        "parameter_notes": {
            "host": "Use 'auto' to scan LAN or pass a fixed IP like '127.0.0.1' with adb forward.",
            "timeout_seconds": "Request timeout. Increase for heavy scans (e.g. 20-60).",
        },
        "result_notes": "Returns effective bridge config and discovery-related fields.",
    },
    "discover_android_bridges": {
        "group": "Bridge Setup",
        "use_when": "Find candidate Android bridge hosts when the target IP is unknown.",
        "example": {},
        "parameter_notes": {},
        "result_notes": "Returns candidates and current bridge state snapshot.",
    },
    "android_bridge_ping": {
        "group": "Bridge Setup",
        "use_when": "Quick connectivity check to tcp_server before invoking heavy tools.",
        "example": {},
        "parameter_notes": {},
        "result_notes": "Returns pong on healthy connection.",
    },
    "connect_android_bridge": {
        "group": "Bridge Setup",
        "use_when": "Create and keep a persistent MCP->Android TCP session for stateful operations.",
        "example": {},
        "parameter_notes": {},
        "result_notes": "After connected, PID and viewer state stay valid until disconnect/reconfigure.",
    },
    "disconnect_android_bridge": {
        "group": "Bridge Setup",
        "use_when": "Explicitly close persistent MCP bridge session and reset sticky PID/session state.",
        "example": {},
        "parameter_notes": {},
        "result_notes": "Use before switching device/host context or to force clean reconnect.",
    },
    "android_target_set_pid": {
        "group": "Target Selection",
        "use_when": "Bind all subsequent scan/view/breakpoint operations to a known PID.",
        "example": {"pid": 1234},
        "parameter_notes": {"pid": "Positive process ID."},
        "result_notes": "Global target PID is updated on Android bridge side.",
    },
    "android_target_attach_package": {
        "group": "Target Selection",
        "use_when": "Resolve package name to PID and bind that process as current target.",
        "example": {"package_name": "com.example.app"},
        "parameter_notes": {"package_name": "Android package name, e.g. me.hd.ggtutorial."},
        "result_notes": "Returns resolved PID and binding result.",
    },
    "android_target_current": {
        "group": "Target Selection",
        "use_when": "Read current bound PID before a complex operation chain.",
        "example": {},
        "parameter_notes": {},
        "result_notes": "Returns active PID used by scan/view/breakpoint operations.",
    },
    "android_memory_regions": {
        "group": "Target Selection",
        "use_when": "Query modules/segments/regions for address planning.",
        "example": {},
        "parameter_notes": {},
        "result_notes": "Returns full memory map payload.",
    },
    "android_module_address": {
        "group": "Target Selection",
        "use_when": "Resolve one module segment boundary quickly.",
        "example": {"module_name": "libgame.so", "segment_index": 0, "which": "start"},
        "parameter_notes": {
            "module_name": "Exact module path/name from android_memory_regions.",
            "segment_index": "Zero-based segment index.",
            "which": "Either 'start' or 'end'.",
        },
        "result_notes": "Returns resolved address in message/pairs.",
    },
    "android_memory_scan_start": {
        "group": "Memory Scan",
        "use_when": "Start a fresh scan task.",
        "example": {"value_type": "i32", "mode": "eq", "value": "1234"},
        "parameter_notes": {
            "value_type": "i8/i16/i32/i64/f32/f64.",
            "mode": "unknown/eq/gt/lt/inc/dec/changed/unchanged/range/pointer.",
            "value": "Required unless mode=unknown.",
            "range_max": "Optional range bound, used for range mode.",
        },
        "result_notes": "Starts (or refreshes) scanner state and returns immediate status.",
    },
    "android_memory_scan_refine": {
        "group": "Memory Scan",
        "use_when": "Refine existing scan results using new condition.",
        "example": {"value_type": "i32", "mode": "eq", "value": "1234"},
        "parameter_notes": {
            "value_type": "Must match intended value interpretation.",
            "mode": "Same token set as android_memory_scan_start.",
            "value": "Required unless mode=unknown.",
            "range_max": "Optional range bound.",
        },
        "result_notes": "Narrows previous result set.",
    },
    "android_memory_scan_results": {
        "group": "Memory Scan",
        "use_when": "Read one page of scan hits.",
        "example": {"start": 0, "count": 100, "value_type": "i32"},
        "parameter_notes": {
            "start": "Offset into scan result list.",
            "count": "1..2000 page size.",
            "value_type": "How to render values in output.",
        },
        "result_notes": "Returns page items and total_count.",
    },
    "android_memory_view_open": {
        "group": "Memory View",
        "use_when": "Open viewer at address in hex/typed/disasm mode.",
        "example": {"address": "0x12345678", "view_format": "disasm"},
        "parameter_notes": {
            "address": "Integer or hex string.",
            "view_format": "hex/hex16/hex64/i8/i16/i32/i64/f32/f64/disasm.",
        },
        "result_notes": "Initializes viewer base and mode.",
    },
    "android_memory_view_move": {
        "group": "Memory View",
        "use_when": "Scroll current viewer window.",
        "example": {"lines": 16},
        "parameter_notes": {
            "lines": "Positive/negative line offset.",
            "step": "Optional custom byte step per line.",
        },
        "result_notes": "Updates viewer position.",
    },
    "android_memory_view_offset": {
        "group": "Memory View",
        "use_when": "Move viewer by relative address expression.",
        "example": {"offset": "+0x20"},
        "parameter_notes": {"offset": "Examples: +0x20, -0x10."},
        "result_notes": "Applies relative shift from current base.",
    },
    "android_memory_view_set_format": {
        "group": "Memory View",
        "use_when": "Switch viewer output format.",
        "example": {"view_format": "disasm"},
        "parameter_notes": {"view_format": "Same set as android_memory_view_open."},
        "result_notes": "Viewer stays at current base with new format.",
    },
    "android_memory_view_read": {
        "group": "Memory View",
        "use_when": "Read current viewer snapshot payload.",
        "example": {},
        "parameter_notes": {},
        "result_notes": "In disasm mode read data.disasm; otherwise use data.data_hex.",
    },
    "android_breakpoint_list": {
        "group": "Breakpoints",
        "use_when": "Inspect active breakpoint info and saved records.",
        "example": {},
        "parameter_notes": {},
        "result_notes": "records may be empty if no hit record exists.",
    },
    "android_breakpoint_set": {
        "group": "Breakpoints",
        "use_when": "Create one hardware breakpoint.",
        "example": {"address": "0x12345678", "bp_type": 1, "bp_scope": 0, "length": 4},
        "parameter_notes": {
            "address": "Target instruction/data address.",
            "bp_type": "Type enum from backend.",
            "bp_scope": "Scope enum from backend.",
            "length": "Byte length constraint from backend.",
        },
        "result_notes": "Creates breakpoint; record update requires existing record index.",
    },
    "android_breakpoint_clear_all": {
        "group": "Breakpoints",
        "use_when": "Remove all active hardware breakpoints.",
        "example": {},
        "parameter_notes": {},
        "result_notes": "Clears runtime breakpoints.",
    },
    "android_breakpoint_record_remove": {
        "group": "Breakpoints",
        "use_when": "Delete one saved breakpoint record entry.",
        "example": {"index": 0},
        "parameter_notes": {"index": "Valid index from android_breakpoint_list.data.records."},
        "result_notes": "Fails with index out of range when records is empty.",
    },
    "android_breakpoint_record_update": {
        "group": "Breakpoints",
        "use_when": "Patch one field in saved breakpoint record.",
        "example": {"index": 0, "field": "addr", "value": "0x12345678"},
        "parameter_notes": {
            "index": "Valid existing record index.",
            "field": "Backend-supported record field name.",
            "value": "Number or hex string depending on field.",
        },
        "result_notes": "Requires non-empty records; otherwise index out of range.",
    },
    "android_help": {
        "group": "Docs",
        "use_when": "Fetch tool usage docs directly from tool channel.",
        "example": {"topic": "android_memory_view_open"},
        "parameter_notes": {"topic": "all/<tool_name>."},
        "result_notes": "Returns minimal tool usage docs.",
    },
}


def _meta_for_tool(tool_name: str) -> dict[str, Any]:
    return TOOL_META.get(
        tool_name,
        {
            "group": "Bridge Setup",
            "use_when": "Use this tool according to its description.",
            "example": {},
            "parameter_notes": {},
            "result_notes": "",
        },
    )


def _build_tool_parameter_docs(
    parameters: dict[str, Any] | None,
    parameter_notes: dict[str, Any] | None,
) -> list[dict[str, Any]]:
    properties = parameters.get("properties", {}) if isinstance(parameters, dict) else {}
    required = set(parameters.get("required", [])) if isinstance(parameters, dict) else set()
    notes = parameter_notes if isinstance(parameter_notes, dict) else {}

    docs: list[dict[str, Any]] = []
    seen: set[str] = set()
    for name, schema in properties.items():
        field_name = str(name)
        seen.add(field_name)
        schema_obj = schema if isinstance(schema, dict) else {}
        item: dict[str, Any] = {
            "name": field_name,
            "type": str(schema_obj.get("type", "any")),
            "required": field_name in required,
            "description": str(notes.get(field_name) or schema_obj.get("description") or ""),
        }
        if "default" in schema_obj:
            item["default"] = schema_obj["default"]
        docs.append(item)

    for name, note in notes.items():
        field_name = str(name)
        if field_name in seen:
            continue
        docs.append(
            {
                "name": field_name,
                "type": "any",
                "required": False,
                "description": str(note),
            }
        )
    return docs


def _build_tool_catalog() -> list[dict[str, Any]]:
    catalog: list[dict[str, Any]] = []
    for tool in mcp._tool_manager.list_tools():
        meta = _meta_for_tool(tool.name)
        parameter_notes = dict(meta.get("parameter_notes", {})) if isinstance(meta.get("parameter_notes"), dict) else {}
        catalog.append(
            {
                "name": tool.name,
                "purpose": str(meta.get("use_when", "") or tool.description or ""),
                "parameters": _build_tool_parameter_docs(tool.parameters, parameter_notes),
                "example": dict(meta.get("example", {})) if isinstance(meta.get("example"), dict) else meta.get("example", {}),
            }
        )
    return catalog


def _group_tool_catalog(tool_catalog: list[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    return {"Tools": list(tool_catalog)}


def _render_tool_guide(tool_catalog: list[dict[str, Any]]) -> str:
    sections: list[str] = []
    for group_name, items in _group_tool_catalog(tool_catalog).items():
        sections.append(group_name)
        for tool in items:
            example_text = json.dumps(tool.get("example", {}), ensure_ascii=False)
            sections.extend(
                [
                    f"- {tool['name']}",
                    f"  作用: {tool.get('purpose', '')}",
                    "  参数说明:",
                ]
            )
            parameters = tool.get("parameters", [])
            if isinstance(parameters, list) and parameters:
                for field in parameters:
                    if not isinstance(field, dict):
                        continue
                    name = str(field.get("name", ""))
                    type_name = str(field.get("type", "any"))
                    required = bool(field.get("required", False))
                    desc = str(field.get("description", ""))
                    fragment = f"  - {name}: type={type_name}, required={required}"
                    if "default" in field:
                        fragment += f", default={field['default']!r}"
                    if desc:
                        fragment += f", desc={desc}"
                    sections.append(fragment)
            else:
                sections.append("  - 无")
            sections.append(f"  调用示例: {example_text}")
        sections.append("")
    return "\n".join(sections).strip()


def _build_ai_guide_payload() -> dict[str, Any]:
    return {
        "tools": _build_tool_catalog(),
    }


def _build_connection_steps(runtime: dict[str, Any]) -> str:
    return "\n".join(
        [
            "1. Connect to this MCP server using Streamable HTTP.",
            f"2. Use this URL: {runtime['streamable_http_url']}",
            "3. Initialize MCP, call tools/list, then call tools by their exact names.",
            "4. Start with configure_android_bridge, connect_android_bridge, android_bridge_ping, and android_target_attach_package.",
            "5. For module base addresses use android_memory_regions or android_module_address.",
            "6. For disassembly use android_memory_view_open(view_format='disasm') and then read data.disasm from android_memory_view_read.",
            "7. Keep the session alive for stateful operations; call disconnect_android_bridge only when you really want to reset PID/session state.",
            "8. The Android bridge also exposes a structured operation protocol at android://protocol.",
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
            "[MCP] AI tool usage guide:",
            "  Resource: android://guide",
            "  Resource: android://tool_catalog",
            "  Tool: android_help(topic='all' or topic='<tool_name>')",
        ]
    )


def _build_config_html(runtime: dict[str, Any]) -> str:
    tool_catalog = _build_tool_catalog()

    guide_text = "\n\n".join(
        [
            "NativeTcpBridge MCP Tool Guide",
            "仅包含: 工具作用 / 参数说明 / 调用示例",
            f"{_render_tool_guide(tool_catalog)}",
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
