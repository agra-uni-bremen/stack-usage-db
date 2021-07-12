override CC = riscv32-unknown-elf-gcc

CFLAGS  = -O0 -ggdb -fstack-usage -march=rv32imc -mabi=ilp32
ASFLAGS = $(CFLAGS)
LDFLAGS = -nostartfiles -Wl,--no-relax

all: main
main: main.o myfunc1.o myfunc2.o bootstrap.o
sim: main
	tiny32-vp --intercept-syscalls $<

.PHONY: all sim
