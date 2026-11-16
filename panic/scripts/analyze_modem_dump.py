#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""解析 Qualcomm modem enhanced dump 的 ELF section，并尝试识别可解压内容。"""

from __future__ import annotations

import argparse
import bz2
import gzip
import json
import lzma
import math
import re
import struct
import zlib
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Callable


PRINTABLE_RE = re.compile(rb"[ -~]{6,}")
KEYWORDS = [
    b"DSDA",
    b"TxS",
    b"Switch",
    b"Seq",
    b"NR-GM",
    b"scheduling",
    b"invalid",
    b"trigger",
    b"reason",
    b"watchdog",
    b"wdt",
    b"panic",
    b"fatal",
    b"assert",
    b"abort",
    b"crash",
    b"mpss",
    b"qsh",
    b"modem",
    b"error",
    b"fail",
    b"reset",
]


@dataclass
class Section:
    index: int
    name: str
    type: int
    flags: int
    addr: int
    offset: int
    size: int
    link: int
    info: int
    addralign: int
    entsize: int


def read_c_string(data: bytes, offset: int) -> str:
    end = data.find(b"\0", offset)
    if end < 0:
        end = len(data)
    return data[offset:end].decode("ascii", errors="replace")


def parse_elf64_sections(data: bytes) -> list[Section]:
    if len(data) < 64 or data[:4] != b"\x7fELF":
        raise ValueError("不是 ELF 文件")
    if data[4] != 2:
        raise ValueError("不是 ELF64 文件")
    if data[5] != 1:
        raise ValueError("当前脚本只处理小端 ELF")

    header = struct.unpack_from("<16sHHIQQQIHHHHHH", data, 0)
    shoff = header[6]
    shentsize = header[11]
    shnum = header[12]
    shstrndx = header[13]
    if shentsize != 64:
        raise ValueError(f"异常 section header 大小: {shentsize}")

    raw_sections = []
    for index in range(shnum):
        off = shoff + index * shentsize
        fields = struct.unpack_from("<IIQQQQIIQQ", data, off)
        raw_sections.append((index, *fields))

    shstr = raw_sections[shstrndx]
    shstr_offset = shstr[5]
    shstr_size = shstr[6]
    names = data[shstr_offset : shstr_offset + shstr_size]

    sections: list[Section] = []
    for raw in raw_sections:
        index, name_off, sec_type, flags, addr, offset, size, link, info, addralign, entsize = raw
        name = read_c_string(names, name_off) if name_off < len(names) else "<bad>"
        sections.append(
            Section(index, name, sec_type, flags, addr, offset, size, link, info, addralign, entsize)
        )
    return sections


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = [0] * 256
    for value in data:
        counts[value] += 1
    result = 0.0
    length = len(data)
    for count in counts:
        if count:
            probability = count / length
            result -= probability * math.log2(probability)
    return result


def extract_strings(data: bytes, limit: int = 80) -> list[str]:
    out = []
    for match in PRINTABLE_RE.finditer(data):
        value = match.group(0).decode("ascii", errors="replace")
        out.append(value)
        if len(out) >= limit:
            break
    return out


def keyword_counts(data: bytes) -> dict[str, int]:
    lower = data.lower()
    counts = {}
    for keyword in KEYWORDS:
        count = lower.count(keyword.lower())
        if count:
            counts[keyword.decode("ascii", errors="replace")] = count
    return counts


def try_decompress(name: str, data: bytes, out_dir: Path) -> list[dict[str, object]]:
    attempts: list[tuple[str, Callable[[bytes], bytes]]] = [
        ("zlib", zlib.decompress),
        ("gzip", gzip.decompress),
        ("bz2", bz2.decompress),
        ("xz_lzma", lzma.decompress),
    ]
    results = []
    for label, func in attempts:
        try:
            decompressed = func(data)
        except Exception as exc:  # noqa: BLE001 - 这里需要记录所有失败原因
            results.append({"method": label, "ok": False, "error": str(exc)[:120]})
            continue
        target = out_dir / f"{name}.{label}.decompressed.bin"
        target.write_bytes(decompressed)
        results.append(
            {
                "method": label,
                "ok": True,
                "size": len(decompressed),
                "entropy": round(entropy(decompressed), 4),
                "output": str(target),
                "strings": extract_strings(decompressed, 20),
            }
        )
    return results


