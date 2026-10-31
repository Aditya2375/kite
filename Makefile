CC      ?= cc
CFLAGS  ?= -O2 -g
# required flags survive a user/environment CFLAGS; extras go in EXTRA
override CFLAGS += -std=gnu11 -Wall -Wextra -Werror -Isrc $(EXTRA)
BIN     := build/kite
SRCS    := src/main.c src/lex.c src/parse.c src/sema.c src/gen.c

all: $(BIN)

$(BIN): $(SRCS) src/kite.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $@

check: $(BIN)
	./tests/run-tests.sh

clean:
	rm -rf build

.PHONY: all check clean
