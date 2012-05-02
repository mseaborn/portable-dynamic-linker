
#include <stdint.h>

#include <asm/ldt.h>
#include <asm/prctl.h>

#include "minimal_libc.h"


int main(int argc, char **argv);

struct glibc_tcbhead {
  void *tcb;
  void *dtv;
  void *self;
  int multiple_threads;
  void *sysinfo;
  uintptr_t stack_guard;
  uintptr_t pointer_guard;
  int gscope_flag;
  char reserved[100];
};

static struct glibc_tcbhead initial_tls;

__attribute__((visibility("hidden"))) void syscall_routine();
asm(".pushsection \".text\",\"ax\",@progbits\n"
    "syscall_routine:\n"
#if defined(__i386__)
    "int $0x80\n"
    "ret\n"
#elif defined(__x86_64__)
    "syscall\n"
    "ret\n"
#else
# error Unsupported architecture
#endif
    ".popsection\n");

static void init_tls() {
#if defined(__i386__)
  initial_tls.sysinfo = syscall_routine;

  struct user_desc ud;
  /* memset(&ud, 0, sizeof(ud)); */
  ud.read_exec_only = 0;
  ud.seg_32bit = 1;
  ud.seg_not_present = 0;
  ud.useable = 1;
  ud.base_addr = (uintptr_t) &initial_tls;
  ud.entry_number = -1;
  ud.contents = MODIFY_LDT_CONTENTS_DATA;
  ud.limit = 0xfffff; /* All of 32-bit address space */
  ud.limit_in_pages = 1;
  int rc = sys_set_thread_area(&ud);
  assert(rc == 0);
  int selector = (ud.entry_number << 3) | 3;
  asm("mov %0, %%gs" : : "r"(selector));
#elif defined(__x86_64__)
  int rc = sys_arch_prctl(ARCH_SET_FS, &initial_tls);
  assert(rc == 0);
#else
# error Unsupported architecture
#endif
}

void do_load(uintptr_t *stack) {
  int argc = stack[0];
  char **argv = (char **) &stack[1];
  init_tls();
  sys_exit_group(main(argc, argv));
}

/*
 * We have to define the actual entry point code (_start) in assembly for
 * each machine.  The kernel startup protocol is not compatible with the
 * normal C function calling convention.  Here, we call do_load (above)
 * using the normal C convention as per the ABI, with the starting stack
 * pointer as its argument; restore the original starting stack; and
 * finally, jump to the dynamic linker's entry point address.
 */
#if defined(__i386__)
asm(".pushsection \".text\",\"ax\",@progbits\n"
    ".globl _start\n"
    ".type _start,@function\n"
    "_start:\n"
    "xorl %ebp, %ebp\n"
    "movl %esp, %ebx\n"         /* Save starting SP in %ebx.  */
    "andl $-16, %esp\n"         /* Align the stack as per ABI.  */
    "pushl %ebx\n"              /* Argument: stack block.  */
    "call do_load\n"
    "movl %ebx, %esp\n"         /* Restore the saved SP.  */
    "jmp *%eax\n"               /* Jump to the entry point.  */
    ".popsection"
    );
#elif defined(__x86_64__)
asm(".pushsection \".text\",\"ax\",@progbits\n"
    ".globl _start\n"
    ".type _start,@function\n"
    "_start:\n"
    "xorq %rbp, %rbp\n"
    "movq %rsp, %rbx\n"         /* Save starting SP in %rbx.  */
    "andq $-16, %rsp\n"         /* Align the stack as per ABI.  */
    "movq %rbx, %rdi\n"         /* Argument: stack block.  */
    "call do_load\n"
    "movq %rbx, %rsp\n"         /* Restore the saved SP.  */
    "jmp *%rax\n"               /* Jump to the entry point.  */
    ".popsection"
    );
#elif defined(__arm__)
asm(".pushsection \".text\",\"ax\",%progbits\n"
    ".globl _start\n"
    ".type _start,#function\n"
    "_start:\n"
#if defined(__thumb2__)
    ".thumb\n"
    ".syntax unified\n"
#endif
    "mov fp, #0\n"
    "mov lr, #0\n"
    "mov r4, sp\n"              /* Save starting SP in r4.  */
    "mov r0, sp\n"              /* Argument: stack block.  */
    "bl do_load\n"
    "mov sp, r4\n"              /* Restore the saved SP.  */
    "blx r0\n"                  /* Jump to the entry point.  */
    ".popsection"
    );
#else
# error "Need stack-preserving _start code for this architecture!"
#endif
