
#ifndef SHARED_H_
#define SHARED_H_

struct prog_header {
  void **plt_trampoline;
  void **pltgot;
  void *user_info;
};

/* PLT entries assume that %ebx is set up on entry on x86-32.  They
   therefore don't work as address-taken functions.  The address-taken
   version should be read from the GOT instead. */

extern void *pltgot_imports[];

#define PLT_BEGIN \
  asm(".pushsection \".text\",\"ax\",@progbits\n" \
      "slowpath_common:\n" \
      "push plt_handle@GOTOFF(%ebx)\n" \
      "jmp *plt_trampoline@GOTOFF(%ebx)\n" \
      ".popsection\n" \
      /* Start of PLTGOT table. */ \
      ".pushsection \".my_pltgot\",\"aw\",@progbits\n" \
      "pltgot_imports:\n" \
      ".popsection\n");

/* These PLT entries are more compact than those generated by binutils
   because they can use the short forms of the "jmp" and "push"
   instructions. */
/* To consider: In x86-32 ELF, the argument pushed by the slow path is
   an offset (a multiple of 8) rather than an index.  But in x86-64
   ELF, the argument pushed is an index.  We use an index below. */
#define PLT_ENTRY(number, name) \
  asm(".pushsection \".text\",\"ax\",@progbits\n" \
      #name ":\n" \
      "jmp *pltgot_" #name "@GOTOFF(%ebx)\n" \
      "slowpath_" #name ":\n" \
      "push $" #number "\n" \
      "jmp slowpath_common\n" \
      ".popsection\n" \
      /* Entry in PLTGOT table */ \
      ".pushsection \".my_pltgot\",\"aw\",@progbits\n" \
      "pltgot_" #name ":\n" \
      ".long slowpath_" #name "\n" \
      ".popsection\n");

#endif
