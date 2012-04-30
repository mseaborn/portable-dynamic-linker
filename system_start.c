
#include <stdint.h>

#include "minimal_libc.h"


int main(int argc, char **argv);

void do_load(uintptr_t *stack) {
  int argc = stack[0];
  char **argv = (char **) &stack[1];
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
