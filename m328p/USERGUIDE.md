GPS Logger
==========
The GPS logger is meant to quietly track your hike and adventures for you at 5 minute intervals.  It should be firmly affixed to the back of the solar panel and promptly forgotten about.  Oh wait, you'll need to charge it.  This should only be necessary about once a week.  Give it a charge and check the blue status LED against the info below.  If it's reporting any errors, give me a ring.

I made lots of headway on keeping the power needs low.  While sleeping (between logging intervals), the module only consumes about 5mA.  Total power usage jumps to about 35mA while searching for a GPS signal and recording it to the SD card.  This search + log step usually takes 8-15 seconds.


### Instructions
In the package you'll find the GPS logger board module and some alcohol wipes.  The GPS logger needs to mounted with the GPS antenna facing out (it's the 3/4" ceramic looking square with a metal dot in the middle).

1)  Take 1 or more of the alcohol swabs and clean the back of the solar panel.
2)  On the back of module, locate the double sided tape and remove the thin red protective covering from it.  This tape is VERY sticky!
3)  Carefully affix the module to the cleaned plastic on the back of the solar panel.  It can go anywhere, but probably best to have it either in the center or at an edge with the GPS antenna as close to the edge as possible.  I'm sure it'll get a good fix regardless, and I'll leave the positioning of the board up to you depending on your needs.  It also doesn't need to be mounted and can safetly hangout in your pack, somewhere near the top.
4)  Tuck the USB charging wire away.  The twist ties should keep it out of the way/coiled up.
5)  You're good to go!


### Normal Operation
If everything is working OK, this is what you'll see:  A very short heartbeat blink from the blue LED at the top of the board every 10 seconds (see below for more info).  It'll remain "quiet" while sleeping.  Every 5 minutes the GPS module will get woken up, at which point the board is active and trying to get a satallite fix.  The GPS board has a red status LED that will start slowly blinking.  This process should last about 8-15 seconds.  As soon as the module gets data indicating a good fix, it'll log the data to the SD card and shut down the GPS module.  When the data is saved, the blue status LED will give 5 very fast blinks.
As the battery drains, the heartbeat LED will change pattern.  See below for what to do.


### About
#### Blue Status LED
	HEARTBEAT:
		Blinks periodically to convey various battery state.
		* Battery good: 5ms blink every 10 seconds
		* Battery low/needs charging: 5ms blink every 3 seconds
		* Battery critical: 1ms blink every 1 second

	GPS position saved:
		* 5 rapid blinks

	Boot OK:
		* 3 quick blinks

	SD Error:
		* 4 long blinks with a very short interval between them

	Daily reset:
		* 1 short, 1 long and 1 short blink


#### Red Status LED (on the blue Adafruit GPS board)
This indicator ships with the GPS module to indicate satallite fix.  A 1Hz blink rate means the module is search, while a short blink every 15 seconds indicates good satallite fix.  You'll likely never see the "satallite fix" pattern since the GPS Logger shuts down the GPS module as soon as it gets data over the serial port indicating a fix.

#### Red charging LED
When you plug in GPS logger to charge, a steady red LED will illuminate.  It's near the USB port.  It'll shut off when it's done charging (or your battery pack is dead!).


### Troubleshooting
In order to solve most of the error codes, you'll need to reset the battery.  The simpliest way which doesn't require unplugging the battery from the board (which will be hard without a pair of tweezers or small pliers) is to quickly short out the battery.  Don't worry, the battery has overload/short circuit protection and will shut off if it detects a short circuit.  To get the battery to turn on again, just plug in the USB charging cable.  To short out the battery:
  * Remove the top cover
  * Orient the board so the module is face up and you can read "GPS Logger v1.0" at the very top of the circuit board
  * Take a small metal object and momentarily connect the vertical jumper pins located on the very bottom, just to the right of the white battery connector (you might see a small spark)
  * Let the board rest for 15-20 seconds
  * Plug the USB charging cable in.  The charge light should turn on and the board will go through a boot sequence and self check
  * Reattach the cover

  Depending on the error, you might try re-seating the SD card.  It shouldn't need to be removed, but will click into place.  Remove it by genetly depressing it into the socket. You'll hear a click and it'll pop (mostly) out.  The battery will probably block it, but that's fine, just put it back in.
