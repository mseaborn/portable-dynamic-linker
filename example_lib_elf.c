
const char *foo() {
  return "example string";
}

const char *bar() {
  return "another example string";
}

const char *import_func0();
const char *import_func1();
int import_args_test(int arg1, int arg2, int arg3, int arg4,
                     int arg5, int arg6, int arg7, int arg8);

const char *test_import0() {
  return import_func0();
}

const char *test_import1() {
  return import_func1();
}

int test_args_via_plt() {
  return import_args_test(1, 2, 3, 4, 5, 6, 7, 8);
}
