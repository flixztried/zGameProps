LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE     := zygisk
LOCAL_SRC_FILES  := module.cpp gamesprops.cpp prophide.cpp stealth.cpp
LOCAL_LDLIBS     := -llog -ldl -landroid
LOCAL_CPPFLAGS   := -std=c++17 -fno-exceptions -fno-rtti -Wall
LOCAL_C_INCLUDES := $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)
