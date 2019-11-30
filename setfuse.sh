#!/bin/bash
# Sets EESAVE fuse

~/devel/avr/toolchain/pyupdi/pyupdi.py -d tiny814 -c /dev/ttyUSB0 -fs 5:0xf7 -b 9600
