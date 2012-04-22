
#include <stdio.h>

#include "shared.h"


const char *example_import0() {
  return "called imported func #0";
}

const char *example_import1() {
  return "called imported func #1";
}

void *my_plt_resolver(struct prog_header *prog_header, int import_id) {
  printf("resolver called for func #%i!\n", import_id);
  void *funcs[] = {
    (void *) example_import0,
    (void *) example_import1,
  };
  prog_header->pltgot[import_id] = funcs[import_id];
  return funcs[import_id];
}

int main() {
  struct prog_header *prog_header = load_from_elf_file("example_lib.so");

  void **function_table = prog_header->user_info;

  const char *(*func)(void);
  func = (const char *(*)(void)) function_table[0];
  printf("result: '%s'\n", func());

  func = (const char *(*)(void)) function_table[1];
  printf("result: '%s'\n", func());

  prog_header->user_plt_resolver = my_plt_resolver;

  func = (const char *(*)(void)) function_table[2];
  printf("result: '%s'\n", func());
  printf("result: '%s'\n", func());
  func = (const char *(*)(void)) function_table[3];
  printf("result: '%s'\n", func());
  printf("result: '%s'\n", func());

  return 0;
}
