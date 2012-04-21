
#include "shared.h"

const char *foo() {
  return "example string";
}

const char *bar() {
  return "another example string";
}

/* PLT entries assume that %ebx is set up on entry on x86-32.  They
   therefore don't work as address-taken functions.  The address-taken
   version should be read from the GOT instead. */
const char *import_func();
void *pltgot_import_func;
asm("import_func: jmp *pltgot_import_func@GOTOFF(%ebx)");

const char *qux() {
  return import_func();
}

void *function_table[] = {
  (void *) foo,
  (void *) bar,
  (void *) qux,
};

struct prog_header prog_header = {
  .pltgot = &pltgot_import_func,
  .user_info = function_table,
};
