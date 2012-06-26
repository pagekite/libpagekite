LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libev


LOCAL_CFLAGS := -DNDEBUG \
				-DHAVE_CONFIG_H \
				-DEV_CONFIG_H="\"ev_config_android.h\""

LOCAL_SRC_FILES := \
	ev.c \
	event.c


LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.
	
include $(BUILD_STATIC_LIBRARY)
