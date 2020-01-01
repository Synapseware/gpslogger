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
FUSE_1			= 0x00
FUSE_2			= 0xDF
FUSE_4			= 0xFF
FUSE_5			= 0xFF
OPTIMIZATION	= s
TARGET			= gpslogger
SRC				= $(TARGET).cpp \
				Drivers/hal.cpp \
				Drivers/flash.cpp \
				Drivers/AdafruitGps.cpp \
				Drivers/at25df321.cpp \
				Drivers/Descriptors.c \
				Drivers/TC_driver.c \
				Drivers/LufaUsb.c \
				$(LUFA_SRC_USB) \
				$(LUFA_SRC_USBCLASS) \
				$(LUFA_SRC_SERIAL) \
				$(LUFA_SRC_SPI) \
				Arduino/Print.cpp
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

AVRDUDE = avrdude
AVRDUDE_FLAGS = -p $(MCU) -P usb -c avrisp2 -b 115200

program: all
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(TARGET).hex

rfuses:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U fuse1:r:/tmp/fuse1.txt:h -U fuse2:r:/tmp/fuse2.txt:h -U fuse4:r:/tmp/fuse4.txt:h -U fuse5:r:/tmp/fuse5.txt:h
	@for idx in 1 2 4 5; do \
		file=/tmp/fuse$$idx.txt; \
		cmd=`cat $$file`; \
		echo "Fuse $$idx: $$cmd"; \
	done

wfuses:
	#$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:w:$(L_FUSE):m -U hfuse:w:$(H_FUSE):m -U efuse:w:$(E_FUSE):m
	@echo "wfuses not implemented";
