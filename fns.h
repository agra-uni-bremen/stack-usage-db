#ifndef STACK_USAGE_DB_FNS
#define STACK_USAGE_DB_FNS

#include <stdint.h>

#include <elfutils/libdwfl.h>

char *dwarf_get_src(const char *, GElf_Addr);

#endif
