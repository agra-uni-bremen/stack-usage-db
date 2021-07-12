CFLAGS ?= -O0 -g -Werror
CFLAGS += -std=c99 -D_POSIX_C_SOURCE=200809L
CFLAGS += -Wpedantic -Wall -Wextra \
	      -Wmissing-prototypes -Wpointer-arith -Wconversion \
	      -Wstrict-prototypes -Wshadow -Wformat-nonliteral

CFLAGS += $(shell pkg-config --cflags libdw)
LDLIBS += $(shell pkg-config --libs libdw)

stack-usage-db: stack-usage-db.c
