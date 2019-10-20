#!/bin/bash

~/devel/avr/toolchain/avr8-gnu-toolchain-linux_x86_64/bin/avr-gcc -mmcu=attiny814 -B ~/devel/avr/toolchain/Atmel.ATtiny_DFP.1.3.229.atpack_FILES/gcc/dev/attiny814/ -I ~/devel/avr/toolchain/Atmel.ATtiny_DFP.1.3.229.atpack_FILES/include/ -O3 main.c -o main.elf

~/devel/avr/toolchain/avr8-gnu-toolchain-linux_x86_64/bin/avr-objcopy -O ihex main.elf main.hex

