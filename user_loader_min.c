
#include <stdio.h>
#include <string.h>

#include "minimal_libc.h"
#include "shared.h"


const char *example_import0() {
  return "called imported func #0";
}

const char *example_import1() {
  return "called imported func #1";
}

static int resolver_call_count = 0;
static int resolver_last_id = -1;

void *my_plt_resolver(void *handle, int import_id) {
  struct dynnacl_obj *dynnacl_obj = handle;
  printf("resolver called for func #%i!\n", import_id);
  resolver_call_count++;
  resolver_last_id = import_id;

  void *funcs[] = {
    (void *) example_import0,
    (void *) example_import1,
  };
  void *func = funcs[import_id];
  dynnacl_set_plt_entry(dynnacl_obj, import_id, func);
  return func;
}

int main() {
  struct dynnacl_obj *dynnacl_obj =
    dynnacl_load_from_elf_file("example_lib_min.so");

  void **function_table = dynnacl_get_user_root(dynnacl_obj);

  printf("testing exported functions...\n");
  const char *(*func)(void);
  const char *result;
  func = (const char *(*)(void)) function_table[0];
  result = func();
  assert(strcmp(result, "example string") == 0);

  func = (const char *(*)(void)) function_table[1];
  result = func();
  assert(strcmp(result, "another example string") == 0);

  printf("testing imported functions...\n");
  dynnacl_set_plt_resolver(dynnacl_obj, my_plt_resolver, dynnacl_obj);

  func = (const char *(*)(void)) function_table[2];
  result = func();
  assert(strcmp(result, "called imported func #0") == 0);
  assert(resolver_call_count == 1);
  assert(resolver_last_id == 0);
  resolver_call_count = 0;
  result = func();
  assert(strcmp(result, "called imported func #0") == 0);
  assert(resolver_call_count == 0);

  func = (const char *(*)(void)) function_table[3];
  result = func();
  assert(strcmp(result, "called imported func #1") == 0);
  assert(resolver_call_count == 1);
  assert(resolver_last_id == 1);
  resolver_call_count = 0;
  result = func();
  assert(strcmp(result, "called imported func #1") == 0);
  assert(resolver_call_count == 0);

  printf("passed\n");
  return 0;
}
