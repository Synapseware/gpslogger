gpslogger
=========
The GPS Logger is an meant to be a lightweight, long running GPS location recorder.  Its main goal is to periodically log GPS position data over as long a timespan as possible.  The sampling interval is kept low (usually 5 minutes) to preserve battery life.

# Background
The GPS logger uses an ATXMega A4U device (the atxmega128a4u) because it is 3.3v compliant, fast, efficient and well supported.  The 2 primary onboard devices require 3.3v for correct operation: The Adafruit Ultimate GPS v3 breakout board and an Atmel 4mb flash (an at25df321).


### Inspiration
I have created this board as a project for a friend.  You are welcome to copy the code, design, etc, as long as you retain the original author's copyright and license information.

### Folders
* /board
    - Eagle files
* /data
    - Some sample log data
* /media
    - Pictures of the board through development to final packaging
* /src
    - Project source
* /LUFA
    - LUFA library used for CDC interface

## GPS Data
(Location altered)

    $GPGGA,072450.000,4078.8163,N,10921.9609,W,1,10,1.01,104.0,M,-17.3,M,,*59
    $GPRMC,072450.000,A,4078.8163,N,10921.9609,W,0.14,355.98,170815,,,A*7A
    $GPGGA,072451.000,4078.8163,N,10921.9609,W,1,10,1.01,104.0,M,-17.3,M,,*58
    $GPRMC,072451.000,A,4078.8163,N,10921.9609,W,0.15,355.98,170815,,,A*7A
    $GPGGA,072452.000,4078.8164,N,10921.9608,W,1,10,1.01,104.0,M,-17.3,M,,*5D
    $GPRMC,072452.000,A,4078.8164,N,10921.9608,W,0.21,355.98,170815,,,A*78
    $GPGGA,072453.000,4078.8163,N,10921.9607,W,1,10,1.01,104.0,M,-17.3,M,,*54
    $GPRMC,072453.000,A,4078.8163,N,10921.9607,W,0.32,355.98,170815,,,A*73


## Rules for Saving Log Data

* Wakeup and get a fix every 5 mintues
* Stop trying to get fix data after xx seconds
    - prevent system from getting stuck without satellite data
* Fix data is parsed from GPRMC message
* Each NMEA log sentence is appended to the log file at the start of the last message



## Notes
- To add TWI support, add `$(LUFA_SRC_TWI)` back to the makefile.
