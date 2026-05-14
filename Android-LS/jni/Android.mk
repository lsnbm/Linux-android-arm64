LOCAL_PATH := $(call my-dir)


#  Capstone 反汇编引擎 (静态库)
include $(CLEAR_VARS)
LOCAL_MODULE := libcapstone
LOCAL_C_INCLUDES := $(LOCAL_PATH)/capstone/include

# 核心源文件
CAPSTONE_CORE_SRC := $(wildcard $(LOCAL_PATH)/capstone/*.c)
# ARM64 架构源文件
CAPSTONE_ARCH_SRC := $(wildcard $(LOCAL_PATH)/capstone/arch/AArch64/*.c)

LOCAL_SRC_FILES := $(CAPSTONE_CORE_SRC:$(LOCAL_PATH)/%=%) \
                   $(CAPSTONE_ARCH_SRC:$(LOCAL_PATH)/%=%)

# 使用 CAPSTONE_HAS_AARCH64
LOCAL_CFLAGS := -O3 -w -std=c99 \
                -DCAPSTONE_HAS_AARCH64 \
                -DCAPSTONE_USE_SYS_DYN_MEM

include $(BUILD_STATIC_LIBRARY)


# 主程序
include $(CLEAR_VARS)
LOCAL_MODULE := LS_KTool



# 前端编译选项 (LOCAL_CPPFLAGS)
LOCAL_CPPFLAGS := -w                                 # [关闭警告] 屏蔽“代码膨胀/精度丢失”等优化警告
LOCAL_CPPFLAGS += -std=c++26                         # [最新语法] C++26标准
LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR      # [宏] Vulkan平台宏


LOCAL_CPPFLAGS += -O3                                # [优化] 保持O3，避免Ofast隐含fast-math破坏精度/NaN语义
# LOCAL_CPPFLAGS += -march=armv8-a+simd                # [架构特化] 明确64位ARM，强制开启NEON向量指令
# LOCAL_CPPFLAGS += -ffp-contract=fast                 # [FMA融合] 强行把 a*b+c 缝合为一条时钟周期的硬件指令


# LOCAL_CPPFLAGS += -flto                              # [全量LTO] 全局极限优化（编译会变慢，但执行极快）
# LOCAL_CPPFLAGS += -falign-functions=64               # [函数对齐] 填入NOP指令，强制对齐CPU 64字节缓存行
# LOCAL_CPPFLAGS += -falign-loops=64                   # [循环对齐] 强制对齐循环体，CPU抓取指令零停顿
# LOCAL_CPPFLAGS += -funroll-loops                     # [循环展开] 无限复制粘贴循环体，消灭跳转与分支预测失败
# LOCAL_CPPFLAGS += -mllvm -inline-threshold=10000     # [无限内联] 拉爆阈值，强迫把函数完整复制嵌入到外部循环里
# LOCAL_CPPFLAGS += -mllvm -unroll-threshold=10000     # [无限展开] 拉爆阈值，强迫暴力展开巨大的for循环


 LOCAL_CPPFLAGS += -fexceptions                       # 开启C++异常
# #LOCAL_CPPFLAGS += -fno-exceptions                    # [禁用异常机制] 关掉try-catch，彻底拔掉拖慢速度的栈展开代码
# LOCAL_CPPFLAGS += -fno-rtti                          # [禁用运行时类型] 关掉dynamic_cast等虚函数校验判断
# LOCAL_CPPFLAGS += -fomit-frame-pointer               # [废弃帧指针] 剥夺FP寄存器，拿来做NEON通用计算，白捡性能
# LOCAL_CPPFLAGS += -fstrict-aliasing                  # [严格别名] 允许寄存器死磕缓存，不反复去读内存
# LOCAL_CPPFLAGS += -fno-semantic-interposition        # [内部直连] 拒绝PLT表跳转(防Hook)，绝对地址写死，纯直线运行



# # 链接器后端缝合选项 (LOCAL_LDFLAGS)

# LOCAL_LDFLAGS += -fexceptions                        # 链接阶段保留异常处理
# LOCAL_LDFLAGS += -static-libstdc++                   # [静态链接] C++基础库打入包内，防残缺机型闪退
# LOCAL_LDFLAGS += -flto                               # [呼应前端] 后端开启Full-LTO，打破模块壁垒
# LOCAL_LDFLAGS += -Wl,--lto-O3                        # [LTO极限优化] 链接器重组代码时，使用O3最高标准
# LOCAL_LDFLAGS += -Wl,-mllvm,-inline-threshold=10000  # [LTO无限内联] 链接期透传参数，强行跨文件内联
# LOCAL_LDFLAGS += -Wl,-mllvm,-unroll-threshold=10000  # [LTO无限展开] 链接期透传参数，强行跨文件展开





# 头文件路径
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/Android_draw
LOCAL_C_INCLUDES += $(LOCAL_PATH)/Android_touch
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ImGui
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ImGui/backends
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ImGui/font
LOCAL_C_INCLUDES += $(LOCAL_PATH)/native_surface

# 主程序源文件
FILE_LIST := $(wildcard $(LOCAL_PATH)/src/main.cpp)
FILE_LIST += $(wildcard $(LOCAL_PATH)/ImGui/*.cpp)
FILE_LIST += $(wildcard $(LOCAL_PATH)/ImGui/backends/*.cpp)
FILE_LIST += $(wildcard $(LOCAL_PATH)/ImGui/font/*.cpp)
FILE_LIST += $(wildcard $(LOCAL_PATH)/native_surface/*.cpp)
LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)



# 链接静态库
LOCAL_STATIC_LIBRARIES := libcapstone
LOCAL_LDLIBS := -lvulkan -landroid -llog -lEGL -lGLESv3


include $(BUILD_EXECUTABLE)


# ------------------------------------------------------------------
# Build finished, push the generated binary to the connected device.
# The default ndk-build target is `all`, so we extend it here instead
# of changing your build command.
AKERNEL_PUSH_DIR := /data/akernel
AKERNEL_TEMP_FILE := /data/local/tmp/$(LOCAL_MODULE_FILENAME)
AKERNEL_BINARY := $(NDK_APP_LIBS_OUT)/$(TARGET_ARCH_ABI)/$(LOCAL_MODULE_FILENAME)
AKERNEL_REMOTE_FILE := $(AKERNEL_PUSH_DIR)/$(LOCAL_MODULE_FILENAME)

.PHONY: push-akernel
push-akernel: $(AKERNEL_BINARY)
	@adb push "$(call host-path,$(AKERNEL_BINARY))" "$(AKERNEL_TEMP_FILE)"
	@adb shell "su -c 'mkdir -p $(AKERNEL_PUSH_DIR) && cp $(AKERNEL_TEMP_FILE) $(AKERNEL_REMOTE_FILE) && chmod 755 $(AKERNEL_REMOTE_FILE)'"

all: push-akernel
