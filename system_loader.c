/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
*/

#define _GNU_SOURCE
#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <link.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

#include "shared.h"

#define MAX_PHNUM               12


struct dynnacl_obj {
  uintptr_t load_bias;
  void *entry;
  ElfW(Dyn) *pt_dynamic;

  user_plt_resolver_t user_plt_resolver;
  void *user_plt_resolver_handle;
};

#if defined(__i386__)
typedef ElfW(Rel) ElfW_Reloc;
# define ELFW_R_TYPE(x) ELF32_R_TYPE(x)
# define ELFW_DT_RELW DT_REL
# define ELFW_DT_RELWSZ DT_RELSZ
#elif defined(__x86_64__)
typedef ElfW(Rela) ElfW_Reloc;
# define ELFW_R_TYPE(x) ELF64_R_TYPE(x)
# define ELFW_DT_RELW DT_RELA
# define ELFW_DT_RELWSZ DT_RELASZ
#else
# error Unsupported architecture
#endif


/*
 * We're not using <string.h> functions here, to avoid dependencies.
 * In the x86 libc, even "simple" functions like memset and strlen can
 * depend on complex startup code, because in newer libc
 * implementations they are defined using STT_GNU_IFUNC.
 */

static void my_bzero(void *buf, size_t n) {
  char *p = buf;
  while (n-- > 0)
    *p++ = 0;
}

static size_t my_strlen(const char *s) {
  size_t n = 0;
  while (*s++ != '\0')
    ++n;
  return n;
}


/*
 * We're avoiding libc, so no printf.  The only nontrivial thing we need
 * is rendering numbers, which is, in fact, pretty trivial.
 * bufsz of course must be enough to hold INT_MIN in decimal.
 */
static void iov_int_string(int value, struct iovec *iov,
                           char *buf, size_t bufsz) {
  char *p = &buf[bufsz];
  int negative = value < 0;
  if (negative)
    value = -value;
  do {
    --p;
    *p = "0123456789"[value % 10];
    value /= 10;
  } while (value != 0);
  if (negative)
    *--p = '-';
  iov->iov_base = p;
  iov->iov_len = &buf[bufsz] - p;
}

#define STRING_IOV(string_constant, cond) \
  { (void *) string_constant, cond ? (sizeof(string_constant) - 1) : 0 }

__attribute__((noreturn)) static void fail(const char *filename,
                                           const char *message,
                                           const char *item1, int value1,
                                           const char *item2, int value2) {
  char valbuf1[32];
  char valbuf2[32];
  struct iovec iov[] = {
    STRING_IOV("bootstrap_helper: ", 1),
    { (void *) filename, my_strlen(filename) },
    STRING_IOV(": ", 1),
    { (void *) message, my_strlen(message) },
    { (void *) item1, item1 == NULL ? 0 : my_strlen(item1) },
    STRING_IOV("=", item1 != NULL),
    { NULL, 0 },                        /* iov[6] */
    STRING_IOV(", ", item1 != NULL && item2 != NULL),
    { (void *) item2, item2 == NULL ? 0 : my_strlen(item2) },
    STRING_IOV("=", item2 != NULL),
    { NULL, 0 },                        /* iov[10] */
    { "\n", 1 },
  };
  const int niov = sizeof(iov) / sizeof(iov[0]);

  if (item1 != NULL)
    iov_int_string(value1, &iov[6], valbuf1, sizeof(valbuf1));
  if (item2 != NULL)
    iov_int_string(value2, &iov[10], valbuf2, sizeof(valbuf2));

  writev(2, iov, niov);
  exit(2);
}


static int my_open(const char *file, int oflag) {
  int result = open(file, oflag, 0);
  if (result < 0)
    fail(file, "Cannot open ELF file!  ", "errno", errno, NULL, 0);
  return result;
}

static void my_pread(const char *file, const char *fail_message,
                     int fd, void *buf, size_t bufsz, uintptr_t pos) {
  ssize_t result = pread(fd, buf, bufsz, pos);
  if (result < 0)
    fail(file, fail_message, "errno", errno, NULL, 0);
  if ((size_t) result != bufsz)
    fail(file, fail_message, "read count", result, NULL, 0);
}

