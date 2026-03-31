#!/bin/bash
set -euo pipefail

# ==============================================================
#  Android 内核驱动 (lsdriver.ko) 多版本批量编译脚本
#  修复版：不会自删、安全清理
# ==============================================================

# -------------------------- 核心配置 --------------------------
BUILD_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CLANG_ROOT="/opt/ddk/clang"
KERNELS_ROOT="/opt/ddk/kdir"
DRIVER_SRC="/mnt/c/home/wen/Linux-android-arm64/lsdriver"
NO_STRIP_VERSIONS=("android16-6.12" "android15-6.6")

# -------------------------- 颜色定义 --------------------------
GREEN='\e[32m'
RED='\e[31m'
YELLOW='\e[33m'
BLUE='\e[34m'
NC='\e[0m'

# -------------------------- 全局变量 --------------------------
declare -a BUILD_RESULTS=()
STRIP_CHOICE="n"

# -------------------------- 日志函数 --------------------------
log_info()  { echo -e "${GREEN}$*${NC}"; }
log_warn()  { echo -e "${YELLOW}$*${NC}"; }
log_error() { echo -e "${RED}$*${NC}"; }
log_title() { echo -e "${BLUE}====================================================${NC}"; }

# -------------------------- 清理编译缓存（修复版！不会删脚本） --------------------------

# 清理驱动源码目录下的编译缓存，保留源码和所有 .ko
clean_driver_build() {
    if [[ ! -d "$DRIVER_SRC" ]]; then
        log_error "driver source directory not found: $DRIVER_SRC"
        return 1
    fi

    # 保留源码/构建定义文件和所有已生成的 .ko，其余全部视为生成物清理掉
    find "$DRIVER_SRC" -mindepth 1 \( \
        -type f ! \( \
            -name 'Makefile' -o \
            -name 'Kconfig' -o \
            -name '*.c' -o \
            -name '*.h' -o \
            -name '*.lds' -o \
            -name '*.S' -o \
            -name '*.s' -o \
            -name '*.ko' \
        \) -o \
        -type d -empty \
    \) -delete
}

# -------------------------- 处理编译产物 --------------------------
handle_output() {
    local version="$1"
    local source_ko="$DRIVER_SRC/lsdriver.ko"
    local target_ko="$DRIVER_SRC/${version}lsdriver.ko"
    if [[ ! -f "$source_ko" ]]; then
        log_error "❌ $version 编译失败（未生成 .ko 文件）"
        BUILD_RESULTS+=("$version: ❌ 编译失败")
        return 1
    fi

    local force_no_strip=false
    for v in "${NO_STRIP_VERSIONS[@]}"; do
        [[ "$version" == "$v" ]] && force_no_strip=true && break
    done

    if [[ "$force_no_strip" == "true" ]]; then
        log_warn "⚠️ $version 强制保留符号（剥离后无法加载）"
        cp "$source_ko" "$target_ko"
    elif [[ "$STRIP_CHOICE" == "y" || "$STRIP_CHOICE" == "Y" ]]; then
        if command -v llvm-strip &>/dev/null; then
            log_info "🔧 剥离 $version 符号..."
            llvm-strip --strip-debug -o "$target_ko" "$source_ko"
        else
            log_warn "⚠️ 未找到 llvm-strip，直接复制"
            cp "$source_ko" "$target_ko"
        fi
    else
        cp "$source_ko" "$target_ko"
    fi

    BUILD_RESULTS+=("$version: ✅ 编译成功")
    log_info "✅ 生成产物：$target_ko"
}

# -------------------------- 核心编译函数 --------------------------
build_kernel() {
    local version="$1"
    local clang_path="$2"
    local cross_compile="$3"

    local kernel_dir="$KERNELS_ROOT/$version"
    log_title
    log_warn "开始编译：$version"

    if [[ ! -d "$kernel_dir" ]]; then
        log_error "❌ 内核目录不存在：$kernel_dir"
        BUILD_RESULTS+=("$version: ❌ 目录不存在")
        return 1
    fi

    clean_driver_build

    # 临时禁用 BTF
    if [[ -f "$kernel_dir/.config" ]]; then
        sed -i 's/CONFIG_DEBUG_INFO_BTF=y/CONFIG_DEBUG_INFO_BTF=n/g' "$kernel_dir/.config"
    fi

    log_info "🚀 执行编译命令..."
    export PATH="$clang_path:$PATH"
    make -C "$kernel_dir" \
        M="$DRIVER_SRC" \
        ARCH=arm64 \
        CROSS_COMPILE="$cross_compile" \
        LLVM=1 \
        LLVM_IAS=1 \
        PATH="$PATH" \
        CONFIG_DEBUG_INFO_BTF=n \
        modules -j"$(nproc)"

    handle_output "$version"
}

# -------------------------- 主函数 --------------------------
main() {
    log_warn "是否剥离符号？(y=剥离/减小体积，n=保留/调试用)"
    read -rp "请输入 (y/n，默认 n): " input
    [[ "$input" =~ ^[Yy]$ ]] && STRIP_CHOICE="y" || STRIP_CHOICE="n"
    readonly STRIP_CHOICE

    build_kernel "android16-6.12" "$CLANG_ROOT/clang-r536225/bin" "aarch64-linux-gnu-"
    build_kernel "android15-6.6" "$CLANG_ROOT/clang-r510928/bin" "aarch64-linux-gnu-"
    build_kernel "android14-6.1" "$CLANG_ROOT/clang-r487747c/bin" "aarch64-linux-gnu-"
    build_kernel "android13-5.15" "$CLANG_ROOT/clang-r450784e/bin" "aarch64-linux-gnu-"
    build_kernel "android13-5.10" "$CLANG_ROOT/clang-r450784e/bin" "aarch64-linux-gnu-"
    build_kernel "android12-5.10" "$CLANG_ROOT/clang-r416183b/bin" "aarch64-linux-gnu-"

    log_title
    echo -e "\n${BLUE}📊 编译结果汇总:${NC}"
    echo "----------------------------------------------------"
    for result in "${BUILD_RESULTS[@]}"; do
        echo -e "  $result"
    done
    echo "----------------------------------------------------"

    echo -e "\n${BLUE}📦 生成的产物列表:${NC}"
    ls -lh "$DRIVER_SRC"/android*.ko 2>/dev/null || log_error "❌ 未找到任何 .ko 产物"

    log_title
    log_info "🎉 批量编译脚本执行完成！"
}

main "$@"