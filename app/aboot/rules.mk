LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LK_TOP_DIR)/platform/msm_shared/include -I$(LK_TOP_DIR)/lib/zlib_inflate

DEFINES += ASSERT_ON_TAMPER=1

MODULES += lib/zlib_inflate

OBJS += \
	$(LOCAL_DIR)/aboot.o \
	$(LOCAL_DIR)/fastboot.o \
	$(LOCAL_DIR)/recovery.o \
	$(LOCAL_DIR)/fotainfo.o

ifeq ($(ENABLE_UNITTEST_FW), 1)
OBJS += \
	$(LOCAL_DIR)/fastboot_test.o
endif

ifeq ($(ENABLE_MDTP_SUPPORT),1)
OBJS += \
	$(LOCAL_DIR)/mdtp.o \
	$(LOCAL_DIR)/mdtp_ui.o \
	$(LOCAL_DIR)/mdtp_fuse.o \
	$(LOCAL_DIR)/mdtp_defs.o
endif
