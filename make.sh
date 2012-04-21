#!/bin/bash

set -eux

# Use 32-bit to make this more similar to NaCl.
flags="-m32 -Wall -g"
gcc $flags -fvisibility=hidden -shared -fPIC -c example_lib.c
gcc $flags -shared -Wl,--entry=prog_header -Wl,-z,defs -nostdlib \
    example_lib.o -o example_lib.so
gcc $flags system_loader.c -o loader
./loader
