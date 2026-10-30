#!/usr/bin/env bash
set -euo pipefail

# Build lsdriver against the prepared kernel trees supplied by the Android DDK.
#
# The DDK calls its targets android16-6.12, while the public installer and
# build_all.sh use 6.12-Android16. This script accepts both spellings and
# always writes canonical 6.12-Android16.ko style names.

BUILD_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CLANG_ROOT="${CLANG_ROOT:-/opt/ddk/clang}"
KERNELS_ROOT="${KERNELS_ROOT:-/opt/ddk/kdir}"
DRIVER_SRC="${DRIVER_SRC:-$BUILD_ROOT/lsdriver}"
JOBS="${JOBS:-$(nproc)}"

if [[ ! "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
    printf 'JOBS must be a positive integer (got %s)\n' "$JOBS" >&2
    exit 2
fi

GREEN='\e[32m'
RED='\e[31m'
YELLOW='\e[33m'
BLUE='\e[34m'
NC='\e[0m'

declare -a BUILD_RESULTS=()
declare -a REQUESTED_VERSIONS=()
REQUESTED_VERSIONS_COUNT=0
STRIP_CHOICE="${STRIP_CHOICE:-n}"

# Canonical names are the names consumed by packer.sh and install_driver.sh.
ALL_VERSIONS=(
    '6.12-Android16'
    '6.6-Android15'
    '6.1-Android14'
    '5.15-Android13'
    '5.10-Android13'
    '5.10-Android12'
)

declare -A KMI_FOR_VERSION=(
    ['6.12-Android16']='android16-6.12'
    ['6.6-Android15']='android15-6.6'
    ['6.1-Android14']='android14-6.1'
    ['5.15-Android13']='android13-5.15'
    ['5.10-Android13']='android13-5.10'
    ['5.10-Android12']='android12-5.10'
)

declare -A CLANG_FOR_VERSION=(
    ['6.12-Android16']='clang-r536225'
    ['6.6-Android15']='clang-r510928'
    ['6.1-Android14']='clang-r487747c'
    ['5.15-Android13']='clang-r450784e'
    ['5.10-Android13']='clang-r450784e'
    ['5.10-Android12']='clang-r416183b'
)

# The newest DDK targets must retain their symbol/debug sections. The default
# is already no-strip; this list also protects an explicit STRIP_CHOICE=y.
NO_STRIP_VERSIONS=('6.12-Android16' '6.6-Android15')

log_info()  { echo -e "${GREEN}$*${NC}"; }
log_warn()  { echo -e "${YELLOW}$*${NC}"; }
log_error() { echo -e "${RED}$*${NC}" >&2; }
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

normalize_version() {
    case "$1" in
        6.12-Android16|android16-6.12) echo '6.12-Android16' ;;
        6.6-Android15|android15-6.6)   echo '6.6-Android15' ;;
        6.1-Android14|android14-6.1)   echo '6.1-Android14' ;;
        5.15-Android13|android13-5.15) echo '5.15-Android13' ;;
        5.10-Android13|android13-5.10) echo '5.10-Android13' ;;
        5.10-Android12|android12-5.10) echo '5.10-Android12' ;;
        *)
            log_error "不支持的 DDK 目标: $1"
            return 2
            ;;
    esac
}

resolve_kernel_dir() {
    local version="$1"
    local kmi="${KMI_FOR_VERSION[$version]}"

    # DDK images expose KERNEL_SRC for a single selected target. Do not reuse
    # it for a multi-target invocation unless DDK_TARGET names that same KMI.
    if [[ -n "${KERNEL_SRC:-}" && -d "$KERNEL_SRC" &&
          ( "$REQUESTED_VERSIONS_COUNT" -eq 1 || "${DDK_TARGET:-}" == "$kmi" ) ]]; then
        printf '%s\n' "$KERNEL_SRC"
        return 0
    fi

    if [[ -d "$KERNELS_ROOT/$kmi" ]]; then
        printf '%s\n' "$KERNELS_ROOT/$kmi"
        return 0
    fi
    if [[ -d "$KERNELS_ROOT/$version" ]]; then
        printf '%s\n' "$KERNELS_ROOT/$version"
        return 0
    fi
    return 1
}

