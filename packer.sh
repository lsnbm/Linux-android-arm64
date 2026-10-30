#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
KO_DIR="$SCRIPT_DIR/lsdriver"
BYTE_STREAM_DIR="$KO_DIR/byte_stream"

mkdir -p "$BYTE_STREAM_DIR"

mapfile -t ko_files < <(find "$KO_DIR" -maxdepth 1 -type f -name '*.ko' -print)

if [[ ${#ko_files[@]} -eq 0 ]]; then
    echo "错误: $KO_DIR 中没有找到 .ko 文件" >&2
    exit 1
fi

for ko_file in "${ko_files[@]}"; do
    echo "移动: $ko_file -> $BYTE_STREAM_DIR/"
    mv -f -- "$ko_file" "$BYTE_STREAM_DIR/"
done

echo "打包完成: ${#ko_files[@]} 个 .ko 文件已移动到 $BYTE_STREAM_DIR"