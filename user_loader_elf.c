
#include <stdio.h>
#include <string.h>

#include <elf.h>
#include <link.h>

#include "minimal_libc.h"
#include "shared.h"


struct elf_obj {
  struct dynnacl_obj *dynnacl_obj;
  ElfW(Sym) *dt_symtab;
  char *dt_strtab;
  uint32_t *dt_hash;
};

static unsigned long elf_hash(const uint8_t *name) {
  unsigned long h = 0;
  unsigned long g;
  while (*name != 0) {
    h = (h << 4) + *name++;
    g = h & 0xf0000000;
    if (g != 0)
      h ^= g >> 24;
    h &= ~g;
  }
  return h;
}

static void *get_biased_dynamic_entry(struct dynnacl_obj *obj, int type) {
  uintptr_t val = elf_get_dynamic_entry(obj, type);
  assert(val != 0);
  return (void *) (elf_get_load_bias(obj) + val);
}

static void dump_symbols(struct elf_obj *obj) {
  uint32_t nchain = obj->dt_hash[1];
  uint32_t index;
  for (index = 0; index < nchain; index++) {
    ElfW(Sym) *sym = &obj->dt_symtab[index];
    printf("symbol %i: %s\n", index, obj->dt_strtab + sym->st_name);
  }
}

static ElfW(Sym) *look_up_symbol(struct elf_obj *obj, const char *name) {
  uint32_t nbucket = obj->dt_hash[0];
  uint32_t nchain = obj->dt_hash[1];
  uint32_t *bucket = obj->dt_hash + 2;
  uint32_t *chain = bucket + nbucket;
  unsigned long entry = bucket[elf_hash((uint8_t *) name) % nbucket];
  while (entry != 0) {
    assert(entry < nchain);
    ElfW(Sym) *sym = &obj->dt_symtab[entry];
    char *name2 = obj->dt_strtab + sym->st_name;
    if (strcmp(name, name2) == 0) {
      return sym;
    }
    entry = chain[entry];
  }
  return NULL;
}

static void *look_up_func(struct elf_obj *obj, const char *name) {
  ElfW(Sym) *sym = look_up_symbol(obj, name);
  if (sym == NULL)
    return NULL;
  return (void *) (elf_get_load_bias(obj->dynnacl_obj) + sym->st_value);
}

const char *example_import0() {
  return "called imported func #0";
}

const char *example_import1() {
  return "called imported func #1";
}

static int resolver_call_count = 0;

static void *my_plt_resolver(void *handle, int import_id) {
  struct elf_obj *elf_obj = handle;

  int sym_number = elf_symbol_id_from_import_id(elf_obj->dynnacl_obj,
                                                import_id);
  char *name = elf_obj->dt_strtab + elf_obj->dt_symtab[sym_number].st_name;

  printf("elf resolver called for func #%i: \"%s\"\n", import_id, name);
  resolver_call_count++;

  void *value;
  if (strcmp(name, "import_func0") == 0) {
    value = example_import0;
  } else if (strcmp(name, "import_func1") == 0) {
    value = example_import1;
  } else {
    assert(0);
  }
  elf_set_plt_entry(elf_obj->dynnacl_obj, import_id, value);
  return value;
}

int main() {
  struct dynnacl_obj *dynnacl_obj =
    dynnacl_load_from_elf_file("example_lib_elf.so");

  struct elf_obj elf_obj;

  elf_obj.dynnacl_obj = dynnacl_obj;
  elf_obj.dt_symtab = get_biased_dynamic_entry(dynnacl_obj, DT_SYMTAB);
  elf_obj.dt_strtab = get_biased_dynamic_entry(dynnacl_obj, DT_STRTAB);
  elf_obj.dt_hash = get_biased_dynamic_entry(dynnacl_obj, DT_HASH);

  dump_symbols(&elf_obj);

  printf("testing exported functions...\n");
  const char *(*func)(void);
  const char *result;

  func = (const char *(*)(void)) look_up_func(&elf_obj, "foo");
  assert(func != NULL);
  result = func();
  assert(strcmp(result, "example string") == 0);

  func = (const char *(*)(void)) look_up_func(&elf_obj, "bar");
  assert(func != NULL);
  result = func();
  assert(strcmp(result, "another example string") == 0);

  void *sym = look_up_func(&elf_obj, "some_undefined_sym");
  assert(sym == NULL);

  printf("testing imported functions...\n");
  elf_set_plt_resolver(dynnacl_obj, my_plt_resolver, &elf_obj);

  func = (const char *(*)(void)) look_up_func(&elf_obj, "test_import0");
  assert(func != NULL);
  result = func();
  assert(strcmp(result, "called imported func #0") == 0);
  assert(resolver_call_count == 1);
  resolver_call_count = 0;
  result = func();
  assert(strcmp(result, "called imported func #0") == 0);
  assert(resolver_call_count == 0);

  func = (const char *(*)(void)) look_up_func(&elf_obj, "test_import1");
  assert(func != NULL);
  result = func();
  assert(strcmp(result, "called imported func #1") == 0);
  assert(resolver_call_count == 1);
  resolver_call_count = 0;
  result = func();
  assert(strcmp(result, "called imported func #1") == 0);
  assert(resolver_call_count == 0);

  return 0;
}
