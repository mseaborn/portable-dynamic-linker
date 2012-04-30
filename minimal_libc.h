
#ifndef MINIMAL_LIBC_H_
#define MINIMAL_LIBC_H_

static int my_errno;
#define SYS_ERRNO my_errno
#include "linux_syscall_support.h"

#define TO_STRING_1(x) #x
#define TO_STRING(x) TO_STRING_1(x)

void die(const char *msg);

#define assert(expr) {                                                        \
  if (!(expr)) die("Assertion failed at " __FILE__ ":" TO_STRING(__LINE__)    \
                   ": " #expr "\n"); }

#endif
