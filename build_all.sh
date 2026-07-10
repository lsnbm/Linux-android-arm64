#!/bin/bash

# ==============================================================
#  Android 内核驱动 (lsdriver.ko) 多版本批量编译脚本
# ==============================================================
#
#  支持的内核版本:
#    Bazel 构建:  6.12-Android16 / 6.6-Android15 / 6.1-Android14 / 5.15-Android13 / 5.10-Android13
#    Legacy 构建: 5.10-Android12
#
#  用法:
#    chmod +x build.sh && ./build.sh
#
# ==============================================================

set -euo pipefail

BUILD_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

# 内核源码根目录 (各版本源码存放在此目录下的子文件夹)
KERNELS_ROOT="/root"

# 驱动源码路径
DRIVER_SRC="/mnt/e/1.CodeRepository/Android/Kernel/lsdriver"

# 强制不剥离符号的版本列表 (剥离后无法加载)
NO_STRIP_VERSIONS=("6.12-Android16" "6.6-Android15")

# 编译外部模块时不导入内核导出的 Module.symvers，生成空 __versions 的版本列表。
# 这样 ko 的 vermagic 仍然带 modversions，但不会携带具体符号 CRC
NO_CRC_VERSIONS=(
    "6.12-Android16"
    "6.6-Android15"
    "6.1-Android14"
    "5.15-Android13"
    "5.10-Android13"
    "5.10-Android12"
)


GREEN='\e[32m'
RED='\e[31m'
YELLOW='\e[33m'
BLUE='\e[34m'
NC='\e[0m'

# 编译结果收集
declare -a BUILD_RESULTS=()


log_info()  { echo -e "${GREEN}$*${NC}"; }
log_warn()  { echo -e "${YELLOW}$*${NC}"; }
log_error() { echo -e "${RED}$*${NC}"; }
log_title() { echo -e "${BLUE}====================================================${NC}"; }

contains_version() {
    local version="$1"
    shift

    local item
    for item in "$@"; do
        [[ "$version" == "$item" ]] && return 0
    done

    return 1
}

fix_empty_ext_modversions() {
    local mod_c="$DRIVER_SRC/lsdriver.mod.c"

    if [[ ! -f "$mod_c" ]]; then
        return 1
    fi

    # Android 16 / 6.12 开启扩展 modversions 时，空 CRC 构建可能生成非法的：
    #   ____version_ext_names[] = ;
    # 这里把它修成空字符串，让最后的 mod.o/ko 链接继续完成。
    if grep -q '__section("__version_ext_names")' "$mod_c" && \
       grep -q '^[[:space:]]*;[[:space:]]*$' "$mod_c"; then
        perl -0pi -e 's/(__used __section\("__version_ext_names"\) =\n);/$1"";/' "$mod_c"
        return 0
    fi

    return 1
}

package_driver() {
    local packer="$BUILD_ROOT/packer.sh"

    if [[ ! -x "$packer" ]]; then
        log_warn "packer.sh 不可执行，尝试直接调用 bash ..."
    fi

    log_warn "开始自动打包 install_driver.sh ..."
    bash "$packer"
}

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

# 处理编译产物: 剥离符号 / 复制 / 重命名
# 参数: $1=版本名  $2=clang工具链路径(可选, 为空则从PATH找)
handle_output() {
    local version="$1"
    local clang_path="${2:-}"
    local source_ko="$DRIVER_SRC/lsdriver.ko"
    local target_ko="$DRIVER_SRC/${version}.ko"

    if [[ ! -f "$source_ko" ]]; then
        log_error "编译 $version 失败! (未生成 ko 文件)"
        BUILD_RESULTS+=("$version: ❌ 编译失败")
        return 1
    fi

    # 检查是否属于强制不剥离版本
    local force_no_strip=false
    if contains_version "$version" "${NO_STRIP_VERSIONS[@]}"; then
        force_no_strip=true
    fi

    if [[ "$force_no_strip" == "true" ]]; then
        log_warn "注意: 版本 $version 强制保留符号 (剥离后无法加载)"
        cp "$source_ko" "$target_ko"
    elif [[ "$STRIP_CHOICE" == "y" || "$STRIP_CHOICE" == "Y" ]]; then
        # 确定 strip 工具路径
        local strip_cmd=""
        if [[ -n "$clang_path" && -x "$clang_path/bin/llvm-strip" ]]; then
            strip_cmd="$clang_path/bin/llvm-strip"
        elif command -v llvm-strip &>/dev/null; then
            strip_cmd="llvm-strip"
        else
            log_warn "未找到 llvm-strip，跳过剥离"
            cp "$source_ko" "$target_ko"
            BUILD_RESULTS+=("$version: ✅ (未剥离, 工具缺失)")
            log_info "生成完成: $target_ko"
            return 0
        fi
        log_info "正在剥离符号..."
        "$strip_cmd" --strip-debug -o "$target_ko" "$source_ko"
    else
        log_info "保留符号，创建副本..."
        cp "$source_ko" "$target_ko"
    fi

    BUILD_RESULTS+=("$version: ✅")
    log_info "生成完成: $target_ko"
}

