#
#             LUFA Library
#     Copyright (C) Dean Camera, 2014.
#
#  dean [at] fourwalledcubicle [dot] com
#           www.lufa-lib.org
#
# --------------------------------------
#         LUFA Project Makefile.
# --------------------------------------

# Run "make help" for target help.
FUSE_1			= 0x00
FUSE_2			= 0xDF
FUSE_4			= 0xFF
FUSE_5			= 0xFF


MCU			= atxmega128a4u
ARCH			= XMEGA
BOARD			= USER
F_CPU			= 32000000
F_USB			= 48000000
OPTIMIZATION		= s
TARGET			= gpslogger
SRC				= $(TARGET).c \
				Descriptors.c \
				Drivers/AdafruitGps.c \
				Drivers/TC_driver.c \
				Drivers/at25df321.c \
				$(LUFA_SRC_USB) \
				$(LUFA_SRC_USBCLASS) \
				$(LUFA_SRC_SERIAL) \
				$(LUFA_SRC_SPI)
LUFA_PATH		= ../LUFA
CC_FLAGS		= -DUSE_LUFA_CONFIG_HEADER -IConfig/
LD_FLAGS		= 

# Default target
all:

# Include LUFA build script makefiles
include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_cppcheck.mk
include $(LUFA_PATH)/Build/lufa_doxygen.mk
include $(LUFA_PATH)/Build/lufa_dfu.mk
include $(LUFA_PATH)/Build/lufa_hid.mk
include $(LUFA_PATH)/Build/lufa_avrdude.mk
include $(LUFA_PATH)/Build/lufa_atprogram.mk


program: all
	avrdude -p $(MCU) -P usb -c avrisp2 -U flash:w:$(TARGET).hex

