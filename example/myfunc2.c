#include <stdint.h>

static volatile uint64_t *MTIME_REG = (uint64_t *)0x200bff8;

static __attribute__((noinline)) uint64_t
get_mtime(void) {
	return *MTIME_REG;
}

int myfunc2(uint64_t a) {
	int b = get_mtime();
	b *= 2;

	return a + b;
}
