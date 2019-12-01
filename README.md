# rotor_control
Control of SPID antenna rotors. These have DC-motor and 1 pulse per degree output. Commands are sent to the controller over simple UART-interface, typically in a terminal emulator such as miniterm. Add RS-232 level shifters for long cables.
See schematic in folder of this repo. No PCB layout is available but due to the very simple circuit any simple dev/proto board with a bit of wires will do.

The code targets an ATtiny814 but should with very minor modifications work on any 8 kbit flash (or more) ATtiny or ATmega of newer series (those with UPDI for programming). For some chips, like ATtiny817, there are cheap dev boards with on-board USB-programmers available from Microchip.  
The script make.sh invokes the compiler and necessary device packs. These are available pre-compiled from Microchip. Edit paths etc to make script work.  
The script program.sh invokes PyUPDI (clone from https://github.com/mraardvark/pyupdi) to use a simple UART port (most often an USB-UART converter of FTDI type) for programming. Path has to be edited.  
Alternatively just import the code into Atmel Studio and use any therein supported programmer.  
The command in setfuse.sh sets the EESAVE fuse such that rotor position is saved also after software upgrade.

---

Commands:  
r x   --- Rotate to position x degrees  
r+ x  --- Rotate clock-wise for x degrees  
r- x  --- Rotate counter-clock-wise for x degrees  
d     --- Print current postion  
s     --- Stop ongoing rotation  
cal x --- Set current position to x degrees (without rotating)   
temp  --- Measure and print current temperature

---

Additional functionality

A timeout function stops the rotator if no pulse is received from the rotator indicator in approximately 2 seconds. This ensures no sustained rotation if indication malfunction. Note that these types of rotators have no mechanical end-stop switches so uncontrolled rotation can cause antenna cables to get damaged or wear out the rotator engine.
There is also a function that monitors the rotator voltage and stops any ongoing rotation and saves the current position to EEPROM if this voltage drops below a threshold level. This is both for safety, if the rotator draws too much current causing a large voltage drop, and to save current position when the system is turned off. (If this function is disabled there is still an internal voltage monitor that performs the latter task is the 5V supply drops out.)

A Spid RAU rotator draws around 1A at 18V in steady rotation, but can draw several amperes in start events. Adding a 1Ohm/1W resistor in series from the supply, with a large electrolytic capacitor (10 000 uF or so) to ground afterwards, is an easy way to offload the supply at start events and at the same time create additional safety in form of a resistor fuse that burns out if the rotator gets stuck. 

Change log:

* 2019-11-30 - Fully functional and tested version
* 2019-12-01 - Docs updated, schematics added to repo
