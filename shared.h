
#ifndef SHARED_H_
#define SHARED_H_

struct prog_header {
  void **plt_trampoline;
  void **pltgot;
  void *user_info;
};

#endif