static uintptr_t my_mmap(const char *file,
                         const char *segment_type, unsigned int segnum,
                         uintptr_t address, size_t size,
                         int prot, int flags, int fd, uintptr_t pos) {
  void *result = mmap((void *) address, size, prot, flags, fd, pos);
  if (result == MAP_FAILED)
    fail(file, "Failed to map segment!  ",
         segment_type, segnum, "errno", errno);
  return (uintptr_t) result;
}

static void my_mprotect(const char *file, unsigned int segnum,
                        uintptr_t address, size_t size, int prot) {
  if (mprotect((void *) address, size, prot) < 0)
    fail(file, "Failed to mprotect segment hole!  ",
         "segment", segnum, "errno", errno);
}


static int prot_from_phdr(const ElfW(Phdr) *phdr) {
  int prot = 0;
  if (phdr->p_flags & PF_R)
    prot |= PROT_READ;
  if (phdr->p_flags & PF_W)
    prot |= PROT_WRITE;
  if (phdr->p_flags & PF_X)
    prot |= PROT_EXEC;
  return prot;
}

static uintptr_t round_up(uintptr_t value, uintptr_t size) {
  return (value + size - 1) & -size;
}

static uintptr_t round_down(uintptr_t value, uintptr_t size) {
  return value & -size;
}

/*
 * Handle the "bss" portion of a segment, where the memory size
 * exceeds the file size and we zero-fill the difference.  For any
 * whole pages in this region, we over-map anonymous pages.  For the
 * sub-page remainder, we zero-fill bytes directly.
 */
