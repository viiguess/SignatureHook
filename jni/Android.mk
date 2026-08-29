LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := SignatureHook
LOCAL_SRC_FILES := SignatureHook.cpp Hooks.cpp MemUtils.cpp

LOCAL_CPPFLAGS  := -std=c++17
LOCAL_CPPFLAGS += -fvisibility=hidden
LOCAL_CPPFLAGS += -fvisibility-inlines-hidden
LOCAL_CPPFLAGS += -include $(LOCAL_PATH)/pch.h

LOCAL_LDLIBS    := -llog

include $(BUILD_SHARED_LIBRARY)