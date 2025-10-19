# JAWBONE
Just Another WSPR Beacon Of Noisy Electronics

Custom PCB and software for a RP2040 based WSPR Beacon intended for tracking high altitude superpressure balloons (aka Pico Balloons).

This is an evolution of my  [pico-WSPRer](https://github.com/EngineerGuy314/pico-WSPRer) project. It is very similar except JAWBONE uses the MS5351 clock generator chip instead of abusing the RP2040's internal PLL oscillator. Also, this project is only intended to be used with a custom PCB, there is no longer an option to assemble it from a RaspberryPi Pico and external components (it is probably still possible, but not tested or documented). For now please continue to reference the [pico-WSPRer Wiki](https://github.com/EngineerGuy314/pico-WSPRer/wiki/pico%E2%80%90WSPRer-(aka-Cheapest-Tracker-in-the-World%E2%84%A2)) because most of the information there is still relevant to JAWBONE.

![jawbone](https://github.com/user-attachments/assets/c03d8802-0c34-4173-b809-a5d2d00af5f1)

fully assemble tracker weights only 2.2 gram

![cropped](https://github.com/user-attachments/assets/37393e77-a89e-4b4b-8a41-1bd8c81d080b)

ready for first test flight! Succesfull launch October 5th, 2025. [Tracking link](https://traquito.github.io/search/spots/dashboard/?band=20m&channel=382&callsign=KC3LBR&dtGte=2025-10-5&slot3MsgDefUserDefined=%7B+%22name%22%3A+%22grid_char7%22%2C+++%22unit%22%3A+%22digit%22%2C+++%22lowValue%22%3A+++0%2C++++%22highValue%22%3A+9%2C++++%22stepSize%22%3A+1+++%7D%2C%0A%7B+%22name%22%3A+%22grid_char8%22%2C+++%22unit%22%3A+%22digit%22%2C++++%22lowValue%22%3A+++0%2C++++%22highValue%22%3A++++9%2C++++%22stepSize%22%3A++1+%7D%2C%0A%7B+%22name%22%3A+%22grid_char9%22%2C+++%22unit%22%3A+%22alpha%22%2C++++++%22lowValue%22%3A+++0%2C++++%22highValue%22%3A+++23%2C++++%22stepSize%22%3A++1+%7D%2C%0A%7B+%22name%22%3A+%22grid_char10%22%2C++%22unit%22%3A+%22alpha%22%2C++++++%22lowValue%22%3A+++0%2C++%22highValue%22%3A+++++23%2C++%22stepSize%22%3A++1+%7D%2C%0A%7B+%22name%22%3A+%22since_boot%22%2C+++%22unit%22%3A+%22minutes10%22%2C++++%22lowValue%22%3A+++0%2C++%22highValue%22%3A+++++100%2C++%22stepSize%22%3A++1+%7D%2C%0A%7B+%22name%22%3A+%22gps_aquisition%22%2C++%22unit%22%3A+%22seconds10%22%2C++%22lowValue%22%3A+++0%2C++%22highValue%22%3A+++++100%2C++%22stepSize%22%3A++1+%7D%2C&slot4MsgDefUserDefined=%7B+%22name%22%3A+%22since_boot%22%2C++++++%22unit%22%3A+%22mins%22%2C++%22lowValue%22%3A+++0%2C+%22highValue%22%3A+6000%2C++++%22stepSize%22%3A+1+%7D%2C%0A%7B+%22name%22%3A+%22GPS_aquisition%22%2C%22unit%22%3A+%22secs%22%2C%22lowValue%22%3A+++0%2C+%22highValue%22%3A+200%2C++++%22stepSize%22%3A+1+%7D%2C%0A%7B+%22name%22%3A+%22Vbus%22%2C+++%22unit%22%3A+%22V_hundreths%22%2C++%22lowValue%22%3A+++0%2C+%22highValue%22%3A+500%2C++++%22stepSize%22%3A+1+%7D%2C) The small board in front has 3 LM66100 "ideal diodes" for isolating the solar panels. On board MCP1640T needs only ~2.2 volts to run the tracker.

I owe great thanks and acknowledgement to Roman Piksaykin (https://github.com/RPiks/) for the original program that was the starting point for pico-WSPRer. Much of his code still lives on inside JAWBONE and I have tried to maintain his copyright information in all relevant files.


Rp2040 main clock runs at only 18Mhz.

**Power**

Current consumption (3.3V):
idle: 10mA
w/GPS: 34mA
w/MS5351: 56mA  (transmitting double ended into 70ohm)

The pin marked "5V" feeds into 3.3V ldo regulator, which feeds VBUS. VBUS is boosted if needed by a MCP1640T.

So "5V" is a universal power input, tolerant of anything between 2v and 7v. But the VBUS input can only be fed by sources between 2v and 3.3v. 

**telemetry changes Oct 2025:** (assumes config 58-)

DEXT type 5 in traquito format (both times roll over):
```
{ "name": "grid_char7",   "unit": "digit",   "lowValue":   0,    "highValue": 9,    "stepSize": 1   },
{ "name": "grid_char8",   "unit": "digit",    "lowValue":   0,    "highValue":    9,    "stepSize":  1 },
{ "name": "grid_char9",   "unit": "alpha",      "lowValue":   0,    "highValue":   23,    "stepSize":  1 },
{ "name": "grid_char10",  "unit": "alpha",      "lowValue":   0,  "highValue":     23,  "stepSize":  1 },
{ "name": "since_boot",   "unit": "minutes10",    "lowValue":   0,  "highValue":     100,  "stepSize":  1 },
{ "name": "gps_aquisition",  "unit": "seconds10",  "lowValue":   0,  "highValue":     100,  "stepSize":  1 },
```
type 5 TWITS Format:
```
#slot 3 (DExt type 5)
grid7,Count,0,9,1,3
grid8,Count,0,9,1,3
grid8,Count,0,23,1,3
grid10,Count,0,23,1,3
sinceBoot10,Count,0,100,1,3
GPS_aqui_10,Count,0,100,1,3
```

DEXT type 8 in traquito format (gps time is CLAMPED):
```
{ "name": "since_boot",      "unit": "mins",  "lowValue":   0, "highValue": 6000,    "stepSize": 1 },
{ "name": "GPS_aquisition","unit": "secs","lowValue":   0, "highValue": 200,    "stepSize": 1 },
{ "name": "Vbus",   "unit": "V_hundreths",  "lowValue":   0, "highValue": 500,    "stepSize": 1 },
```
type 8 TWITS Format:
```
#slot 4 (DExt type 8)
mins_since_boot,Count,0,6000,1,4
GPS_aqu_secs,Count,0,200,1,4
vbus,Count,0,500,1,4
```


**random notes:**

Optional_Debug bitmapping (MSB to LSB)
	7-
	6-.,
	5- .,
	4- .,
	3- .,
	2-debug WSPRBeacon dispatch
	1-IO Status
	0-raw GPS dump



power notes: (donor board)
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