# ======================== Bazel 构建 =========================
# 适用于: Android 13+ 内核 (6.12-Android16 / 6.6-Android15 / 6.1-Android14 / 5.15-Android13 / 5.10-Android13)

build_kernel() {
    local version="$1"
    local clang_path="$2"
    local cross_prefix="$3"
    local extra_params="${4:-}"

    local kernel_dir="$KERNELS_ROOT/$version"

    log_title
    log_warn "正在开始编译内核版本: $version"

    if [[ ! -d "$kernel_dir" ]]; then
        log_error "错误: 找不到内核目录 $kernel_dir"
        BUILD_RESULTS+=("$version: ❌ 目录不存在")
        return
    fi

    log_warn "正在清理旧的构建产物..."
    clean_driver_build

    # --- 准备 Bazel 环境 ---
    cd "$kernel_dir" || return
    local bazel_out
    bazel_out=$(readlink -f bazel-bin/common/kernel_aarch64 2>/dev/null || true)

    if [[ -z "$bazel_out" || ! -d "$bazel_out" ]]; then
        log_warn "检测到 $version 未进行内核编译，尝试 Bazel build..."
        tools/bazel build //common:kernel_aarch64 //common:kernel_aarch64_modules_prepare
        bazel_out=$(readlink -f bazel-bin/common/kernel_aarch64)
    fi

    # 解压 modules_prepare 环境
    cd "$bazel_out" || return
    if [[ -f "../kernel_aarch64_modules_prepare/modules_prepare_outdir.tar.gz" ]]; then
        tar -xzf ../kernel_aarch64_modules_prepare/modules_prepare_outdir.tar.gz
    fi

    # --- 编译模块 ---
    cd "$kernel_dir" || return

    log_info "执行 Make 编译 ($version)..."

    local symvers_file="$bazel_out/Module.symvers"
    local symvers_backup=""
    local modpost_warn_param=""

    if contains_version "$version" "${NO_CRC_VERSIONS[@]}"; then
        # Kbuild/modpost 会从输出目录的 Module.symvers 读取符号 CRC 并写入 ko 的 __versions。
        # 临时改名隐藏它，配合 KBUILD_MODPOST_WARN=1，把未解析符号降级为 warning，最终得到空 CRC ko。
        modpost_warn_param="KBUILD_MODPOST_WARN=1 CONFIG_EXTENDED_MODVERSIONS=n"
        if [[ -f "$symvers_file" ]]; then
            symvers_backup="$symvers_file.no_crc_bak.$$"
            log_warn "临时隐藏 $symvers_file，避免 modpost 导入 CRC"
            mv "$symvers_file" "$symvers_backup"
        else
            log_warn "未找到 $symvers_file，modpost 将不会导入 CRC"
        fi
    fi

    # 注意: extra_params 故意不加引号, 依赖 word splitting 拆分多参数
    # shellcheck disable=SC2086
    set +e
    env PATH="$clang_path/bin:$PATH" \
        make -C "$kernel_dir/common" \
            O="$bazel_out" \
            M="$DRIVER_SRC" \
            ARCH=arm64 \
            LLVM=1 \
            LLVM_IAS=1 \
            CROSS_COMPILE="$cross_prefix" \
            $extra_params \
            $modpost_warn_param \
            modules -j"$(nproc)"
    local make_status=$?
    #make 第一次失败后，判断需要“无 CRC”构建。用 fix_empty_ext_modversions() 去修 lsdriver.mod.c 里那个空的 __version_ext_names 初始化问题，在重试最终链接阶段
    if [[ $make_status -ne 0 ]] && contains_version "$version" "${NO_CRC_VERSIONS[@]}" && fix_empty_ext_modversions; then
        # 第一次 make 已经完成 modpost 并留下 lsdriver.mod.c；这里只重试最终编译/链接阶段。
        log_warn "检测到空 __version_ext_names，已修补 lsdriver.mod.c 并重试最终链接"
        env PATH="$clang_path/bin:$PATH" \
            make -C "$kernel_dir/common" \
                O="$bazel_out" \
                M="$DRIVER_SRC" \
                ARCH=arm64 \
                LLVM=1 \
                LLVM_IAS=1 \
                CROSS_COMPILE="$cross_prefix" \
                $extra_params \
                $modpost_warn_param \
                modules -j"$(nproc)"
        make_status=$?
    fi

    set -e

    if [[ -n "$symvers_backup" ]]; then
        mv "$symvers_backup" "$symvers_file"
        log_info "已恢复 $symvers_file"
    fi

    if [[ $make_status -ne 0 ]]; then
        BUILD_RESULTS+=("$version: ❌ 编译失败")
        return "$make_status"
    fi

    handle_output "$version" "$clang_path"
}

