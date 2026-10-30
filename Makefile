CC      ?= cc
CFLAGS  ?= -std=gnu11 -O2 -g -Wall -Wextra -Werror -Isrc
BIN     := build/kite
SRCS    := src/main.c src/lex.c src/parse.c src/gen.c

all: $(BIN)

$(BIN): $(SRCS) src/kite.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $@

check: $(BIN)
	./tests/run-tests.sh

clean:
	rm -rf build

.PHONY: all check clean
