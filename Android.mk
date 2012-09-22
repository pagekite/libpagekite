TOP_LOCAL_PATH := $(call my-dir)

include $(TOP_LOCAL_PATH)/libev/Android.mk

NDK_PROJECT_PATH := $(TOP_LOCAL_PATH)/openssl-android/
# We skip the openssl apps, don't need them.
include $(TOP_LOCAL_PATH)/openssl-android/ssl/Android.mk
include $(TOP_LOCAL_PATH)/openssl-android/crypto/Android.mk

include $(CLEAR_VARS)
NDK_PROJECT_PATH    := $(TOP_LOCAL_PATH)
LOCAL_PATH          := $(TOP_LOCAL_PATH)
LOCAL_C_INCLUDES    := $(LOCAL_PATH)/ $(LOCAL_PATH)/libev/ $(LOCAL_PATH)/openssl-android/ $(LOCAL_PATH)/openssl-android/include/
LOCAL_MODULE        := pagekite
LOCAL_SRC_FILES     := utils.c pd_sha1.c pkproto.c pkstate.c pklogging.c pkerror.c \
                       pkconn.c pkmanager.c pkblocker.c
LOCAL_LDLIBS        := -lc -llog
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_PATH             := $(TOP_LOCAL_PATH)
LOCAL_C_INCLUDES       := $(LOCAL_PATH)/ $(LOCAL_PATH)/libev/ $(LOCAL_PATH)/openssl-android/ $(LOCAL_PATH)/openssl-android/include/
LOCAL_MODULE           := pagekite-jni
LOCAL_SRC_FILES        := pagekite-jni.c
LOCAL_STATIC_LIBRARIES := pagekite libev
LOCAL_SHARED_LIBRARIES := libcrypto libssl
LOCAL_LDLIBS           := -lc -llog
include $(BUILD_SHARED_LIBRARY)
