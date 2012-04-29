
const char *foo() {
  return "example string";
}

const char *bar() {
  return "another example string";
}

const char *import_func0();
const char *import_func1();

const char *test_import0() {
  return import_func0();
}

const char *test_import1() {
  return import_func1();
}
