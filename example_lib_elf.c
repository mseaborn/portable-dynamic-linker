
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

/* Generates a R_*_GLOB_DAT relocation. */
int var = 123;
int test_var() {
  return var;
}

/* Generates a R_386_32/R_X86_64_64 relocation. */
int var2 = 456;
static int *var2_ptr = &var2;
int test_var2() {
  return *var2_ptr;
}

__thread int tls_var = 321;
int test_tls_var() {
  return tls_var;
}

__attribute__((tls_model("initial-exec")))
__thread int tls_var_ie = 654;
int test_tls_var_ie() {
  return tls_var_ie;
}