static void handle_bss(const char *file,
                       unsigned int segnum, const ElfW(Phdr) *ph,
                       ElfW(Addr) load_bias, size_t pagesize) {
  if (ph->p_memsz > ph->p_filesz) {
    ElfW(Addr) file_end = ph->p_vaddr + load_bias + ph->p_filesz;
    ElfW(Addr) file_page_end = round_up(file_end, pagesize);
    ElfW(Addr) page_end = round_up(ph->p_vaddr + load_bias +
                                   ph->p_memsz, pagesize);
    if (page_end > file_page_end)
      my_mmap(file, "bss segment", segnum,
              file_page_end, page_end - file_page_end,
              prot_from_phdr(ph), MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (file_page_end > file_end && (ph->p_flags & PF_W))
      my_bzero((void *) file_end, file_page_end - file_end);
  }
}

ElfW(Word) get_dynamic_entry(ElfW(Dyn) *dynamic, int field) {
  for (; dynamic->d_tag != DT_NULL; dynamic++) {
    if (dynamic->d_tag == field) {
      return dynamic->d_un.d_val;
    }
  }
  /* TODO: Distinguish between 0 and the field not being present. */
  return 0;
}

/*
 * Open an ELF file and load it into memory.
 */
struct dynnacl_obj *load_elf_file(const char *filename,
                                  size_t pagesize,
                                  ElfW(Addr) *out_phdr,
                                  ElfW(Addr) *out_phnum,
                                  const char **out_interp) {
  int fd = my_open(filename, O_RDONLY);

  ElfW(Ehdr) ehdr;
  my_pread(filename, "Failed to read ELF header from file!  ",
           fd, &ehdr, sizeof(ehdr), 0);

  if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
      ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
      ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
      ehdr.e_ident[EI_MAG3] != ELFMAG3 ||
      ehdr.e_version != EV_CURRENT ||
      ehdr.e_ehsize != sizeof(ehdr) ||
      ehdr.e_phentsize != sizeof(ElfW(Phdr)))
    fail(filename, "File has no valid ELF header!", NULL, 0, NULL, 0);

  switch (ehdr.e_machine) {
#if defined(__i386__)
    case EM_386:
#elif defined(__x86_64__)
    case EM_X86_64:
#elif defined(__arm__)
    case EM_ARM:
#else
# error "Don't know the e_machine value for this architecture!"
#endif
      break;
    default:
      fail(filename, "ELF file has wrong architecture!  ",
           "e_machine", ehdr.e_machine, NULL, 0);
      break;
  }

  ElfW(Phdr) phdr[MAX_PHNUM];
  if (ehdr.e_phnum > sizeof(phdr) / sizeof(phdr[0]) || ehdr.e_phnum < 1)
    fail(filename, "ELF file has unreasonable ",
         "e_phnum", ehdr.e_phnum, NULL, 0);

  if (ehdr.e_type != ET_DYN)
    fail(filename, "ELF file not ET_DYN!  ",
         "e_type", ehdr.e_type, NULL, 0);

  my_pread(filename, "Failed to read program headers from ELF file!  ",
           fd, phdr, sizeof(phdr[0]) * ehdr.e_phnum, ehdr.e_phoff);

  size_t i = 0;
  while (i < ehdr.e_phnum && phdr[i].p_type != PT_LOAD)
    ++i;
  if (i == ehdr.e_phnum)
    fail(filename, "ELF file has no PT_LOAD header!",
         NULL, 0, NULL, 0);

  /*
   * ELF requires that PT_LOAD segments be in ascending order of p_vaddr.
   * Find the last one to calculate the whole address span of the image.
   */
  const ElfW(Phdr) *first_load = &phdr[i];
  const ElfW(Phdr) *last_load = &phdr[ehdr.e_phnum - 1];
  while (last_load > first_load && last_load->p_type != PT_LOAD)
    --last_load;

  size_t span = last_load->p_vaddr + last_load->p_memsz - first_load->p_vaddr;

  /*
   * Map the first segment and reserve the space used for the rest and
   * for holes between segments.
   */
  const uintptr_t mapping = my_mmap(filename, "segment", first_load - phdr,
                                    round_down(first_load->p_vaddr, pagesize),
                                    span, prot_from_phdr(first_load),
                                    MAP_PRIVATE, fd,
                                    round_down(first_load->p_offset, pagesize));

  const ElfW(Addr) load_bias = mapping - round_down(first_load->p_vaddr,
                                                    pagesize);

  if (first_load->p_offset > ehdr.e_phoff ||
      first_load->p_filesz < ehdr.e_phoff + (ehdr.e_phnum * sizeof(ElfW(Phdr))))
    fail(filename, "First load segment of ELF file does not contain phdrs!",
         NULL, 0, NULL, 0);

  handle_bss(filename, first_load - phdr, first_load, load_bias, pagesize);

  ElfW(Addr) last_end = first_load->p_vaddr + load_bias + first_load->p_memsz;

  /*
   * Map the remaining segments, and protect any holes between them.
   */
  const ElfW(Phdr) *ph;
  for (ph = first_load + 1; ph <= last_load; ++ph) {
    if (ph->p_type == PT_LOAD) {
      ElfW(Addr) last_page_end = round_up(last_end, pagesize);

      last_end = ph->p_vaddr + load_bias + ph->p_memsz;
      ElfW(Addr) start = round_down(ph->p_vaddr + load_bias, pagesize);
      ElfW(Addr) end = round_up(last_end, pagesize);

      if (start > last_page_end)
        my_mprotect(filename,
                    ph - phdr, last_page_end, start - last_page_end, PROT_NONE);

      my_mmap(filename, "segment", ph - phdr,
              start, end - start,
              prot_from_phdr(ph), MAP_PRIVATE | MAP_FIXED, fd,
              round_down(ph->p_offset, pagesize));

      handle_bss(filename, ph - phdr, ph, load_bias, pagesize);
    }
  }

  if (out_interp != NULL) {
    /*
     * Find the PT_INTERP header, if there is one.
     */
    for (i = 0; i < ehdr.e_phnum; ++i) {
      if (phdr[i].p_type == PT_INTERP) {
        /*
         * The PT_INTERP isn't really required to sit inside the first
         * (or any) load segment, though it normally does.  So we can
         * easily avoid an extra read in that case.
         */
        if (phdr[i].p_offset >= first_load->p_offset &&
            phdr[i].p_filesz <= first_load->p_filesz) {
          *out_interp = (const char *) (phdr[i].p_vaddr + load_bias);
        } else {
          static char interp_buffer[PATH_MAX + 1];
          if (phdr[i].p_filesz >= sizeof(interp_buffer)) {
            fail(filename, "ELF file has unreasonable PT_INTERP size!  ",
                 "segment", i, "p_filesz", phdr[i].p_filesz);
          }
          my_pread(filename, "Cannot read PT_INTERP segment contents!",
                   fd, interp_buffer, phdr[i].p_filesz, phdr[i].p_offset);
          *out_interp = interp_buffer;
        }
        break;
      }
    }
  }

  /* Find PT_DYNAMIC header. */
  ElfW(Dyn) *dynamic = NULL;
  for (i = 0; i < ehdr.e_phnum; ++i) {
    if (phdr[i].p_type == PT_DYNAMIC) {
      assert(dynamic == NULL);
      dynamic = (ElfW(Dyn) *) (load_bias + phdr[i].p_vaddr);
    }
  }
  assert(dynamic != NULL);

  ElfW_Reloc *relocs =
    (ElfW_Reloc *) (load_bias +
                    get_dynamic_entry(dynamic, ELFW_DT_RELW));
  size_t relocs_size = get_dynamic_entry(dynamic, ELFW_DT_RELWSZ);
  for (i = 0; i < relocs_size / sizeof(ElfW_Reloc); i++) {
    ElfW_Reloc *reloc = &relocs[i];
    int reloc_type = ELFW_R_TYPE(reloc->r_info);
    switch (reloc_type) {
#if defined(__i386__)
      case R_386_RELATIVE:
#elif defined(__x86_64__)
      case R_X86_64_RELATIVE:
#endif
        {
          ElfW(Addr) *addr = (ElfW(Addr) *) (load_bias + reloc->r_offset);
          *addr += load_bias;
          break;
        }
    default:
      assert(0);
    }
  }

  struct dynnacl_obj *dynnacl_obj = malloc(sizeof(dynnacl_obj));
  assert(dynnacl_obj != NULL);

  dynnacl_obj->load_bias = load_bias;
  dynnacl_obj->entry = (void *) (ehdr.e_entry + load_bias);
  dynnacl_obj->pt_dynamic = dynamic;

  close(fd);

  if (out_phdr != NULL)
    *out_phdr = (ehdr.e_phoff - first_load->p_offset +
                 first_load->p_vaddr + load_bias);
  if (out_phnum != NULL)
    *out_phnum = ehdr.e_phnum;

  return dynnacl_obj;
}

