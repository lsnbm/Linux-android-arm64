#!/bin/bash

# 1. 配置：你的内核源码存放在哪个目录下 (比如 /root)
KERNELS_ROOT="/root"

# 2. 配置：驱动源码存放路径
DRIVER_SRC="/mnt/e/1.CodeRepository/Android/Kernel/lsdriver"

# 颜色定义
GREEN='\e[32m'
RED='\e[31m'
YELLOW='\e[33m'
BLUE='\e[34m'
NC='\e[0m'

# --- 新增：在程序开始时询问是否剥离符号 ---
echo -e "${YELLOW}是否需要剥离(strip)符号？${NC}"
echo -e "输入 ${GREEN}'y'${NC} 进行剥离 ()"
echo -e "输入 ${GREEN}'n'${NC} 不剥离 (保留调试符号，创建一个副本)"
read -p "请输入 (y/n): " STRIP_CHOICE

build_kernel() {
    local VERSION=$1
    local CLANG_PATH=$2
    local CROSS_PREFIX=$3
    local EXTRA_PARAMS=$4

    # 定义当前版本的完整路径
    local KERNEL_DIR="$KERNELS_ROOT/$VERSION"
    
    echo -e "${BLUE}====================================================${NC}"
    echo -e "${YELLOW}正在开始编译内核版本: ${VERSION}${NC}"
    echo -e "${BLUE}路径: ${KERNEL_DIR}${NC}"
    echo -e "${BLUE}====================================================${NC}"

    # 检查内核目录是否存在
    if [ ! -d "$KERNEL_DIR" ]; then
        echo -e "${RED}错误: 找不到内核目录 $KERNEL_DIR${NC}"
        return
    fi

    # --- 环境准备步骤 ---
    cd "$KERNEL_DIR" || return
    local BAZEL_OUT=$(readlink -f bazel-bin/common/kernel_aarch64)

    # 如果输出目录不在，则执行 Bazel 编译
    if [ ! -d "$BAZEL_OUT" ]; then
        echo -e "${YELLOW}检测到 $VERSION 未进行内核编译，正在执行   tools/bazel build //common:kernel_aarch64 //common:kernel_aarch64_modules_prepare...${NC}"
        tools/bazel  build //common:kernel_aarch64 //common:kernel_aarch64_modules_prepare
        
        # 编译后重新获取路径
        BAZEL_OUT=$(readlink -f bazel-bin/common/kernel_aarch64)
        if [ ! -d "$BAZEL_OUT" ]; then
            echo -e "${RED}错误: $VERSION 的内核编译失败，无法继续编译驱动。${NC}"
            return
        fi
    fi

    echo -e "${YELLOW}正在准备编译环境 (解压 modules_prepare)...${NC}"
    # 进入内核输出目录并解压环境包
    cd "$BAZEL_OUT" || return
    if [ -f "../kernel_aarch64_modules_prepare/modules_prepare_outdir.tar.gz" ]; then
        tar -xzf ../kernel_aarch64_modules_prepare/modules_prepare_outdir.tar.gz
    else
        echo -e "${RED}警告: 未找到 modules_prepare_outdir.tar.gz，跳过解压${NC}"
    fi

    # 回到内核根目录准备执行 make
    cd "$KERNEL_DIR" || return

    # --- 编译步骤 ---
    # 设置编译器环境变量
    export PATH="$CLANG_PATH/bin:$PATH"

    echo -e "${GREEN}执行 Make 编译...${NC}"
    make -C "$KERNEL_DIR/common" \
        O="$BAZEL_OUT" \
        M="$DRIVER_SRC" \
        ARCH=arm64 \
        LLVM=1 \
        LLVM_IAS=1 \
        CROSS_COMPILE="$CROSS_PREFIX" \
        $EXTRA_PARAMS \
        modules -j$(nproc)

    if [ $? -eq 0 ]; then
        # --- 根据选择执行剥离或直接复制 ---
        if [[ "$STRIP_CHOICE" == "y" || "$STRIP_CHOICE" == "Y" ]]; then
            echo -e "${GREEN}编译成功! 正在剥离符号...${NC}"
            "$CLANG_PATH/bin/llvm-strip" --strip-debug \
                -o "$DRIVER_SRC/${VERSION}lsdriver.ko" \
                "$DRIVER_SRC/lsdriver.ko"
            echo -e "${GREEN}剥离完成: ${VERSION}lsdriver.ko${NC}"
        else
            echo -e "${GREEN}编译成功! 不剥离符号，正在创建副本...${NC}"
            cp "$DRIVER_SRC/lsdriver.ko" "$DRIVER_SRC/${VERSION}lsdriver.ko"
            echo -e "${GREEN}副本创建完成: ${VERSION}lsdriver.ko (含符号)${NC}"
        fi
    else
        echo -e "${RED}编译 $VERSION 失败!${NC}"
    fi
}

# --- 执行流程 ---
#修改对应内核源码目录名称

#修改对应内核源码目录名称, 6.6和6.12剥离符号无法加载驱动

build_kernel "6.12" \
    "$KERNELS_ROOT/6.12/prebuilts/clang/host/linux-x86/clang-r536225" \
    "aarch64-linux-gnu-" \
    "CLANG_TRIPLE=aarch64-linux-gnu-"


# 1. 编译 6.6
build_kernel "6.6" \
    "$KERNELS_ROOT/6.6/prebuilts/clang/host/linux-x86/clang-r510928" \
    "aarch64-linux-gnu-" \
    "CLANG_TRIPLE=aarch64-linux-gnu-"

# 2. 编译 6.1
build_kernel "6.1" \
    "$KERNELS_ROOT/6.1/prebuilts/clang/host/linux-x86/clang-r487747c" \
    "$KERNELS_ROOT/6.1/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android-" \
    "LLVM_TOOLCHAIN_PATH=$KERNELS_ROOT/6.1/prebuilts/clang/host/linux-x86/clang-r487747c"

# 3. 编译 5.15
build_kernel "5.15" \
    "$KERNELS_ROOT/5.15/prebuilts/clang/host/linux-x86/clang-r450784e" \
    "$KERNELS_ROOT/5.15/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android-" \
    "LLVM_TOOLCHAIN_PATH=$KERNELS_ROOT/5.15/prebuilts/clang/host/linux-x86/clang-r450784e"

# 4. 编译 5.10
build_kernel "android13-5.10" \
    "$KERNELS_ROOT/android13-5.10/prebuilts/clang/host/linux-x86/clang-r450784e" \
    "$KERNELS_ROOT/android13-5.10/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android-" \
    "LLVM_TOOLCHAIN_PATH=$KERNELS_ROOT/android13-5.10/prebuilts/clang/host/linux-x86/clang-r450784e KBUILD_MODPOST_WARN=1"

echo -e "${BLUE}全部任务处理完毕!${NC}"
ls -lh $DRIVER_SRC/*lsdriver.ko