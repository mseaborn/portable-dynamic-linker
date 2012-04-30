#!/bin/bash

set -eux

for bits in 32 64; do
  flags="-m$bits -Wall -Werror -g -fno-builtin-printf"
  dir="out-$bits"
  mkdir -p $dir

  system_lib="system_loader.c system_start.c minimal_libc.c -nostdlib"
  test_flags="-pie -fPIE -fvisibility=hidden"

  gcc $flags -fvisibility=hidden -shared -fPIC -c \
      example_lib_min.c -o $dir/example_lib_min.o
  gcc $flags -shared -Wl,--entry=prog_header -Wl,-z,defs -nostdlib \
      $dir/example_lib_min.o -o $dir/example_lib_min.so
  gcc $flags $test_flags $system_lib user_loader_min.c -o $dir/loader_min
  (cd $dir && ./loader_min) || false

  gcc $flags -shared -fPIC -c \
      example_lib_elf.c -o $dir/example_lib_elf.o
  gcc $flags -shared -nostdlib \
      $dir/example_lib_elf.o -o $dir/example_lib_elf.so
  gcc $flags $test_flags $system_lib user_loader_elf.c -o $dir/loader_elf
  (cd $dir && ./loader_elf) || false
done
