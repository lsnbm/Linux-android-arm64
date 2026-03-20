#!/bin/bash
# Android 驱动打包器 
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
KO_DIR="$SCRIPT_DIR/lsdriver"
OUTPUT_FILE="$SCRIPT_DIR/install_driver.sh"

echo "正在生成脚本: $OUTPUT_FILE ..."

# -------------------------------------------------------
# 1. 写入头部 (Shebang & 变量)
# -------------------------------------------------------
cat > "$OUTPUT_FILE" << 'HEADER_END'
#!/system/bin/sh

# Android 临时目录
TEMP_KO="/data/local/tmp/driver_auto_$$.ko"

# 清理
cleanup() {
    rm -f "$TEMP_KO" 2>/dev/null
}
trap cleanup EXIT
HEADER_END

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


embed_file "$KO_DIR/android14-6.1lsdriver.ko"  "payload_6_1"
embed_file "$KO_DIR/android15-6.6lsdriver.ko"  "payload_6_6"
embed_file "$KO_DIR/android16-6.12lsdriver.ko" "payload_6_12"
embed_file "$KO_DIR/android13-5.15lsdriver.ko" "payload_5_15"
embed_file "$KO_DIR/android12-5.10lsdriver.ko" "payload_android12"
embed_file "$KO_DIR/android13-5.10lsdriver.ko" "payload_android13"

# -------------------------------------------------------
# 3. 核心逻辑 (根据内核字符串匹配)
# -------------------------------------------------------
cat >> "$OUTPUT_FILE" << 'LOGIC_END'

load_driver_logic() {
    local payload_func=$1
    local desc=$2

    echo "=========================================="
    echo "[-] 内核版本: $KERNEL_VER"
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
KERNEL_VER=$(uname -r)

case "$KERNEL_VER" in
    # 6.x 系列
    6.12.*)
        load_driver_logic "payload_6_12" "android16-6.12"
        ;;
    6.6.*)
        load_driver_logic "payload_6_6" "android15-6.6"
        ;;
    6.1.*)
        load_driver_logic "payload_6_1" "android14-6.1"
        ;;
    
    # 5.15 系列
    5.15.*)
        load_driver_logic "payload_5_15" "android13-5.15"
        ;;
    
    # 5.10 系列 (匹配内核名中的 android12 或 android13)
    5.10.*android12*)
        load_driver_logic "payload_android12" "android12-5.10"
        ;;
    5.10.*android13*)
        load_driver_logic "payload_android13" "android13-5.10"
        ;;
    
    # 5.10 兜底 (如果内核名里没写 android 版本，默认试用 13)
    5.10.*)
        echo "[!] 警告: 5.10 内核但未识别到 android12/13 标签。"
        echo "[!] 依次尝试 android13 -> android12..."
        load_driver_logic "payload_android13" "android13-5.10 (fallback)"
        ;;
    
    # 其他
    *)
        echo "[!] 错误: 不支持的内核版本 ($KERNEL_VER)"
        echo "[!] 支持: 5.10 / 5.15 / 6.1 / 6.6 / 6.12"
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
echo "生成完毕！请推送到手机: /data/local/tmp/install_driver.sh"
