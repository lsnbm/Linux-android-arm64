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

# 保留浮点比较与 NaN 语义，不启用 fast-math。
LOCAL_CPPFLAGS := -w -std=c++26 -DVK_USE_PLATFORM_ANDROID_KHR -O3 -fexceptions
LOCAL_ASFLAGS += -I$(LOCAL_PATH)

# 默认启用调试日志；LS_DEBUG_LOG=0 会移除 LS_LOGD 及其格式字符串。
LS_DEBUG_LOG ?= 1
LOCAL_CFLAGS += -DLS_DEBUG_LOG=$(LS_DEBUG_LOG)
LOCAL_CPPFLAGS += -include $(LOCAL_PATH)/logger/logger.h

# 头文件路径
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/driver
LOCAL_C_INCLUDES += $(LOCAL_PATH)/font
LOCAL_C_INCLUDES += $(LOCAL_PATH)/android_draw
LOCAL_C_INCLUDES += $(LOCAL_PATH)/android_imgui_input
LOCAL_C_INCLUDES += $(LOCAL_PATH)/imgui
LOCAL_C_INCLUDES += $(LOCAL_PATH)/imgui/backends
LOCAL_C_INCLUDES += $(LOCAL_PATH)/json/single_include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/thread-pool/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/android_surface

# 主程序源文件
FILE_LIST := $(wildcard $(LOCAL_PATH)/src/main.cpp)
FILE_LIST += $(wildcard $(LOCAL_PATH)/src/cloudflared_blob.S)
FILE_LIST += $(wildcard $(LOCAL_PATH)/imgui/*.cpp)
FILE_LIST += $(LOCAL_PATH)/imgui/backends/imgui_impl_android.cpp
FILE_LIST += $(LOCAL_PATH)/imgui/backends/imgui_impl_vulkan.cpp
LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)

# 链接静态库
LOCAL_STATIC_LIBRARIES := libcapstone
LOCAL_LDLIBS := -lvulkan -landroid -llog

include $(BUILD_EXECUTABLE)

# ------------------------------------------------------------------
# Build finished, push the generated binary to the connected device.
# The default ndk-build target is `all`, so we extend it here instead
# of changing your build command.
AKERNEL_PUSH_DIR := /data/akernel
AKERNEL_TEMP_FILE := /data/local/tmp/$(LOCAL_MODULE_FILENAME)
AKERNEL_BINARY := $(NDK_APP_LIBS_OUT)/$(TARGET_ARCH_ABI)/$(LOCAL_MODULE_FILENAME)
AKERNEL_REMOTE_FILE := $(AKERNEL_PUSH_DIR)/$(LOCAL_MODULE_FILENAME)
AKERNEL_REMOTE_NEW_FILE := $(AKERNEL_REMOTE_FILE).new
.PHONY: push-akernel
push-akernel: $(AKERNEL_BINARY)
	@adb push "$(call host-path,$(AKERNEL_BINARY))" "$(AKERNEL_TEMP_FILE)"
	@adb shell "echo 'mkdir -p $(AKERNEL_PUSH_DIR) && cp $(AKERNEL_TEMP_FILE) $(AKERNEL_REMOTE_NEW_FILE) && chmod 755 $(AKERNEL_REMOTE_NEW_FILE) && mv -f $(AKERNEL_REMOTE_NEW_FILE) $(AKERNEL_REMOTE_FILE)' | su"

all: push-akernel
