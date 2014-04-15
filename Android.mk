# Copyright 2011, The Android-x86 Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=		\
	libhoudini_hook.cpp

LOCAL_MODULE := libhoudini_hook
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := libdl

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=		\
	houdini_hook.c

LOCAL_MODULE := houdini_hook
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)
