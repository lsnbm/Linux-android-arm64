#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve()
SCRIPT_DIR = SCRIPT_PATH.parent
DEFAULT_OUTPUT_DIR = SCRIPT_DIR
DEFAULT_ICON_PATH = SCRIPT_DIR / "icon.png"
DEFAULT_EXCLUDED_FILES = {"http_bridge.py"}
EXECUTABLE_VERSION = (1, 0, 0, 0)

GUI_MARKERS = (
    "import PySide6",
    "from PySide6",
    "import PyQt5",
    "from PyQt5",
    "import PyQt6",
    "from PyQt6",
    "import tkinter",
    "from tkinter",
    "QApplication(",
)

MCP_MARKERS = (
    "from mcp.server.fastmcp import FastMCP",
    "import mcp",
    "from mcp.",
)


def _ensure_pyinstaller() -> None:
    try:
        import PyInstaller.__main__  # noqa: F401
    except ImportError as exc:  # pragma: no cover - depends on local env
        raise SystemExit(
            "PyInstaller is not installed. Run: python -m pip install pyinstaller typer"
        ) from exc


def _run_pyinstaller(args: list[str]) -> None:
    import PyInstaller.__main__

    PyInstaller.__main__.run(args)


def _read_script_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore")


def _is_gui_script(script_text: str) -> bool:
    return any(marker in script_text for marker in GUI_MARKERS)


def _needs_mcp_bundle(script_text: str) -> bool:
    return any(marker in script_text for marker in MCP_MARKERS)


def _write_version_file(target_name: str, spec_dir: Path) -> Path:
        version_text = ".".join(str(part) for part in EXECUTABLE_VERSION)
        version_file = spec_dir / f"{target_name}.version.txt"
        version_file.write_text(
                f"""VSVersionInfo(
    ffi=FixedFileInfo(
        filevers={EXECUTABLE_VERSION},
        prodvers={EXECUTABLE_VERSION},
        mask=0x3f,
        flags=0x0,
        OS=0x40004,
        fileType=0x1,
        subtype=0x0,
        date=(0, 0)
    ),
    kids=[
        StringFileInfo([
            StringTable('040904B0', [
                StringStruct('FileDescription', {target_name!r}),
                StringStruct('FileVersion', {version_text!r}),
                StringStruct('InternalName', {target_name!r}),
                StringStruct('OriginalFilename', {f'{target_name}.exe'!r}),
                StringStruct('ProductName', {target_name!r}),
                StringStruct('ProductVersion', {version_text!r})
            ])
        ]),
        VarFileInfo([VarStruct('Translation', [1033, 1200])])
    ]
)
""",
                encoding="utf-8",
        )
        return version_file


def _discover_local_python_files() -> list[Path]:
    return [
        path.resolve()
        for path in sorted(SCRIPT_DIR.glob("*.py"))
        if path.resolve() != SCRIPT_PATH and path.name not in DEFAULT_EXCLUDED_FILES
    ]


def _validate_unique_output_names(targets: list[Path]) -> None:
    names: dict[str, Path] = {}
    for target in targets:
        key = target.stem.casefold()
        previous = names.get(key)
        if previous is not None:
            raise SystemExit(
                f"Targets produce the same executable name '{target.stem}.exe': "
                f"{previous} and {target}"
            )
        names[key] = target


def _resolve_target(path_text: str) -> Path:
    raw_path = Path(path_text)
    candidates = []
    if raw_path.is_absolute():
        candidates.append(raw_path)
    else:
        candidates.append((Path.cwd() / raw_path).resolve())
        candidates.append((SCRIPT_DIR / raw_path).resolve())

    for candidate in candidates:
        if candidate.exists():
            if candidate.is_dir():
                raise SystemExit(f"Target is a directory, not a .py file: {candidate}")
            if candidate.suffix.lower() != ".py":
                raise SystemExit(f"Target is not a .py file: {candidate}")
            return candidate

    raise SystemExit(f"Python file not found: {path_text}")


def _resolve_targets(raw_targets: list[str]) -> list[Path]:
    if not raw_targets:
        targets = _discover_local_python_files()
        if not targets:
            raise SystemExit(f"No .py files found in {SCRIPT_DIR}")
        _validate_unique_output_names(targets)
        return targets

    resolved: list[Path] = []
    seen: set[Path] = set()
    for raw_target in raw_targets:
        target = _resolve_target(raw_target)
        if target.resolve() == SCRIPT_PATH:
            continue
        if target in seen:
            continue
        seen.add(target)
        resolved.append(target)

    if not resolved:
        raise SystemExit("No valid .py files to build.")
    _validate_unique_output_names(resolved)
    return resolved


