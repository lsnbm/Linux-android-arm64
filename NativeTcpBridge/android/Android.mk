LOCAL_PATH := $(call my-dir)

# 复用 Android-LS 里的 Capstone 源码目录
CAPSTONE_REL_PATH := ../../Android-LS/capstone
CAPSTONE_ROOT := $(LOCAL_PATH)/$(CAPSTONE_REL_PATH)

# Capstone 反汇编引擎 (静态库)
include $(CLEAR_VARS)
LOCAL_MODULE := libcapstone
LOCAL_C_INCLUDES := $(CAPSTONE_ROOT)/include

# 核心源文件
CAPSTONE_CORE_SRC := $(wildcard $(CAPSTONE_ROOT)/*.c)
# ARM64 架构源文件
CAPSTONE_ARCH_SRC := $(wildcard $(CAPSTONE_ROOT)/arch/AArch64/*.c)

LOCAL_SRC_FILES := $(CAPSTONE_CORE_SRC:$(LOCAL_PATH)/%=%) \
                   $(CAPSTONE_ARCH_SRC:$(LOCAL_PATH)/%=%)

# 使用 CAPSTONE_HAS_AARCH64
LOCAL_CFLAGS := -O3 -w -std=c99 \
                -DCAPSTONE_HAS_AARCH64 \
                -DCAPSTONE_USE_SYS_DYN_MEM

include $(BUILD_STATIC_LIBRARY)

# 主程序
include $(CLEAR_VARS)
LOCAL_MODULE := tcp_server
LOCAL_SRC_FILES := src/main.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(CAPSTONE_ROOT)/include
LOCAL_CPPFLAGS := -std=c++2c -Wall -Wextra -Wno-ignored-attributes -Wno-packed-non-pod -fexceptions
LOCAL_STATIC_LIBRARIES := libcapstone
LOCAL_LDLIBS := -llog -lc++
include $(BUILD_EXECUTABLE)

# ------------------------------------------------------------------
# 编译完成后自动推送到设备，不自动运行
AKERNEL_MODULE := tcp_server
AKERNEL_PUSH_DIR := /data/akernel
AKERNEL_REMOTE_FILE := $(AKERNEL_PUSH_DIR)/$(AKERNEL_MODULE)
AKERNEL_TMP_FILE := /data/local/tmp/$(AKERNEL_MODULE)
AKERNEL_BINARY := $(NDK_APP_LIBS_OUT)/$(TARGET_ARCH_ABI)/$(AKERNEL_MODULE)

.PHONY: push-akernel

push-akernel: $(AKERNEL_BINARY)
	@echo [AKERNEL] push $(AKERNEL_BINARY) to $(AKERNEL_TMP_FILE), then move to $(AKERNEL_REMOTE_FILE) as root
	@adb shell "su -c 'killall $(AKERNEL_MODULE) >/dev/null 2>&1 || true; mkdir -p $(AKERNEL_PUSH_DIR)'"
	@adb push "$(call host-path,$(AKERNEL_BINARY))" "$(AKERNEL_TMP_FILE)"
	@adb shell "su -c 'cp $(AKERNEL_TMP_FILE) $(AKERNEL_REMOTE_FILE) && chmod 755 $(AKERNEL_REMOTE_FILE) && rm -f $(AKERNEL_TMP_FILE)'"

all: push-akernel
