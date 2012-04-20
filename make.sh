#!/bin/bash

set -eux

gcc -Wall -shared -fPIC -Wl,--entry=foo example_lib.c -o example_lib.so
gcc -Wall system_loader.c -o loader
./loader
