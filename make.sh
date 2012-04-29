#!/bin/bash

set -eux

for bits in 32 64; do
  flags="-m$bits -Wall -Werror -g"
  dir="out-$bits"
  mkdir -p $dir
  gcc $flags -fvisibility=hidden -shared -fPIC -c \
      example_lib.c -o $dir/example_lib.o
  gcc $flags -shared -Wl,--entry=prog_header -Wl,-z,defs -nostdlib \
      $dir/example_lib.o -o $dir/example_lib.so
  gcc $flags system_loader.c user_loader.c -o $dir/loader
  (cd $dir && ./loader) || false
done
