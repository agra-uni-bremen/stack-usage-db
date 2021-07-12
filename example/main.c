#include <stdint.h>

extern int myfunc1(void);

int main(void) {
	volatile int r = myfunc1();
	r * 2;

	return r;
}
