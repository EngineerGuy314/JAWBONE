# JAWBONE
Just Another WSPR Beacon Of Noisy Electronics

Custom PCB and software for a RP2040 based WSPR Beacon intended for tracking high altitude superpressure balloons (aka Pico Balloons).

This is an evolution of my  [pico-WSPRer](https://github.com/EngineerGuy314/pico-WSPRer) project. It is very similar except JAWBONE uses the MS5351 clock generator chip instead of abusing the RP2040's internal PLL oscillator. Also, this project is only intended to be used with a custom PCB, there is no longer an option to assemble it from a RaspberryPi Pico and external components (it is probably still possible, but not tested or documented). For now please continue to reference the [pico-WSPRer Wiki](https://github.com/EngineerGuy314/pico-WSPRer/wiki/pico%E2%80%90WSPRer-(aka-Cheapest-Tracker-in-the-World%E2%84%A2)) because most of the information there is still relevant to JAWBONE.

I owe great thanks and acknowledgement to Roman Piksaykin (https://github.com/RPiks/) for the original program that was the starting point for pico-WSPRer. Much of his code still lives on inside JAWBONE and I have tried to maintain his copyright information in all relevant files.



*sometimes* both clocks on same phase? hmm, 


random notes:
try 18Mhz for best Klock speed?
Optional_Debug bitmapping (MSB to LSB): .,.,.,.,.,IO Status,raw GPS dump

prelim power notes: (kevins board)
18Mhz @ 5V 
idel: 10mA
w/GPS: 34mA
w/Si5351 no load: 46mA
w/ 70 Ohm    56ma

125Mhz @ 5V
idle 27mA
w/gps 52mA
w/si no load 64mA
w/ 70 Ohm    74mA
