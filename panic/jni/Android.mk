LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := panic_reserve_dump
LOCAL_SRC_FILES := dump_finder.cpp
LOCAL_CPPFLAGS := -std=c++17 -Wall -Wextra -Werror
LOCAL_LDLIBS := -llog
include $(BUILD_EXECUTABLE)
