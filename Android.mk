TOP_LOCAL_PATH := $(call my-dir)

include $(TOP_LOCAL_PATH)/libev/Android.mk
LOCAL_PATH := $(TOP_LOCAL_PATH)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/. $(LOCAL_PATH)/libev/.
LOCAL_MODULE     := pagekite
LOCAL_SRC_FILES  := pkerror.c pkproto.c pkmanager.c pklogging.c utils.c sha1.c
LOCAL_EXPORT_LDLIBS += -lc
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES       := $(LOCAL_PATH)/. $(LOCAL_PATH)/libev/.
LOCAL_MODULE           := pagekite-jni
LOCAL_SRC_FILES        := pagekite-jni.c
LOCAL_STATIC_LIBRARIES := pagekite libev
LOCAL_EXPORT_LDLIBS += -lc

include $(BUILD_SHARED_LIBRARY)
