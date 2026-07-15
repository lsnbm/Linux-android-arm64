#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Any

from mcp.server.fastmcp import FastMCP

PROJECT_WINDOWS_DIR = Path(__file__).resolve().parents[1] / "windows"
if str(PROJECT_WINDOWS_DIR) not in sys.path:
    sys.path.insert(0, str(PROJECT_WINDOWS_DIR))

from http_bridge import (  # noqa: E402
    AndroidHttpClient,
    DEFAULT_ANDROID_HOST,
    DEFAULT_ANDROID_PORT,
    DEFAULT_ANDROID_TIMEOUT_SECONDS,
    format_address,
    normalize_view_format,
)

DEFAULT_MCP_BIND_HOST = os.getenv("ANDROID_MCP_BIND_HOST", "127.0.0.1").strip() or "127.0.0.1"
DEFAULT_MCP_BIND_PORT = int(os.getenv("ANDROID_MCP_BIND_PORT", "14447"))
DEFAULT_MCP_PATH = os.getenv("ANDROID_MCP_PATH", "/mcp")

bridge = AndroidHttpClient(
    host=DEFAULT_ANDROID_HOST,
    port=DEFAULT_ANDROID_PORT,
    timeout_seconds=DEFAULT_ANDROID_TIMEOUT_SECONDS,
)
def _call_bridge_operation(operation: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
    return bridge.call_operation(operation, params).require_ok().to_dict()


def _strip_scan_regions(response: dict[str, Any]) -> dict[str, Any]:
    data = response.get("data")
    if not isinstance(data, dict):
        return response
    slim_data = dict(data)
    slim_data.pop("regions", None)
    slim_data.pop("region_count", None)
    slim_response = dict(response)
    slim_response["data"] = slim_data
    return slim_response


def _scan_params(value_type: str, mode: str, value: str, range_max: str, *, is_first: bool) -> dict[str, Any]:
    mode_token = mode.strip().lower()
    mode_token = {
        "equal": "eq",
        "greater": "gt",
        "less": "lt",
        "increased": "inc",
        "decreased": "dec",
        "chg": "changed",
        "unchg": "unchanged",
        "ptr": "pointer",
        "str": "string",
    }.get(mode_token, mode_token)
    value_token = str(value).strip()
    range_token = str(range_max).strip()
    history_modes = {"inc", "dec", "changed", "unchanged"}
    value_modes = {"eq", "gt", "lt", "range", "pointer", "string"}
    if is_first and mode_token in history_modes:
        raise ValueError("first scan cannot use inc, dec, changed, or unchanged")
    if mode_token in value_modes and not value_token:
        raise ValueError(f"value is required for mode '{mode_token}'")
    if mode_token == "range" and not range_token:
        raise ValueError("range_max is required for mode 'range'")

    params: dict[str, Any] = {"mode": mode_token}
    if mode_token == "pointer":
        params["value_type"] = "i64"
    elif mode_token != "string":
        type_token = value_type.strip().lower()
        if not type_token:
            raise ValueError("value_type is required for numeric scans")
        params["value_type"] = type_token
    if mode_token in value_modes:
        params["value"] = value_token
    if mode_token == "range":
        params["range_max"] = range_token
    return params


def _normalize_breakpoint_points(points: list[dict[str, Any]]) -> list[dict[str, Any]]:
    normalized: list[dict[str, Any]] = []
    for point in points:
        if not isinstance(point, dict):
            raise ValueError("points must contain objects with address, bp_type, bp_scope, length")
        normalized.append(
            {
                "address": format_address(point["address"]),
                "bp_type": point["bp_type"],
                "bp_scope": point["bp_scope"],
                "length": max(1, min(8, int(point["length"]))),
            }
        )
    if not normalized:
        raise ValueError("points must not be empty")
    return normalized


mcp = FastMCP(
    "NativeHttpBridge Android MCP",
    host=DEFAULT_MCP_BIND_HOST,
    port=DEFAULT_MCP_BIND_PORT,
    streamable_http_path=DEFAULT_MCP_PATH,
)


@mcp.resource("android://connection")
def android_connection() -> dict[str, Any]:
    """Return the current Android HTTP connection settings used by this MCP server."""
    return bridge.connection_state()


@mcp.tool()
def configure_android_bridge(
    host: str = DEFAULT_ANDROID_HOST,
    timeout_seconds: float = DEFAULT_ANDROID_TIMEOUT_SECONDS,
) -> dict[str, Any]:
    """Configure an Android HTTP bridge. Use host='auto' for LAN scan or pass a full HTTPS Tunnel URL."""
    bridge.configure(host=host, timeout_seconds=timeout_seconds)
    return bridge.connection_state()


@mcp.tool()
def discover_android_bridges() -> dict[str, Any]:
    """Discover Android HTTP bridge candidates on the LAN and show the current bridge state."""
    return bridge.discover()


@mcp.tool()
def android_bridge_ping() -> dict[str, Any]:
    """Diagnose bridge reachability; normal tools connect automatically and do not require this first."""
    return _call_bridge_operation("bridge.ping")


@mcp.tool()
def android_target_set_pid(pid: int) -> dict[str, Any]:
    """Bind all scan, viewer, and breakpoint operations to a known PID."""
    return _call_bridge_operation("target.select", {"pid": pid})


@mcp.tool()
def android_target_attach_package(package_name: str) -> dict[str, Any]:
    """Resolve a package name to PID and make that process the current target."""
    return _call_bridge_operation("target.attach", {"package_name": package_name})


@mcp.tool()
def android_target_find_pid(package_name: str) -> dict[str, Any]:
    """Resolve a package name to PID without changing the current target."""
    return _call_bridge_operation("target.find", {"package_name": package_name})


@mcp.tool()
def android_target_current() -> dict[str, Any]:
    """Read the current target process bound inside the Android HTTP bridge."""
    return _call_bridge_operation("target.get")


@mcp.tool()
def android_env_get_params(thread_name: str = "") -> dict[str, Any]:
    """Read ARM64 environment parameters for the current target process."""
    name = str(thread_name or "").strip()
    return _call_bridge_operation("env.read", {"thread_name": name})


@mcp.tool()
def android_memory_regions() -> dict[str, Any]:
    """Fetch module and segment information only; scan regions are not returned to MCP clients."""
    return _strip_scan_regions(_call_bridge_operation("memory.map"))


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
def android_memory_dump(target: str) -> dict[str, Any]:
    """Dump a module name or a half-open address range such as 0x5000-0x6000."""
    normalized = str(target).strip()
    if not normalized:
        raise ValueError("target must be a module name or start-end address range")
    return _call_bridge_operation("memory.dump", {"target": normalized})


@mcp.tool()
def android_memory_scan_start(
    value_type: str,
    mode: str,
    value: str = "",
    range_max: str = "",
) -> dict[str, Any]:
    """Start a new memory scan. Example: value_type='i32', mode='eq', value='1234'."""
    return _call_bridge_operation("scan.start", _scan_params(value_type, mode, value, range_max, is_first=True))


@mcp.tool()
def android_memory_scan_refine(
    value_type: str,
    mode: str,
    value: str = "",
    range_max: str = "",
) -> dict[str, Any]:
    """Refine the current memory scan result set."""
    return _call_bridge_operation("scan.refine", _scan_params(value_type, mode, value, range_max, is_first=False))


@mcp.tool()
def android_memory_scan_results(start: int = 0, count: int = 100, value_type: str = "i32") -> dict[str, Any]:
    """Read one page of the current memory scan results."""
    if count <= 0 or count > 200:
        raise ValueError("count must be in 1..200")
    return _call_bridge_operation(
        "scan.results",
        {"start": start, "count": count, "value_type": value_type.strip().lower()},
    )


@mcp.tool()
def android_memory_scan_status() -> dict[str, Any]:
    """Read the current memory scan progress and result count."""
    return _call_bridge_operation("scan.get")


@mcp.tool()
def android_memory_scan_clear() -> dict[str, Any]:
    """Clear the current memory scan result set."""
    return _call_bridge_operation("scan.clear")


@mcp.tool()
def android_memory_read(address: int | str, size: int) -> dict[str, Any]:
    """Read 1 to 1048576 raw bytes from any valid target address."""
    if size <= 0 or size > 1024 * 1024:
        raise ValueError("size must be in 1..1048576")
    return _call_bridge_operation(
        "memory.read",
        {"address": format_address(address), "size": size},
    )


@mcp.tool()
def android_memory_write(address: int | str, data_hex: str) -> dict[str, Any]:
    """Write raw bytes and return Android-side readback verification."""
    normalized = "".join(str(data_hex).split())
    if not normalized:
        raise ValueError("data_hex must not be empty")
    if len(normalized) % 2 != 0:
        raise ValueError("data_hex must contain complete bytes")
    if len(normalized) // 2 > 1024 * 1024:
        raise ValueError("data_hex must contain at most 1048576 bytes")
    return _call_bridge_operation(
        "memory.write",
        {"address": format_address(address), "data_hex": normalized},
    )


@mcp.tool()
def android_memory_lock(address: int | str, value_type: str, value: str) -> dict[str, Any]:
    """Continuously lock one target address to the requested typed value."""
    return _call_bridge_operation(
        "lock.set",
        {"address": format_address(address), "value_type": value_type.strip().lower(), "value": str(value)},
    )


@mcp.tool()
def android_memory_unlock(address: int | str) -> dict[str, Any]:
    """Remove the continuous value lock from one target address."""
    return _call_bridge_operation("lock.remove", {"address": format_address(address)})


@mcp.tool()
def android_pointer_status() -> dict[str, Any]:
    """Read current pointer scan task state and preserved result count."""
    return _call_bridge_operation("pointer.get")


@mcp.tool()
def android_pointer_scan(
    target: int | str,
    depth: int,
    max_offset: int,
    mode: str = "module",
    manual_base: int | str | None = None,
    array_base: int | str | None = None,
    array_count: int | None = None,
    module_filter: str = "",
) -> dict[str, Any]:
    """Start a pointer scan using module/manual/array base mode."""
    mode_token = str(mode).strip().lower() or "module"
    if mode_token not in {"module", "manual", "array"}:
        raise ValueError("mode must be one of: module, manual, array")
    if depth <= 0 or depth > 16:
        raise ValueError("depth must be in 1..16")
    if max_offset <= 0:
        raise ValueError("max_offset must be greater than 0")

    params: dict[str, Any] = {
        "target": format_address(target),
        "depth": depth,
        "max_offset": max_offset,
    }
    if mode_token != "module":
        params["mode"] = mode_token
    if module_filter.strip():
        params["module_filter"] = module_filter

    if mode_token == "manual":
        if manual_base is None:
            raise ValueError("manual mode requires manual_base")
        params["manual_base"] = format_address(manual_base)
    elif mode_token == "array":
        if array_base is None or array_count is None:
            raise ValueError("array mode requires array_base and array_count")
        if array_count <= 0 or array_count > 1_000_000:
            raise ValueError("array_count must be in 1..1000000")
        params["array_base"] = format_address(array_base)
        params["array_count"] = array_count

    return _call_bridge_operation("pointer.scan", params)


@mcp.tool()
def android_pointer_merge() -> dict[str, Any]:
    """Merge all saved pointer bin files by keeping chains with matching offset structure."""
    return _call_bridge_operation("pointer.merge")


@mcp.tool()
def android_pointer_export() -> dict[str, Any]:
    """Export the merged pointer bin data into a human-readable text file."""
    return _call_bridge_operation("pointer.export")


@mcp.tool()
def android_memory_view_open(address: int | str, view_format: str = "hexadecimal") -> dict[str, Any]:
    """Open an address and return its freshly read 100-byte snapshot."""
    return _call_bridge_operation(
        "viewer.open",
        {"address": format_address(address), "view_format": normalize_view_format(view_format)},
    )


@mcp.tool()
def android_memory_view_offset(offset: str) -> dict[str, Any]:
    """Move by an exact byte offset such as '+0x20' or '-0x10' and return the fresh snapshot."""
    return _call_bridge_operation("viewer.seek", {"offset": offset})


@mcp.tool()
def android_memory_view_set_format(view_format: str) -> dict[str, Any]:
    """Change the viewer format and return the freshly decoded snapshot."""
    return _call_bridge_operation("viewer.format", {"view_format": normalize_view_format(view_format)})


@mcp.tool()
def android_memory_view_read() -> dict[str, Any]:
    """Refresh and return the current 100-byte snapshot; disasm also returns decoded instructions."""
    return _call_bridge_operation("viewer.refresh")


@mcp.tool()
def android_breakpoint_get() -> dict[str, Any]:
    """Return the current breakpoint mode and records."""
    return _call_bridge_operation("breakpoint.get")


@mcp.tool()
def android_breakpoint_set(mode: str, points: list[dict[str, Any]]) -> dict[str, Any]:
    """Set hwbp, ptebp, or stepbp points for the current target."""
    mode_token = str(mode).strip().lower()
    if mode_token not in {"hwbp", "ptebp", "stepbp"}:
        raise ValueError("mode must be one of: hwbp, ptebp, stepbp")
    return _call_bridge_operation(
        "breakpoint.set",
        {"mode": mode_token, "points": _normalize_breakpoint_points(points)},
    )


@mcp.tool()
def android_breakpoint_clear() -> dict[str, Any]:
    """Clear the currently active hwbp, ptebp, or stepbp mode."""
    return _call_bridge_operation("breakpoint.clear")


@mcp.tool()
def android_breakpoint_record_remove(index: int) -> dict[str, Any]:
    """Remove one saved hardware breakpoint record by index."""
    return _call_bridge_operation("breakpoint_record.remove", {"index": index})


@mcp.tool()
def android_breakpoint_record_update(index: int, field: str, value: int | str) -> dict[str, Any]:
    """Patch one saved hardware breakpoint register field; backend sets the write mask before writing the field."""
    normalized = f"0x{value:X}" if isinstance(value, int) else str(value).strip()
    return _call_bridge_operation("breakpoint_record.update", {"index": index, "field": field, "value": normalized})


@mcp.tool()
def android_syscall_start() -> dict[str, Any]:
    """Start syscall monitoring and return the currently available log."""
    response = _call_bridge_operation("syscall.start")
    log_response = _call_bridge_operation("syscall.read")
    log_data = log_response.get("data")
    data = response.setdefault("data", {})
    if isinstance(data, dict) and isinstance(log_data, dict):
        data["log"] = log_data.get("log", "")
        data["line_count"] = log_data.get("line_count", 0)
    return response


@mcp.tool()
def android_syscall_stop() -> dict[str, Any]:
    """Stop syscall monitoring for the current target PID."""
    return _call_bridge_operation("syscall.stop")


@mcp.tool()
def android_syscall_log() -> dict[str, Any]:
    """Read all currently available lsdriver syscall log lines."""
    return _call_bridge_operation("syscall.read")


@mcp.tool()
def android_signature_scan_address(
    address: int | str,
    range_size: int,
    file_name: str = "Signature.txt",
) -> dict[str, Any]:
    """Generate and save a signature around one target address."""
    return _call_bridge_operation(
        "signature.create",
        {"address": format_address(address), "range": range_size, "file_name": file_name},
    )


@mcp.tool()
def android_signature_scan_file(file_name: str = "Signature.txt") -> dict[str, Any]:
    """Scan the target process using every signature stored in a file."""
    return _call_bridge_operation("signature.scan", {"file_name": file_name})


@mcp.tool()
def android_signature_scan_pattern(pattern: str, range_offset: int = 0) -> dict[str, Any]:
    """Scan the target process with one signature pattern and address offset."""
    return _call_bridge_operation(
        "signature.match",
        {"pattern": pattern, "range_offset": range_offset},
    )


@mcp.tool()
def android_signature_filter(address: int | str, file_name: str = "Signature.txt") -> dict[str, Any]:
    """Update a signature file by filtering changed bytes at the supplied address."""
    return _call_bridge_operation(
        "signature.filter",
        {"address": format_address(address), "file_name": file_name},
    )


def _normalize_http_path(path: str) -> str:
    normalized = str(path).strip() or "/mcp"
    if not normalized.startswith("/"):
        normalized = "/" + normalized
    if len(normalized) > 1:
        normalized = normalized.rstrip("/")
    return normalized or "/mcp"


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Expose the NativeHttpBridge Android bridge as an MCP server.",
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
        "--android-host",
        default=DEFAULT_ANDROID_HOST,
        help=(
            "Target Android IP/host, a full HTTP(S) Tunnel URL, or 'auto' for LAN discovery. "
            f"Bare hosts use port {DEFAULT_ANDROID_PORT}."
        ),
    )
    parser.add_argument(
        "--android-timeout",
        type=float,
        default=DEFAULT_ANDROID_TIMEOUT_SECONDS,
        help="Timeout in seconds for Android HTTP bridge requests.",
    )
    return parser


def _format_http_endpoint(host: str, port: int, path: str) -> str:
    display_host = "127.0.0.1" if host == "0.0.0.0" else host
    return f"http://{display_host}:{port}{path}"


def _emit_startup_info(runtime: dict[str, Any]) -> None:
    print("[MCP] Server started:", file=sys.stderr, flush=True)
    print(f"  Streamable HTTP: {runtime['streamable_http_url']}", file=sys.stderr, flush=True)


def _configure_runtime(args: argparse.Namespace) -> dict[str, Any]:
    bridge.configure(
        host=args.android_host,
        timeout_seconds=args.android_timeout,
    )

    mcp.settings.host = args.mcp_host.strip() or DEFAULT_MCP_BIND_HOST
    mcp.settings.port = int(args.mcp_port)
    mcp.settings.streamable_http_path = _normalize_http_path(args.mcp_path)

    return {
        "streamable_http_url": _format_http_endpoint(
            mcp.settings.host,
            mcp.settings.port,
            mcp.settings.streamable_http_path,
        ),
    }


def main() -> int:
    parser = _build_arg_parser()
    args = parser.parse_args()
    runtime = _configure_runtime(args)
    _emit_startup_info(runtime)
    mcp.run(transport="streamable-http")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
