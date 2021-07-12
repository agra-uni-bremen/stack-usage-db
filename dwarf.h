#ifndef STACK_USAGE_DB_DWARF
#define STACK_USAGE_DB_DWARF

#include <elfutils/libdwfl.h>

char *dwarf_get_src(const char *, GElf_Addr);

#endif
