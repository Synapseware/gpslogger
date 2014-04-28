gpslogger
=========
The GPS Logger is an Arduino clone board that uses the Adafruit GPS breakout board and a micro-SD socket.  The intention is to create a low power GPS logger that can last a couple of weeks between recharges and that can record data nearly indefinitely.

The initial implementation has a few problems.  Namely:
* The Arduino's TX & RX pins are backwards, requireing the use of the serial-header board to correct the pinouts.
* Not all the Adafruit GPS breakout board pins are brought out to the Arduino
* No on-board reset button.

However...
I have created this board as a project for a friend.  You are welcome to copy the code, design, etc, as long as you retain the original author's copyright and license information.

### Folders
* /board
  * Eagle files
* /media
  * Pictures of the board through development to final packaging
* /sketchbookfolder/gpslogger
  * Project source
* /sketchbookfolder/libraries
  * 3rd party Arduino libraries
