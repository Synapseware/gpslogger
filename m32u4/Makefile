

PROGRAMMER=usbtiny
PROGRAMMER=avrispv2
PORT=/dev/ttyACM1
MCU=m32u4
FLASH=.pio/build/leonardo/firmware.hex



all:
	pio run

program: all
	@avrdude -c ${PROGRAMMER} -P ${PORT} -p ${MCU} -B 0.1 -e -U flash:w:.pio/build/leonardo/firmware.hex