void plt_trampoline();
#if defined(__i386__)
/* A more sophisticated version would save and restore registers, in
   case the function called through the PLT passes arguments in
   registers. */
asm(".pushsection \".text\",\"ax\",@progbits\n"
    "plt_trampoline:\n"
    "call system_plt_resolver\n"
    "add $8, %esp\n" /* Drop arguments */
    "jmp *%eax\n"
    ".popsection\n");
#elif defined(__x86_64__)
asm(".pushsection \".text\",\"ax\",@progbits\n"
    "plt_trampoline:\n"
    "pop %rdi\n" /* Argument 1 */
    "pop %rsi\n" /* Argument 2 */
    "call system_plt_resolver\n"
    "jmp *%rax\n"
    ".popsection\n");
#else
# error Unsupported architecture
#endif

void *system_plt_resolver(struct dynnacl_obj *dynnacl_obj, int import_id) {
  /* This could be inlined into the assembly code above, but that
     would require putting knowledge of the struct layout into the
     assembly code. */
  return dynnacl_obj->user_plt_resolver(dynnacl_obj->user_plt_resolver_handle,
                                        import_id);
}

struct dynnacl_obj *dynnacl_load_from_elf_file(const char *filename) {
  size_t pagesize = 0x1000;
  return load_elf_file(filename, pagesize, NULL, NULL, NULL);
}

void *dynnacl_get_user_root(struct dynnacl_obj *dynnacl_obj) {
  struct dynnacl_prog_header *prog_header = dynnacl_obj->entry;
  return prog_header->user_info;
}

void dynnacl_set_plt_resolver(struct dynnacl_obj *dynnacl_obj,
                              user_plt_resolver_t plt_resolver,
                              void *handle) {
  struct dynnacl_prog_header *prog_header = dynnacl_obj->entry;
  *prog_header->plt_trampoline = (void *) plt_trampoline;
  *prog_header->plt_handle = dynnacl_obj;
  dynnacl_obj->user_plt_resolver = plt_resolver;
  dynnacl_obj->user_plt_resolver_handle = handle;
}

void dynnacl_set_plt_entry(struct dynnacl_obj *dynnacl_obj,
                           int import_id, void *func) {
  struct dynnacl_prog_header *prog_header = dynnacl_obj->entry;
  prog_header->pltgot[import_id] = func;
}

uintptr_t elf_get_load_bias(struct dynnacl_obj *dynnacl_obj) {
  return dynnacl_obj->load_bias;
}

uintptr_t elf_get_dynamic_entry(struct dynnacl_obj *dynnacl_obj, int type) {
  return get_dynamic_entry(dynnacl_obj->pt_dynamic, type);
}
