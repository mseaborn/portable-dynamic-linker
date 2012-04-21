#!/bin/bash

set -eux

# Use 32-bit to make this more similar to NaCl.
flags="-m32 -Wall"
gcc $flags -fvisibility=hidden -nostdlib -shared -fPIC \
    -Wl,--entry=function_table example_lib.c -o example_lib.so
gcc $flags system_loader.c -o loader
./loader