resolve_clang_bin() {
    local version="$1"
    local kernel_dir="$2"
    local clang_name="${CLANG_FOR_VERSION[$version]}"
    local candidate

    for candidate in \
        "$CLANG_ROOT/$clang_name/bin" \
        "$kernel_dir/prebuilts/clang/host/linux-x86/$clang_name/bin" \
        "$kernel_dir/prebuilts-master/clang/host/linux-x86/$clang_name/bin"; do
        if [[ -x "$candidate/clang" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    # DDK images normally put the matching clang on PATH. An empty result is
    # intentional; make will then use the image's hermetic toolchain.
    printf '%s\n' ''
}

clean_driver_build() {
    if [[ ! -d "$DRIVER_SRC" ]]; then
        log_error "driver source directory not found: $DRIVER_SRC"
        return 1
    fi

    find "$DRIVER_SRC" -type f \( \
        -name '*.o' -o \
        -name '*.o.d' -o \
        -name '*.mod' -o \
        -name '*.mod.c' -o \
        -name '*.order' -o \
        -name '*.symvers' -o \
        -name '*.cmd' -o \
        -name '*.usyms' \
    \) -delete
    find "$DRIVER_SRC" -type d -name '.tmp_versions' -prune -exec rm -rf -- {} +
}

cleanup_driver_build_on_exit() {
    local status=$?
    trap - EXIT
    clean_driver_build || true
    exit "$status"
}

fix_empty_ext_modversions() {
    local mod_c="$DRIVER_SRC/lsdriver.mod.c"
    [[ -f "$mod_c" ]] || return 1

    # CONFIG_EXTENDED_MODVERSIONS=n on some 6.12 trees leaves an empty
    # __version_ext_names initializer. Turn it into a valid empty string and
    # retry only the final module link, matching build_all.sh.
    if grep -q '__section("__version_ext_names")' "$mod_c" && \
       grep -q '^[[:space:]]*;[[:space:]]*$' "$mod_c"; then
        perl -0pi -e 's/(__used __section\("__version_ext_names"\)\s*=\s*)\n\s*;/$1"";/s' "$mod_c"
        return 0
    fi
    return 1
}

verify_module() {
    local ko="$1"
    local readelf_cmd=""
    if command -v llvm-readelf >/dev/null 2>&1; then
        readelf_cmd="$(command -v llvm-readelf)"
    elif command -v readelf >/dev/null 2>&1; then
        readelf_cmd="$(command -v readelf)"
    fi

    [[ -s "$ko" ]] || { log_error "模块为空: $ko"; return 1; }
    if [[ -n "$readelf_cmd" ]]; then
        "$readelf_cmd" -h "$ko" | grep -Eqi 'Machine:.*AArch64|AArch64.*relocatable' || {
            log_error "模块不是 AArch64 ELF: $ko"
            return 1
        }
    fi
}

find_strip_cmd() {
    local clang_bin="${1:-}"
    if [[ -n "$clang_bin" && -x "$clang_bin/llvm-strip" ]]; then
        printf '%s\n' "$clang_bin/llvm-strip"
    elif command -v llvm-strip >/dev/null 2>&1; then
        command -v llvm-strip
    else
        printf '%s\n' ''
    fi
}

handle_output() {
    local version="$1"
    local clang_bin="${2:-}"
    local source_ko="$DRIVER_SRC/lsdriver.ko"
    local target_ko="$DRIVER_SRC/${version}.ko"
    local strip_cmd

    [[ -f "$source_ko" ]] || {
        log_error "$version 未生成 lsdriver.ko"
        BUILD_RESULTS+=("$version: FAIL (no .ko)")
        return 1
    }

    if contains_version "$version" "${NO_STRIP_VERSIONS[@]}"; then
        log_warn "$version 保留完整符号和调试段"
        cp -f "$source_ko" "$target_ko"
    elif [[ "$STRIP_CHOICE" =~ ^[yY]$ ]]; then
        strip_cmd="$(find_strip_cmd "$clang_bin")"
        if [[ -n "$strip_cmd" ]]; then
            "$strip_cmd" --strip-debug -o "$target_ko" "$source_ko"
        else
            log_warn "找不到 llvm-strip，保留未剥离模块"
            cp -f "$source_ko" "$target_ko"
        fi
    else
        cp -f "$source_ko" "$target_ko"
    fi

    verify_module "$target_ko"
    BUILD_RESULTS+=("$version: OK")
    log_info "生成: $target_ko"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$target_ko"
    fi
}

build_kernel() {
    local version="$1"
    local kernel_dir="$2"
    local clang_bin="$3"
    local symvers_file="$kernel_dir/Module.symvers"
    local symvers_backup=""
    local make_status=0
    local env_path="$PATH"

    log_title
    log_warn "开始 DDK 编译: $version"
    log_info "KDIR=$kernel_dir"
    [[ -n "$clang_bin" ]] && env_path="$clang_bin:$env_path"

    [[ -f "$kernel_dir/.config" ]] || {
        log_error "$version 缺少已准备好的 .config: $kernel_dir"
        BUILD_RESULTS+=("$version: FAIL (KDIR not prepared)")
        return 1
    }

    clean_driver_build
    rm -f -- "$DRIVER_SRC/lsdriver.ko" "$DRIVER_SRC/${version}.ko"

    # Match the official script's no-CRC module contract. Keep the DDK's
    # CFI/LTO/BTI/PAC configuration untouched; only module BTF is disabled.
    if [[ -f "$symvers_file" ]]; then
        symvers_backup="$symvers_file.no_crc_bak.$$"
        mv -- "$symvers_file" "$symvers_backup"
    fi

    local -a make_args=(
        -C "$kernel_dir"
        M="$DRIVER_SRC"
        ARCH=arm64
        CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
        LLVM=1
        LLVM_IAS=1
        CONFIG_DEBUG_INFO_BTF=n
        CONFIG_DEBUG_INFO_BTF_MODULES=
        CONFIG_EXTENDED_MODVERSIONS=n
        KBUILD_MODPOST_WARN=1
        modules
        "-j$JOBS"
    )

    set +e
    env PATH="$env_path" make "${make_args[@]}"
    make_status=$?
    if [[ "$make_status" -ne 0 ]] && fix_empty_ext_modversions; then
        log_warn "$version 修复空 __version_ext_names 后重试链接"
        env PATH="$env_path" make "${make_args[@]}"
        make_status=$?
    fi
    set -e

    if [[ -n "$symvers_backup" ]]; then
        mv -- "$symvers_backup" "$symvers_file"
    fi

    if [[ "$make_status" -ne 0 ]]; then
        BUILD_RESULTS+=("$version: FAIL (make=$make_status)")
        clean_driver_build
        return "$make_status"
    fi

    handle_output "$version" "$clang_bin"
    clean_driver_build
}

usage() {
    cat <<'EOF'
用法: bash ddk_build_all.sh [all|版本...]

版本可使用官方名称（6.12-Android16）或 DDK KMI 名称（android16-6.12）。
环境变量: KERNELS_ROOT、KERNEL_SRC、CLANG_ROOT、DRIVER_SRC、JOBS、STRIP_CHOICE。
DDK 镜像中通常只选择一个目标，并由 DDK_TARGET/KERNEL_SRC 提供对应内核目录。
EOF
}

main() {
    local raw version
    local -a input=("$@")

    if [[ "${input[0]:-}" == '-h' || "${input[0]:-}" == '--help' ]]; then
        usage
        return 0
    fi
    if [[ ${#input[@]} -eq 0 || "${input[0]:-}" == 'all' ]]; then
        REQUESTED_VERSIONS=("${ALL_VERSIONS[@]}")
    else
        for raw in "${input[@]}"; do
            version="$(normalize_version "$raw")" || return
            REQUESTED_VERSIONS+=("$version")
        done
    fi
    REQUESTED_VERSIONS_COUNT=${#REQUESTED_VERSIONS[@]}

    if [[ ! "$STRIP_CHOICE" =~ ^[yYnN]$ ]]; then
        STRIP_CHOICE=n
    fi
    if [[ -t 0 && -z "${STRIP_CHOICE_FROM_ENV:-}" ]]; then
        read -r -p '是否剥离调试符号 (y/n，默认 n): ' raw || true
        [[ "$raw" =~ ^[yY]$ ]] && STRIP_CHOICE=y || STRIP_CHOICE=n
    fi
    readonly STRIP_CHOICE

    trap cleanup_driver_build_on_exit EXIT
    local failures=0
    for version in "${REQUESTED_VERSIONS[@]}"; do
        local kernel_dir clang_bin
        if ! kernel_dir="$(resolve_kernel_dir "$version")"; then
            log_error "找不到 $version 对应的 DDK KDIR（尝试 ${KMI_FOR_VERSION[$version]} 和 $version）"
            BUILD_RESULTS+=("$version: FAIL (KDIR missing)")
            failures=$((failures + 1))
            continue
        fi
        clang_bin="$(resolve_clang_bin "$version" "$kernel_dir")"
        if ! build_kernel "$version" "$kernel_dir" "$clang_bin"; then
            failures=$((failures + 1))
        fi
    done

    log_title
    printf '%s\n' 'DDK 编译结果:'
    printf '  %s\n' "${BUILD_RESULTS[@]}"
    log_title
    if (( failures > 0 )); then
        return 1
    fi
    log_info 'DDK 模块编译完成。请用对应版本的 .ko 做 SHA256 和设备 dmesg/帧率 A/B。'
}

main "$@"
