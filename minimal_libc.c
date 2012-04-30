
#include <string.h>
#include <sys/mman.h>

#include "minimal_libc.h"


size_t strlen(const char *s) {
  size_t n = 0;
  while (*s++ != '\0')
    ++n;
  return n;
}

int strcmp(const char *s1, const char *s2) {
  while (1) {
    if (*s1 < *s2) {
      return -1;
    }
    if (*s2 < *s1) {
      return 1;
    }
    if (*s1 == 0 && *s2 == 0) {
      return 0;
    }
    s1++;
    s2++;
  }
}

void die(const char *msg) {
  sys_write(2, msg, strlen(msg));
  sys_exit_group(1);
}

static void *my_mmap(void *addr, size_t size, int prot, int flags,
                     int fd, off_t offset) {
#if defined(__NR_mmap2)
  return sys_mmap2(addr, size, prot, flags, fd, offset >> 12);
#else
  return sys_mmap(addr, size, prot, flags, fd, offset);
#endif
}

void *malloc(size_t size) {
  char *mapped = my_mmap(NULL, size, PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (mapped == MAP_FAILED)
    return NULL;
  return mapped;
}

int printf(const char *fmt, ...) {
  sys_write(1, fmt, strlen(fmt));
  return 0;
}
