
#include "shared.h"

const char *foo() {
  return "example string";
}

const char *bar() {
  return "another example string";
}

const char *import_func0();
const char *import_func1();

PLT_BEGIN;
PLT_ENTRY(0, import_func0);
PLT_ENTRY(1, import_func1);

const char *test_import0() {
  return import_func0();
}

const char *test_import1() {
  return import_func1();
}

void *function_table[] = {
  (void *) foo,
  (void *) bar,
  (void *) test_import0,
  (void *) test_import1,
};

DEFINE_HEADER(function_table);