# ======================== Legacy 构建 ========================
# 适用于: Android 12 及以下 (5.10-Android12)
build_legacy_kernel() {
    local version="5.10-Android12"
    local kernel_dir="$KERNELS_ROOT/$version"

    log_title
    log_warn "正在开始编译内核版本: $version (Legacy)"

    if [[ ! -d "$kernel_dir" ]]; then
        log_error "错误: 找不到内核目录 $kernel_dir"
        BUILD_RESULTS+=("$version: ❌ 目录不存在")
        return
    fi

    log_warn "正在清理旧的构建产物..."
    clean_driver_build

    cd "$kernel_dir" || return

    local common_out_dir
    common_out_dir="$(pwd)/out/$version/common"
    local kernel_build_dir="$common_out_dir/common"
    local kernel_config="$kernel_build_dir/.config"
    local kernel_image="$kernel_build_dir/arch/arm64/boot/Image"
    local dist_image="$common_out_dir/dist/Image"
    local kernel_src="$kernel_dir/common"

    # ============ 与 build.config.common 完全一致的工具链 ============
    local legacy_clang="$kernel_dir/prebuilts-master/clang/host/linux-x86/clang-r416183b"
    local build_tools="$kernel_dir/build/build-tools/path/linux-x86"
    local FULL_PATH="$legacy_clang/bin:$build_tools:$PATH"

    if [[ ! -x "$legacy_clang/bin/clang" ]]; then
        log_error "❌ 找不到 clang"
        BUILD_RESULTS+=("$version: ❌ clang 不存在")
        return 1
    fi

    log_info "使用 clang: $legacy_clang"

    # --- 编译前检查 ---
    # 外部模块只需要已经配置/准备好的输出目录，不需要每次链接完整 vmlinux。
    # Android12 的 vmlinux LTO 很吃内存，WSL 容易在这里被 OOM kill，所以优先复用已有 .config。
    if [[ -f "$kernel_config" ]]; then
        log_info "✅ 检测到内核配置 (.config)，跳过全量内核链接..."
    elif [[ -f "$kernel_image" ]] || [[ -f "$dist_image" ]]; then
        log_info "✅ 检测到内核产物 (Image)，跳过全量编译..."
    else
        log_warn "🚀 未找到内核配置，执行 build/build.sh 初始化输出目录..."
        BUILD_CONFIG=common/build.config.gki.aarch64 \
            OUT_DIR="$common_out_dir" \
            build/build.sh
        if [[ ! -f "$kernel_config" ]]; then
            log_error "❌ 内核输出目录初始化失败"
            BUILD_RESULTS+=("$version: ❌ 内核编译失败")
            return 1
        fi
    fi


    # --- modules_prepare ---
    log_warn "⚡ 正在准备模块构建环境..."
    env PATH="$FULL_PATH" \
        HOSTCFLAGS="--sysroot=$kernel_dir/build/build-tools/sysroot -I$kernel_dir/prebuilts/kernel-build-tools/linux-x86/include" \
        HOSTLDFLAGS="--sysroot=$kernel_dir/build/build-tools/sysroot -L$kernel_dir/prebuilts/kernel-build-tools/linux-x86/lib64 -fuse-ld=lld --rtlib=compiler-rt" \
    make -C "$kernel_src" \
        O="$kernel_build_dir" \
        ARCH=arm64 \
        LLVM=1 \
        LLVM_IAS=1 \
        CROSS_COMPILE=aarch64-linux-gnu- \
        HOSTCC=clang \
        HOSTCXX=clang++ \
        HOSTLD=ld.lld \
        modules_prepare

    # --- 编译外部模块 ---
    log_info "正在编译外部模块 ($version)..."

    local symvers_file="$kernel_build_dir/Module.symvers"
    local symvers_backup=""
    local modpost_warn_param=""

    if contains_version "$version" "${NO_CRC_VERSIONS[@]}"; then
        # Legacy 构建同样依赖输出目录 Module.symvers 写入 __versions，处理方式与 Bazel 分支一致。
        modpost_warn_param="KBUILD_MODPOST_WARN=1 CONFIG_EXTENDED_MODVERSIONS=n"
        if [[ -f "$symvers_file" ]]; then
            symvers_backup="$symvers_file.no_crc_bak.$$"
            log_warn "临时隐藏 $symvers_file，避免 modpost 导入 CRC"
            mv "$symvers_file" "$symvers_backup"
        else
            log_warn "未找到 $symvers_file，modpost 将不会导入 CRC"
        fi
    fi

    set +e
    env PATH="$FULL_PATH" \
        HOSTCFLAGS="--sysroot=$kernel_dir/build/build-tools/sysroot -I$kernel_dir/prebuilts/kernel-build-tools/linux-x86/include" \
        HOSTLDFLAGS="--sysroot=$kernel_dir/build/build-tools/sysroot -L$kernel_dir/prebuilts/kernel-build-tools/linux-x86/lib64 -fuse-ld=lld --rtlib=compiler-rt" \
    make -C "$kernel_src" \
        O="$kernel_build_dir" \
        M="$DRIVER_SRC" \
        ARCH=arm64 \
        LLVM=1 \
        LLVM_IAS=1 \
        CROSS_COMPILE=aarch64-linux-gnu- \
        HOSTCC=clang \
        HOSTCXX=clang++ \
        HOSTLD=ld.lld \
        $modpost_warn_param \
        modules -j"$(nproc)"
    local make_status=$?

    if [[ $make_status -ne 0 ]] && contains_version "$version" "${NO_CRC_VERSIONS[@]}" && fix_empty_ext_modversions; then
        # 以防万一兼容 Legacy 内核开启扩展 modversions 的情况。
        log_warn "检测到空 __version_ext_names，已修补 lsdriver.mod.c 并重试最终链接"
        env PATH="$FULL_PATH" \
            HOSTCFLAGS="--sysroot=$kernel_dir/build/build-tools/sysroot -I$kernel_dir/prebuilts/kernel-build-tools/linux-x86/include" \
            HOSTLDFLAGS="--sysroot=$kernel_dir/build/build-tools/sysroot -L$kernel_dir/prebuilts/kernel-build-tools/linux-x86/lib64 -fuse-ld=lld --rtlib=compiler-rt" \
        make -C "$kernel_src" \
            O="$kernel_build_dir" \
            M="$DRIVER_SRC" \
            ARCH=arm64 \
            LLVM=1 \
            LLVM_IAS=1 \
            CROSS_COMPILE=aarch64-linux-gnu- \
            HOSTCC=clang \
            HOSTCXX=clang++ \
            HOSTLD=ld.lld \
            $modpost_warn_param \
            modules -j"$(nproc)"
        make_status=$?
    fi

    set -e

    if [[ -n "$symvers_backup" ]]; then
        mv "$symvers_backup" "$symvers_file"
        log_info "已恢复 $symvers_file"
    fi

    if [[ $make_status -ne 0 ]]; then
        BUILD_RESULTS+=("$version: ❌ 编译失败")
        return "$make_status"
    fi


    handle_output "$version" "$legacy_clang"
}





