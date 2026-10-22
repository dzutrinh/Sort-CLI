CC=cc
CFLAGS=-O2 -Wall -Wextra -std=c99
LFLAGS=-lm

ifeq ($(OS),Windows_NT)
	CC=gcc
	BIN=sort-cli.exe
	TEST_BIN=sort-cli-test.exe
	TEST_RUN=$(TEST_BIN)
	RM=del
	LFLAGS+=-s
else
	BIN=sort-cli
	TEST_BIN=sort-cli-test
	TEST_RUN=./$(TEST_BIN)
	RM=rm -f
endif

HEADERS=sort-cli.h term.h text.h gradient.h data.h trace.h algs.h view.h ui.h pet.h cmds.h

all: $(BIN)

$(BIN): sort-cli.c $(HEADERS)
	$(CC) $(CFLAGS) $< -o $(BIN) $(LFLAGS)

$(TEST_BIN): test.c $(HEADERS)
	$(CC) $(CFLAGS) $< -o $(TEST_BIN) $(LFLAGS)

test: $(TEST_BIN)
	$(TEST_RUN)

clean:
	$(RM) $(BIN) $(TEST_BIN)

.PHONY: all test clean
