# rotor_control
Control of SPID antenna rotors. These have DC-motor and pulse output. Commands are sent to the controller over simple UART-interface. Add RS-232 level shifters for long cables.

Targets an ATtiny814 but should with very minor modifications work on any 8 kbit flash (or more) ATtiny or ATmega of newer series (those with UPDI for programming).  
The script make.sh invokes the compiler and necessary device packs. These are available pre-compiled from Microchip. Edit paths etc to make script work.  
The script program.sh invokes PyUPDI for programming. Path has to be edited.
Alternatively just import the code into Atmel Studio and use any therein supported programmer.
