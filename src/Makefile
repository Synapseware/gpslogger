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

MCU				= atxmega128a4u
ARCH			= XMEGA
BOARD			= USER
F_CPU			= 32000000
F_USB			= 48000000
OPTIMIZATION	= s
TARGET			= gpslogger
SRC				= $(TARGET).c \
				Descriptors.c \
				NMEAParser.c \
				Board/LCD.c \
				Drivers/AdafruitGps.c \
				Drivers/TC_driver.c \
				Drivers/at25df321.c \
				Drivers/hd44780_twi.c \
				$(LUFA_SRC_USB) \
				$(LUFA_SRC_USBCLASS) \
				$(LUFA_SRC_SERIAL) \
				$(LUFA_SRC_SPI) \
				$(LUFA_SRC_TWI)
LUFA_PATH		= ../LUFA
CC_FLAGS		= -DUSE_LUFA_CONFIG_HEADER -IConfig/
LD_FLAGS		= -Wl,-u,vfprintf -lprintf_flt

# Default target
all:

# Include LUFA-specific DMBS extension modules
DMBS_LUFA_PATH ?= $(LUFA_PATH)/Build/LUFA
include $(DMBS_LUFA_PATH)/lufa-sources.mk
include $(DMBS_LUFA_PATH)/lufa-gcc.mk

# Include common DMBS build system modules
DMBS_PATH      ?= $(LUFA_PATH)/Build/DMBS/DMBS
include $(DMBS_PATH)/core.mk
include $(DMBS_PATH)/cppcheck.mk
include $(DMBS_PATH)/doxygen.mk
include $(DMBS_PATH)/dfu.mk
include $(DMBS_PATH)/gcc.mk
include $(DMBS_PATH)/hid.mk
include $(DMBS_PATH)/avrdude.mk
include $(DMBS_PATH)/atprogram.mk


program: all
	avrdude -p $(MCU) -P usb -c avrisp2 -U flash:w:$(TARGET).hex