main() {
    local requested_versions=("$@")

    log_warn "是否需要剥离(strip)符号？"
    echo -e "  输入 ${GREEN}'y'${NC} 进行剥离 (减小体积)"
    echo -e "  输入 ${GREEN}'n'${NC} 不剥离 (保留调试符号)"
    read -rp "请输入 (y/n): " STRIP_CHOICE


    if [[ "$STRIP_CHOICE" != "y" && "$STRIP_CHOICE" != "Y" && \
          "$STRIP_CHOICE" != "n" && "$STRIP_CHOICE" != "N" ]]; then
        log_warn "无效输入，默认不剥离"
        STRIP_CHOICE="n"
    fi

    # 导出给子函数使用 (非 export, 同进程内可见)
    readonly STRIP_CHOICE

    if [[ ${#requested_versions[@]} -gt 0 ]]; then
        log_warn "仅编译指定版本: ${requested_versions[*]}"
    fi

    should_build() {
        [[ ${#requested_versions[@]} -eq 0 ]] || contains_version "$1" "${requested_versions[@]}"
    }

    # 内核 6.12-Android16
    should_build "6.12-Android16" && build_kernel "6.12-Android16" \
        "$KERNELS_ROOT/6.12-Android16/prebuilts/clang/host/linux-x86/clang-r536225" \
        "aarch64-linux-gnu-" \
        "CLANG_TRIPLE=aarch64-linux-gnu-"

    # 2. 内核 6.6-Android15
    should_build "6.6-Android15" && build_kernel "6.6-Android15" \
        "$KERNELS_ROOT/6.6-Android15/prebuilts/clang/host/linux-x86/clang-r510928" \
        "aarch64-linux-gnu-" \
        "CLANG_TRIPLE=aarch64-linux-gnu-"

    # 3. 内核 6.1-Android14
    should_build "6.1-Android14" && build_kernel "6.1-Android14" \
        "$KERNELS_ROOT/6.1-Android14/prebuilts/clang/host/linux-x86/clang-r487747c" \
        "aarch64-linux-gnu-" \
        "LLVM_TOOLCHAIN_PATH=$KERNELS_ROOT/6.1-Android14/prebuilts/clang/host/linux-x86/clang-r487747c"

    # 4. 内核 5.15-Android13
    should_build "5.15-Android13" && build_kernel "5.15-Android13" \
        "$KERNELS_ROOT/5.15-Android13/prebuilts/clang/host/linux-x86/clang-r450784e" \
        "aarch64-linux-gnu-" \
        "LLVM_TOOLCHAIN_PATH=$KERNELS_ROOT/5.15-Android13/prebuilts/clang/host/linux-x86/clang-r450784e"

    # 5. 5.10-Android13
    should_build "5.10-Android13" && build_kernel "5.10-Android13" \
        "$KERNELS_ROOT/5.10-Android13/prebuilts/clang/host/linux-x86/clang-r450784e" \
        "aarch64-linux-gnu-" \
        "LLVM_TOOLCHAIN_PATH=$KERNELS_ROOT/5.10-Android13/prebuilts/clang/host/linux-x86/clang-r450784e KBUILD_MODPOST_WARN=1"

    # 6. 5.10-Android12 (Legacy)
    should_build "5.10-Android12" && build_legacy_kernel

    log_title
    echo ""
    echo -e "${BLUE}编译结果汇总:${NC}"
    echo -e "${BLUE}----------------------------------------------------${NC}"
    for result in "${BUILD_RESULTS[@]}"; do
        echo -e "  $result"
    done
    echo -e "${BLUE}----------------------------------------------------${NC}"
    echo ""

    clean_driver_build

    package_driver

    echo -e "${BLUE}产物列表:${NC}"
    # shellcheck disable=SC2086
    ls -lh "$DRIVER_SRC"/[0-9]*-Android*.ko 2>/dev/null || \
        log_error "未找到任何 .ko 文件"

    if [[ -f "$BUILD_ROOT/install_driver.sh" ]]; then
        ls -lh "$BUILD_ROOT/install_driver.sh"
    fi

    log_title
}

main "$@"
