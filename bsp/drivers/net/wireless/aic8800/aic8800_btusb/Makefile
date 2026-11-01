obj-$(CONFIG_AIC8800_BTUSB_SUPPORT) := aic8800_btusb.o

SRCPATH := $(shell dirname $(lastword $(MAKEFILE_LIST)))
ccflags-y += -I$(SRCPATH)/../aic8800_bsp
ccflags-y += -Wno-implicit-fallthrough

aic8800_btusb-y := aic_btusb.o

CONFIG_SUPPORT_VENDOR_APCF = n
# Need to set fw path in BOARD_KERNEL_CMDLINE
CONFIG_USE_FW_REQUEST = y
# If you need to use the feature of waking up with BLE during STR Suspend sleep, please enable this macro.
CONFIG_BT_WAKEUP_IN_PM = n
# If CONFIG_BT_WAKEUP_IN_PM = y, you can choose GPIO High or low trigger method, Or GPIO waveform trigger method
CONFIG_WOBLE_WAVEFORM = n

ifeq ($(CONFIG_SUPPORT_VENDOR_APCF), y)
obj-$(CONFIG_AIC8800_BTUSB_SUPPORT) := aic_btusb_external_featrue.o
endif

ccflags-$(CONFIG_SUPPORT_VENDOR_APCF) += -DCONFIG_SUPPORT_VENDOR_APCF
ccflags-$(CONFIG_BT_WAKEUP_IN_PM) += -DCONFIG_BT_WAKEUP_IN_PM
ifeq ($(CONFIG_BT_WAKEUP_IN_PM), y)
ccflags-$(CONFIG_WOBLE_WAVEFORM) += -DCONFIG_WOBLE_WAVEFORM
endif

ccflags-$(CONFIG_USE_FW_REQUEST) += -DCONFIG_USE_FW_REQUEST

CONFIG_PLATFORM_ALLWINNER ?= y

ccflags-$(CONFIG_PLATFORM_ALLWINNER) += -DCONFIG_PLATFORM_ALLWINNER