def parse_mss_enc_meta(data: bytes) -> dict[str, object]:
    values = list(struct.unpack_from("<" + "I" * (len(data) // 4), data, 0))
    return {
        "u32_hex": [f"0x{value:08X}" for value in values],
        "u32_dec": values,
        "guess": {
            "version_or_enabled": values[0] if values else None,
            "possible_item_count": values[9] if len(values) > 9 else None,
            "possible_plain_size": values[12] if len(values) > 12 else None,
            "possible_encoded_size": values[13] if len(values) > 13 else None,
        },
    }


def analyze_dump(input_path: Path, out_dir: Path) -> dict[str, object]:
    data = input_path.read_bytes()
    sections = parse_elf64_sections(data)
    out_dir.mkdir(parents=True, exist_ok=True)
    section_dir = out_dir / "sections"
    section_dir.mkdir(exist_ok=True)

    report: dict[str, object] = {
        "input": str(input_path),
        "size": len(data),
        "sections": [],
        "keyword_counts": keyword_counts(data),
        "mss_enc_meta": None,
        "decompress_attempts": {},
        "interesting_strings": extract_strings(data, 120),
    }

    for section in sections:
        if section.size == 0:
            continue
        section_data = data[section.offset : section.offset + section.size]
        safe_name = section.name.replace("/", "_") or f"section_{section.index}"
        section_path = section_dir / f"{section.index:02d}_{safe_name}.bin"
        section_path.write_bytes(section_data)
        section_info = asdict(section)
        section_info.update(
            {
                "entropy": round(entropy(section_data), 4),
                "output": str(section_path),
                "keyword_counts": keyword_counts(section_data),
                "strings": extract_strings(section_data, 20),
            }
        )
        report["sections"].append(section_info)

        if section.name == "mss_enc_meta":
            report["mss_enc_meta"] = parse_mss_enc_meta(section_data)
        if section.name in {"mss_enc_content", "mss_compr_ext", "QSH_MDUMP"}:
            report["decompress_attempts"][section.name] = try_decompress(
                section.name, section_data, out_dir
            )

    report_path = out_dir / "report.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def print_summary(report: dict[str, object], report_path: Path) -> None:
    print(f"输入文件: {report['input']}")
    print(f"文件大小: {report['size']} 字节")
    print("\n关键词命中:")
    for key, value in report["keyword_counts"].items():
        print(f"  {key}: {value}")

    print("\n最大 section:")
    sections = sorted(report["sections"], key=lambda item: item["size"], reverse=True)
    for section in sections[:12]:
        print(
            f"  {section['name']}: size={section['size']} entropy={section['entropy']} "
            f"strings={len(section['strings'])}"
        )

    print("\nmss_enc_meta:")
    meta = report.get("mss_enc_meta")
    if meta:
        print("  " + " ".join(meta["u32_hex"]))
        print(f"  猜测字段: {meta['guess']}")
    else:
        print("  未找到")

    print("\n解压尝试:")
    attempts = report["decompress_attempts"]
    for section_name, rows in attempts.items():
        print(f"  {section_name}:")
        for row in rows:
            if row["ok"]:
                print(f"    {row['method']}: 成功 size={row['size']} output={row['output']}")
            else:
                print(f"    {row['method']}: 失败 {row['error']}")

    print("\n报告文件:")
    print("  " + str(report_path))


def main() -> int:
    parser = argparse.ArgumentParser(description="解析 Qualcomm modem enhanced dump")
    parser.add_argument("input", type=Path, help="modem_enhance_dump.elf 路径")
    parser.add_argument(
        "-o",
        "--out",
        type=Path,
        default=Path("modem_dump_analysis"),
        help="输出目录，默认 modem_dump_analysis",
    )
    args = parser.parse_args()

    report = analyze_dump(args.input, args.out)
    print_summary(report, args.out / "report.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
