
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <elf.h>
#include <link.h>

#include "shared.h"


struct elf_obj {
  ElfW(Sym) *dt_symtab;
  char *dt_strtab;
  uint32_t *dt_hash;
};

static void *get_biased_dynamic_entry(struct dynnacl_obj *obj, int type) {
  uintptr_t val = elf_get_dynamic_entry(obj, type);
  assert(val != 0);
  return (void *) (elf_get_load_bias(obj) + val);
}

void dump_symbols(struct elf_obj *obj) {
  uint32_t nchain = obj->dt_hash[1];
  uint32_t index;
  for (index = 0; index < nchain; index++) {
    ElfW(Sym) *sym = &obj->dt_symtab[index];
    printf("symbol %i: %s\n", index, obj->dt_strtab + sym->st_name);
  }
}

int main() {
  struct dynnacl_obj *dynnacl_obj =
    dynnacl_load_from_elf_file("example_lib_elf.so");

  struct elf_obj elf_obj;

  elf_obj.dt_symtab = get_biased_dynamic_entry(dynnacl_obj, DT_SYMTAB);
  elf_obj.dt_strtab = get_biased_dynamic_entry(dynnacl_obj, DT_STRTAB);
  elf_obj.dt_hash = get_biased_dynamic_entry(dynnacl_obj, DT_HASH);

  dump_symbols(&elf_obj);

  return 0;
}
