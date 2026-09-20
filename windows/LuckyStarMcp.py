#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Annotated, Any, Literal, TypedDict

from mcp.server import MCPServer
from pydantic import Field

PROJECT_WINDOWS_DIR = Path(__file__).resolve().parents[1] / "windows"
if str(PROJECT_WINDOWS_DIR) not in sys.path:
    sys.path.insert(0, str(PROJECT_WINDOWS_DIR))

from http_bridge import (  # noqa: E402
    AndroidHttpClient,
    BridgeResponse,
    DEFAULT_ANDROID_HOST,
    DEFAULT_ANDROID_PORT,
    DEFAULT_ANDROID_TIMEOUT_SECONDS,
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

AndroidPid = Annotated[int, Field(gt=0, le=2_147_483_647)]
BridgePort = Annotated[int, Field(ge=1, le=65_535)]
PositiveTimeout = Annotated[float, Field(gt=0)]
HexAddress = Annotated[
    str,
    Field(strict=True, pattern=r"^0[xX][0-9A-Fa-f]{1,16}$", description="64-bit address as a 0x-prefixed hex string; JSON numbers are not accepted."),
]
HexRegisterValue = Annotated[
    str,
    Field(strict=True, pattern=r"^0[xX][0-9A-Fa-f]{1,32}$", description="Register value as a 0x-prefixed hex string, up to 128 bits; JSON numbers are not accepted."),
]
ScanValueType = Literal["i8", "i16", "i32", "i64", "f32", "f64"]
ScanResultValueType = Literal["i8", "i16", "i32", "i64", "f32", "f64", "string"]
ScanStartMode = Literal["unknown", "equal", "greater", "less", "range", "pointer", "string"]
ScanRefineMode = Literal["equal", "greater", "less", "increased", "decreased", "changed", "unchanged", "range", "pointer", "string"]
SavedValueKind = Literal["numeric", "pointer", "text"]
PointerMode = Literal["module", "manual", "array"]
ViewerFormat = Literal["hexadecimal", "hex", "i8", "i16", "i32", "i64", "f32", "f64", "disasm"]
BreakpointRecordField = Annotated[
    str,
    Field(
        pattern=(
            r"(?i)^(?:(?:(?:op|mask)\.)?"
            r"(?:pc|hit_count|lr|sp|pstate|orig_x0|syscallno|fpsr|fpcr|x(?:[0-9]|[12][0-9])|[qv](?:[0-9]|[12][0-9]|3[01]))"
            r"|mask(?:[0-9]|1[0-7]|[._](?:[0-9]|1[0-7])|\[(?:[0-9]|1[0-7])\]))$"
        )
    ),
]


class AndroidBreakpointPoint(TypedDict):
    address: HexAddress
    bp_type: Annotated[
        Literal["read", "write", "read_write", "execute"],
        Field(description="Breakpoint access type; use execute for an instruction breakpoint."),
    ]
    bp_scope: Annotated[
        Literal["main", "other", "all"],
        Field(description="Thread scope: main thread, other threads, or all threads."),
    ]
    length: Annotated[int, Field(ge=1, le=8, description="Breakpoint length in bytes.")]


_MAX_SAFE_INTEGER = (1 << 53) - 1


def _mcp_response(response: BridgeResponse) -> dict[str, Any]:
    return response.require_ok().to_dict()


def _call_bridge_operation(operation: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
    return _mcp_response(bridge.call_operation(operation, params))


mcp = MCPServer(
    "NativeHttpBridge Android MCP",
    instructions=(
        "Pass addresses and register values as 0x-prefixed strings, never JSON numbers. "
        "Responses encode addresses and registers as hex strings. Counts, lengths and indexes "
        "Counts, lengths and indexes remain numbers. "
        "Scan and saved values remain strings in their requested data format."
    ),
)


@mcp.resource("android://connection")
def android_connection() -> dict[str, Any]:
    """Return the current Android HTTP connection settings used by this MCP server."""
    return bridge.connection_state()


@mcp.tool()
def configure_android_bridge(
    host: str = DEFAULT_ANDROID_HOST,
    port: BridgePort = DEFAULT_ANDROID_PORT,
    timeout_seconds: PositiveTimeout = DEFAULT_ANDROID_TIMEOUT_SECONDS,
) -> dict[str, Any]:
    """Configure the bridge using host='auto', a host/IP plus port, or a full HTTP(S) Tunnel URL."""
    bridge.configure(host=host, port=port, timeout_seconds=timeout_seconds)
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
def android_target_set_pid(pid: AndroidPid) -> dict[str, Any]:
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
    response = _call_bridge_operation("memory.map")
    data = response.get("data")
    if not isinstance(data, dict):
        return response
    data = dict(data)
    data.pop("regions", None)
    data.pop("region_count", None)
    response["data"] = data
    return response


@mcp.tool()
def android_module_address(
    module_name: str,
    segment_index: Annotated[int, Field(ge=0, le=2_147_483_647)] = 0,
    which: Literal["start", "end"] = "start",
) -> dict[str, Any]:
    """Resolve a module segment start or end address from the current target process."""
    return _call_bridge_operation(
        "module.resolve",
        {"module_name": module_name, "segment_index": segment_index, "which": which},
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
    mode: ScanStartMode,
    value_type: ScanValueType = "i32",
    value: str = "",
    range_max: str = "",
) -> dict[str, Any]:
    """Start a scan. equal/greater/less/pointer/string need value; range also needs range_max; unknown needs neither."""
    return _mcp_response(bridge.scan_start(value_type, mode, value, range_max))


@mcp.tool()
def android_memory_scan_refine(
    mode: ScanRefineMode,
    value_type: ScanValueType = "i32",
    value: str = "",
    range_max: str = "",
) -> dict[str, Any]:
    """Refine results. Value modes need value, range also needs range_max, and history modes need neither."""
    return _mcp_response(bridge.scan_refine(value_type, mode, value, range_max))


@mcp.tool()
def android_memory_scan_results(
    start: Annotated[int, Field(ge=0, le=(1 << 53) - 1)] = 0,
    count: Annotated[int, Field(ge=1, le=200)] = 100,
    value_type: ScanResultValueType = "i32",
) -> dict[str, Any]:
    """Read one result page; value_type must match the active scan (string for string scans)."""
    return _call_bridge_operation(
        "scan.results",
        {"start": start, "count": count, "value_type": value_type},
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
def android_memory_read(
    address: HexAddress,
    size: Annotated[int, Field(ge=1, le=1_048_576)],
) -> dict[str, Any]:
    """Read 1 to 1048576 raw bytes from any valid target address."""
    return _call_bridge_operation(
        "memory.read",
        {"address": address, "size": size},
    )


@mcp.tool()
def android_memory_write(address: HexAddress, data_hex: str) -> dict[str, Any]:
    """Write up to 1048576 bytes as even-length hex digits (whitespace allowed, no 0x prefix)."""
    normalized = "".join(str(data_hex).split())
    if not normalized:
        raise ValueError("data_hex must not be empty")
    if len(normalized) % 2 != 0:
        raise ValueError("data_hex must contain complete bytes")
    if len(normalized) // 2 > 1024 * 1024:
        raise ValueError("data_hex must contain at most 1048576 bytes")
    return _call_bridge_operation(
        "memory.write",
        {"address": address, "data_hex": normalized},
    )


@mcp.tool()
def android_saved_list() -> dict[str, Any]:
    """Return the server-owned saved address list with current values and lock states."""
    return _mcp_response(bridge.saved_list())


@mcp.tool()
def android_saved_add(
    address: HexAddress,
    value_type: ScanValueType = "i32",
    value_kind: SavedValueKind = "numeric",
    text_length: Annotated[int, Field(ge=1, le=256)] = 64,
    note: str = "",
) -> dict[str, Any]:
    """Save an address. pointer forces i64; text forces i8 and uses text_length; numeric uses value_type."""
    return _mcp_response(bridge.saved_add(
        address,
        value_type,
        value_kind=value_kind,
        text_length=text_length,
        note=note,
    ))


@mcp.tool()
def android_saved_remove(address: HexAddress) -> dict[str, Any]:
    """Remove one address and its associated saved lock."""
    return _mcp_response(bridge.saved_remove(address))


@mcp.tool()
def android_saved_write(address: HexAddress, value: str) -> dict[str, Any]:
    """Write a saved address using its server-owned type and update its lock value."""
    return _mcp_response(bridge.saved_write(address, value))


@mcp.tool()
def android_saved_set_note(address: HexAddress, note: str) -> dict[str, Any]:
    """Set or clear the note for one server-owned saved address."""
    return _mcp_response(bridge.saved_set_note(address, note))


@mcp.tool()
def android_saved_set_locked(address: HexAddress, locked: bool, value: str = "") -> dict[str, Any]:
    """Set lock state. An empty value locks the current value; value is ignored when unlocking."""
    return _mcp_response(bridge.saved_set_locked(address, locked, value))


@mcp.tool()
def android_saved_offset(offset: str) -> dict[str, Any]:
    """Apply a signed hexadecimal byte offset to all server-owned saved addresses."""
    return _mcp_response(bridge.saved_offset(offset))


@mcp.tool()
def android_saved_clear() -> dict[str, Any]:
    """Clear the server-owned saved address list and associated saved locks."""
    return _mcp_response(bridge.saved_clear())


@mcp.tool()
def android_pointer_status() -> dict[str, Any]:
    """Read current pointer scan task state and preserved result count."""
    return _call_bridge_operation("pointer.get")


@mcp.tool()
def android_pointer_scan(
    target: HexAddress,
    depth: Annotated[int, Field(ge=1, le=16)],
    max_offset: Annotated[int, Field(ge=1, le=2_147_483_647)],
    mode: PointerMode = "module",
    manual_base: HexAddress | None = None,
    array_base: HexAddress | None = None,
    array_count: Annotated[int, Field(ge=1, le=1_000_000)] | None = None,
    module_filter: str = "",
) -> dict[str, Any]:
    """Start a pointer scan. manual requires manual_base; array requires array_base and array_count."""
    mode_token = mode

    params: dict[str, Any] = {
        "target": target,
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
        params["manual_base"] = manual_base
    elif mode_token == "array":
        if array_base is None or array_count is None:
            raise ValueError("array mode requires array_base and array_count")
        params["array_base"] = array_base
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
def android_memory_view_open(address: HexAddress, view_format: ViewerFormat = "hexadecimal") -> dict[str, Any]:
    """Open an address and return its freshly read 100-byte snapshot."""
    return _mcp_response(bridge.viewer_open(address, view_format))


@mcp.tool()
def android_memory_view_offset(offset: str) -> dict[str, Any]:
    """Move by an exact byte offset such as '+0x20' or '-0x10' and return the fresh snapshot."""
    return _call_bridge_operation("viewer.seek", {"offset": offset})


@mcp.tool()
def android_memory_view_set_format(view_format: ViewerFormat) -> dict[str, Any]:
    """Change the viewer format and return the freshly decoded snapshot."""
    return _call_bridge_operation("viewer.format", {"view_format": normalize_view_format(view_format)})


@mcp.tool()
def android_memory_view_read() -> dict[str, Any]:
    """Refresh and return the current 100-byte snapshot; disasm also returns decoded instructions."""
    return _call_bridge_operation("viewer.refresh")


@mcp.tool()
def android_breakpoint_get() -> dict[str, Any]:
    """Return breakpoints with addresses, registers and Q halves as exact 0x strings."""
    return _call_bridge_operation("breakpoint.get")


@mcp.tool()
def android_breakpoint_set(
    mode: Literal["hwbp", "ptebp", "stepbp"],
    points: Annotated[
        list[AndroidBreakpointPoint],
        Field(
            min_length=1,
            max_length=16,
            description=(
                "Array of 1..16 objects. Every object must contain exactly these required fields: "
                "address (nonzero 0x-prefixed string), "
                "bp_type (read|write|read_write|execute), "
                "bp_scope (main|other|all), and length (integer 1..8). "
                "Execution example: "
                "[{\"address\":\"0x7A12345678\",\"bp_type\":\"execute\",\"bp_scope\":\"all\",\"length\":4}]"
            ),
            examples=[
                [
                    {
                        "address": "0x7A12345678",
                        "bp_type": "execute",
                        "bp_scope": "all",
                        "length": 4,
                    }
                ]
            ],
        ),
    ],
) -> dict[str, Any]:
    """Set hwbp/ptebp/stepbp points; use 0x strings for addresses, also returned as 0x strings."""
    return _mcp_response(bridge.breakpoint_set(mode, points))


@mcp.tool()
def android_breakpoint_clear() -> dict[str, Any]:
    """Clear the currently active hwbp, ptebp, or stepbp mode."""
    return _call_bridge_operation("breakpoint.clear")


@mcp.tool()
def android_breakpoint_record_update(
    index: Annotated[int, Field(ge=0, le=2_147_483_647)],
    field: BreakpointRecordField,
    value: HexRegisterValue,
) -> dict[str, Any]:
    """Patch pc, x0..x29, q0..q31, masks or op.<register>; pass 64/128-bit values as 0x strings."""
    return _call_bridge_operation("breakpoint_record.update", {"index": index, "field": field, "value": value})


def _start_monitor(operation: str) -> dict[str, Any]:
    response = _call_bridge_operation(f"{operation}.start")
    log_response = _call_bridge_operation(f"{operation}.read")
    log_data = log_response.get("data")
    data = response.setdefault("data", {})
    if isinstance(data, dict) and isinstance(log_data, dict):
        data["log"] = log_data.get("log", "")
        data["line_count"] = log_data.get("line_count", 0)
    return response


@mcp.tool()
def android_syscall_start() -> dict[str, Any]:
    """Start syscall monitoring and return the currently available log."""
    return _start_monitor("syscall")


@mcp.tool()
def android_syscall_stop() -> dict[str, Any]:
    """Stop syscall monitoring for the current target PID."""
    return _call_bridge_operation("syscall.stop")


@mcp.tool()
def android_syscall_log() -> dict[str, Any]:
    """Read all currently available lsdriver syscall log lines."""
    return _call_bridge_operation("syscall.read")


@mcp.tool()
def android_cntvct_start() -> dict[str, Any]:
    """Start CNTVCT_EL0 read monitoring and return the currently available log."""
    return _start_monitor("cntvct")


@mcp.tool()
def android_cntvct_stop() -> dict[str, Any]:
    """Stop CNTVCT_EL0 read monitoring for the current target PID."""
    return _call_bridge_operation("cntvct.stop")


@mcp.tool()
def android_cntvct_log() -> dict[str, Any]:
    """Read all currently available lsdriver CNTVCT monitor log lines."""
    return _call_bridge_operation("cntvct.read")


@mcp.tool()
def android_signature_scan_address(
    address: HexAddress,
    range_size: Annotated[int, Field(ge=1, le=1200)],
    file_name: str = "Signature.txt",
) -> dict[str, Any]:
    """Generate an Android-side signature using range_size bytes before and after the address."""
    return _call_bridge_operation(
        "signature.create",
        {"address": address, "range": range_size, "file_name": file_name},
    )


@mcp.tool()
def android_signature_scan_file(file_name: str = "Signature.txt") -> dict[str, Any]:
    """Scan using an Android-side signature file; relative names may resolve under /data/akernel/."""
    return _call_bridge_operation("signature.scan", {"file_name": file_name})


@mcp.tool()
def android_signature_scan_pattern(
    pattern: Annotated[str, Field(min_length=1)],
    range_offset: Annotated[int, Field(ge=-2_147_483_648, le=2_147_483_647)] = 0,
) -> dict[str, Any]:
    """Scan a pattern such as '48 8B ?? FFh'; '?' and '??' are wildcard bytes."""
    return _call_bridge_operation(
        "signature.match",
        {"pattern": pattern, "range_offset": range_offset},
    )


@mcp.tool()
def android_signature_filter(address: HexAddress, file_name: str = "Signature.txt") -> dict[str, Any]:
    """Filter changed bytes in an Android-side signature file at the supplied address."""
    return _call_bridge_operation(
        "signature.filter",
        {"address": address, "file_name": file_name},
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Expose the NativeHttpBridge Android bridge as an MCP server.",
    )
    parser.add_argument("--mcp-host", default=DEFAULT_MCP_BIND_HOST, help="Bind host for the local MCP web server.")
    parser.add_argument("--mcp-port", type=int, default=DEFAULT_MCP_BIND_PORT, help="Bind port for the local MCP web server.")
    parser.add_argument("--mcp-path", default=DEFAULT_MCP_PATH, help="HTTP endpoint path for streamable-http clients.")
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
    args = parser.parse_args()

    bridge.configure(host=args.android_host, timeout_seconds=args.android_timeout)
    mcp_host = args.mcp_host.strip() or DEFAULT_MCP_BIND_HOST
    mcp_port = int(args.mcp_port)
    mcp_path = str(args.mcp_path).strip() or "/mcp"
    if not mcp_path.startswith("/"):
        mcp_path = "/" + mcp_path
    if len(mcp_path) > 1:
        mcp_path = mcp_path.rstrip("/")
    mcp_path = mcp_path or "/mcp"

    display_host = "127.0.0.1" if mcp_host == "0.0.0.0" else mcp_host
    print("[MCP] Server started:", file=sys.stderr, flush=True)
    print(
        f"  Streamable HTTP: http://{display_host}:{mcp_port}{mcp_path}",
        file=sys.stderr,
        flush=True,
    )
    mcp.run(
        transport="streamable-http",
        host=mcp_host,
        port=mcp_port,
        streamable_http_path=mcp_path,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
