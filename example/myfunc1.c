#include <stdint.h>

extern int myfunc2(uint64_t);

static volatile uint64_t *MTIME_REG = (uint64_t *)0x200bff8;

static __attribute__((noinline)) uint64_t
get_mtime(void) {
	volatile uint32_t n = 5;
	return n * *MTIME_REG;
}

int myfunc1(void) {
	uint64_t a = get_mtime();
	return myfunc2(a);
}
