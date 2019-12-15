#!/bin/bash

avrdude -c usbtiny -p m32u4 -B 0.1 -e -U flash:w:.pio/build/leonardo/firmware.hex

