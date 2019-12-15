#!/bin/bash

# /opt/arduino-1.8.10/hardware/arduino/avr/bootloaders/caterina/Leonardo-prod-firmware-2012-12-10.hex
# $HOME/.arduino15/packages/arduino/hardware/avr/1.8.2/bootloaders/caterina/Caterina-Leonardo.hex"

FW_PATH="/opt/arduino-1.8.10/hardware/arduino/avr/bootloaders/caterina/Caterina-Leonardo.hex"

avrdude -P usb -c usbtiny -B 0.1 -p m32u4 -e -U flash:w:"$FW_PATH"


#avrdude -P /dev/ttyACM1 -c avrispmkii -p m32u4 -B 0.1  -e -U flash:w:
