#!/bin/bash
# Android 驱动打包器
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
KO_DIR="$SCRIPT_DIR/lsdriver"
FINAL_OUTPUT_FILE="$SCRIPT_DIR/install_driver.sh"
VERSION_FILE="$SCRIPT_DIR/install_driver.version"
OUTPUT_FILE="${FINAL_OUTPUT_FILE}.tmp.$$"
VERSION_TEMP_FILE="${VERSION_FILE}.tmp.$$"

cleanup() {
    rm -f "$OUTPUT_FILE" "$VERSION_TEMP_FILE"
}
trap cleanup EXIT

CURRENT_VERSION="0.10.12"
if [[ -f "$VERSION_FILE" ]]; then
    CURRENT_VERSION="$(tr -d '\r\n' < "$VERSION_FILE")"
fi
if [[ ! "$CURRENT_VERSION" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
    echo "错误: $VERSION_FILE 必须使用 x.y.z 格式" >&2
    exit 1
fi

VERSION_MAJOR=$((10#${BASH_REMATCH[1]}))
VERSION_MINOR=$((10#${BASH_REMATCH[2]}))
VERSION_PATCH=$((10#${BASH_REMATCH[3]}))
if ((VERSION_MAJOR > 12 || VERSION_MINOR > 12 || VERSION_PATCH > 12)); then
    echo "错误: 版本号每一段都必须在 0 到 12 之间" >&2
    exit 1
fi

PACKAGE_VERSION="$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH"
if ((VERSION_PATCH < 12)); then
    ((VERSION_PATCH += 1))
elif ((VERSION_MINOR < 12)); then
    VERSION_PATCH=0
    ((VERSION_MINOR += 1))
elif ((VERSION_MAJOR < 12)); then
    VERSION_PATCH=0
    VERSION_MINOR=0
    ((VERSION_MAJOR += 1))
else
    echo "错误: 版本号已经达到上限 12.12.12" >&2
    exit 1
fi
NEXT_VERSION="$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH"

echo "正在生成脚本: $FINAL_OUTPUT_FILE (版本 $PACKAGE_VERSION) ..."

# -------------------------------------------------------
# 1. 写入头部 (Shebang & 变量)
# -------------------------------------------------------
{
cat << 'HEADER_END'
#!/system/bin/sh
HEADER_END
printf '\nINSTALL_DRIVER_VERSION=%s\n' "$PACKAGE_VERSION"
cat << 'HEADER_END'

# Android 临时目录
TEMP_KO="/data/local/tmp/driver_auto_$$.ko"

# 清理
cleanup() {
    rm -f "$TEMP_KO" 2>/dev/null
}
trap cleanup EXIT
HEADER_END
} > "$OUTPUT_FILE"

# -------------------------------------------------------
# 2. 嵌入数据函数 (必须放在逻辑执行之前！)
# -------------------------------------------------------
embed_file() {
    local filename="$1"
    local funcname="$2"
    
    echo -n "打包: $filename -> $funcname ... "
    
    if [ -f "$filename" ]; then
        echo "" >> "$OUTPUT_FILE"
        echo "$funcname() {" >> "$OUTPUT_FILE"
        echo "cat << 'B64EOF'" >> "$OUTPUT_FILE"
        base64 "$filename" >> "$OUTPUT_FILE"
        echo "B64EOF" >> "$OUTPUT_FILE"
        echo "}" >> "$OUTPUT_FILE"
        echo "OK"
    else
        echo "跳过 (文件不存在)"
        echo "$funcname() { echo ''; }" >> "$OUTPUT_FILE"
    fi
}


embed_file "$KO_DIR/6.1-Android14.ko"  "payload_6_1"
embed_file "$KO_DIR/6.6-Android15.ko"  "payload_6_6"
embed_file "$KO_DIR/6.12-Android16.ko" "payload_6_12"
embed_file "$KO_DIR/6.18-Android17.ko" "payload_6_18"
embed_file "$KO_DIR/5.15-Android13.ko" "payload_5_15"
embed_file "$KO_DIR/5.10-Android12.ko" "payload_android12"
embed_file "$KO_DIR/5.10-Android13.ko" "payload_android13"

# -------------------------------------------------------
# 3. 核心逻辑 (根据内核字符串匹配)
# -------------------------------------------------------
cat >> "$OUTPUT_FILE" << 'LOGIC_END'

load_driver_logic() {
    local payload_func=$1
    local desc=$2

    echo "=========================================="
    printf '[-] 安装脚本版本: \033[1;93m%s\033[0m\n' "$INSTALL_DRIVER_VERSION"
    echo "[-] 内核版本: $KERNEL_VER"
    echo "[-] 系统指纹: $BUILD_FINGERPRINT"
    echo "[-] 匹配分支: $desc"
    echo "[-] 提取位置: $TEMP_KO"

    # 提取 (调用上方已定义的函数)
    $payload_func | base64 -d > "$TEMP_KO" 2>/dev/null
    
    if [ ! -s "$TEMP_KO" ]; then
        echo "[!] 错误: 提取失败 (文件为空)！"
        echo "    请检查对应版本的 ko 文件是否已打包。"
        exit 1
    fi

    echo "[-] 正在加载..."
    dmesg -c >/dev/null 2>&1
    
    if OUTPUT=$(insmod "$TEMP_KO" 2>&1); then
        echo "[+] 成功: 驱动已加载！"
        echo "=========================================="
        exit 0
    else
        echo "[!] 失败！"
        echo ">>> insmod 报错:"
        echo "$OUTPUT"
        echo ">>> dmesg 日志:"
        dmesg | tail -n 10
        echo "=========================================="
        exit 1
    fi
}

# --- 主入口 ---
if [ "${1:-}" = "--version" ]; then
    echo "$INSTALL_DRIVER_VERSION"
    exit 0
fi

KERNEL_VER=$(uname -r)
BUILD_FINGERPRINT=$(getprop ro.build.fingerprint 2>/dev/null)

if [ -z "$BUILD_FINGERPRINT" ]; then
    BUILD_FINGERPRINT="unknown"
fi

case "$KERNEL_VER" in
    # 6.x 系列
    6.18.*)
        load_driver_logic "payload_6_18" "6.18-Android17"
        ;;
    6.12.*)
        load_driver_logic "payload_6_12" "6.12-Android16"
        ;;
    6.6.*)
        load_driver_logic "payload_6_6" "6.6-Android15"
        ;;
    6.1.*)
        load_driver_logic "payload_6_1" "6.1-Android14"
        ;;
    
    # 5.15 系列
    5.15.*)
        load_driver_logic "payload_5_15" "5.15-Android13"
        ;;
    
    # 5.10 系列 (匹配内核名中的 android12 或 android13)
    5.10.*android12*)
        load_driver_logic "payload_android12" "5.10-Android12"
        ;;
    5.10.*android13*)
        load_driver_logic "payload_android13" "5.10-Android13"
        ;;
    
    # 5.10 兜底 (如果内核名里没写 android 版本，默认试用 13)
    5.10.*)
        echo "[!] 警告: 5.10 内核但未识别到 android12/13 标签。"
        echo "[!] 依次尝试 android13 -> android12..."
        load_driver_logic "payload_android13" "5.10-Android13 (fallback)"
        ;;
    
    # 其他
    *)
        echo "[!] 错误: 不支持的内核版本 ($KERNEL_VER)"
        echo "[!] 支持: 5.10 / 5.15 / 6.1 / 6.6 / 6.12 / 6.18"
        exit 1
        ;;
esac
exit 0
LOGIC_END

# 4. 格式修复 (防止 Windows 换行符导致的 function not found)
if command -v sed >/dev/null 2>&1; then
    sed -i 's/\r//g' "$OUTPUT_FILE"
fi

chmod +x "$OUTPUT_FILE"
mv -f "$OUTPUT_FILE" "$FINAL_OUTPUT_FILE"
printf '%s\n' "$NEXT_VERSION" > "$VERSION_TEMP_FILE"
mv -f "$VERSION_TEMP_FILE" "$VERSION_FILE"
trap - EXIT

echo "生成完毕！版本: $PACKAGE_VERSION"
echo "请推送到手机: /data/local/tmp/install_driver.sh"
