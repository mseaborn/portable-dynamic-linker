
const char *foo() {
  return "example string";
}

const char *bar() {
  return "another example string";
}

void *function_table[] = {
  (void *) foo,
  (void *) bar,
};
