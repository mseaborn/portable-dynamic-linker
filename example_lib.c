
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
const char *import_func0();
const char *import_func1();
void *slowpath_import_func0();
void *slowpath_import_func1();
void *pltgot_imports[] = {
  slowpath_import_func0,
  slowpath_import_func1,
};
asm(".pushsection \".text\",\"ax\",@progbits\n"
    "slowpath_common:\n"
    "push plt_handle@GOTOFF(%ebx)\n"
    "jmp *plt_trampoline@GOTOFF(%ebx)\n"
    ".popsection\n");
asm(".pushsection \".text\",\"ax\",@progbits\n"
    "import_func0:\n"
    "jmp *pltgot_imports@GOTOFF(%ebx)\n"
    "slowpath_import_func0:\n"
    "push $0\n"
    "jmp slowpath_common\n"
    ".popsection\n");
asm(".pushsection \".text\",\"ax\",@progbits\n"
    "import_func1:\n"
    "jmp *pltgot_imports+4@GOTOFF(%ebx)\n"
    "slowpath_import_func1:\n"
    "push $1\n"
    "jmp slowpath_common\n"
    ".popsection\n");

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

void *plt_trampoline;

struct prog_header prog_header = {
  .plt_trampoline = &plt_trampoline,
  .pltgot = &pltgot_imports,
  .user_info = function_table,
};

struct prog_header *plt_handle = &prog_header;