def _build_script(
    target: Path,
    dist_dir: Path,
    clean: bool,
    work_root: Path,
    spec_dir: Path,
) -> None:
    script_text = _read_script_text(target)
    target_name = target.stem
    target_work_dir = work_root / target_name
    version_file = _write_version_file(target_name, spec_dir)

    cmd = [
        "--noconfirm",
        "--onefile",
        "--name",
        target_name,
        "--icon",
        str(DEFAULT_ICON_PATH),
        "--version-file",
        str(version_file),
        "--distpath",
        str(dist_dir),
        "--workpath",
        str(target_work_dir),
        "--specpath",
        str(spec_dir),
        "--paths",
        str(target.parent),
        "--paths",
        str(SCRIPT_DIR),
    ]

    if _is_gui_script(script_text):
        cmd.extend(["--add-data", f"{DEFAULT_ICON_PATH}{os.pathsep}."])
        cmd.append("--windowed")

    if _needs_mcp_bundle(script_text):
        cmd.extend(
            [
                "--collect-all",
                "mcp",
                "--hidden-import",
                "uvicorn",
                "--hidden-import",
                "starlette",
            ]
        )

    if clean:
        cmd.append("--clean")

    cmd.append(str(target))

    print(f"[BUILD] {target.name} -> {target_name}.exe")
    _run_pyinstaller(cmd)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Build Windows executables with PyInstaller. "
            "Without parameters, this script builds the executable entry points next to itself."
        )
    )
    parser.add_argument(
        "scripts",
        nargs="*",
        help=(
            "Specific .py files to build. Supports relative or absolute paths. "
            "If omitted, local executable entry points are built."
        ),
    )
    parser.add_argument(
        "--output-dir",
        default=str(DEFAULT_OUTPUT_DIR),
        help="Directory for final .exe files. Default: the script directory.",
    )
    parser.add_argument(
        "--no-clean",
        action="store_true",
        help="Skip PyInstaller --clean.",
    )
    parser.add_argument(
        "--purge-cache",
        action="store_true",
        help="Delete the local build cache before building.",
    )
    parser.add_argument(
        "--pause",
        action="store_true",
        help="Pause and wait for Enter before exiting.",
    )
    parser.add_argument(
        "--list-only",
        action="store_true",
        help="Only print the scripts that would be built, without building them.",
    )
    return parser.parse_args()


def _should_pause(args: argparse.Namespace) -> bool:
    if args.pause:
        return True
    return len(sys.argv) == 1


def _pause_if_needed(should_pause: bool, exit_code: int) -> None:
    if not should_pause:
        return
    status = "completed" if exit_code == 0 else "failed"
    try:
        input(f"\nBuild {status}. Press Enter to exit...")
    except EOFError:
        pass


def main(args: argparse.Namespace) -> int:
    try:
        targets = _resolve_targets(args.scripts)

        print("[INFO] Build targets:")
        for target in targets:
            print(f"  - {target}")

        if args.list_only:
            return 0

        if not DEFAULT_ICON_PATH.is_file():
            raise SystemExit(f"Executable icon not found: {DEFAULT_ICON_PATH}")

        _ensure_pyinstaller()

        dist_dir = Path(args.output_dir).resolve()
        build_root = dist_dir / ".pyinstaller"
        work_root = build_root / "work"
        spec_dir = build_root / "spec"

        if args.purge_cache and build_root.exists():
            shutil.rmtree(build_root)

        dist_dir.mkdir(parents=True, exist_ok=True)
        work_root.mkdir(parents=True, exist_ok=True)
        spec_dir.mkdir(parents=True, exist_ok=True)

        clean = not args.no_clean
        for target in targets:
            _build_script(target, dist_dir, clean, work_root, spec_dir)

        print(f"[DONE] Output directory: {dist_dir}")
        return 0
    except SystemExit as exc:
        message = exc.code if isinstance(exc.code, str) else None
        if message:
            print(message, file=sys.stderr)
        return 1 if exc.code not in (None, 0) else 0
    except Exception as exc:  # pragma: no cover - depends on local env
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    parsed_args = _parse_args()
    exit_code = main(parsed_args)
    _pause_if_needed(_should_pause(parsed_args), exit_code)
    raise SystemExit(exit_code)
