# Makefile!

OPT ?= -O3
CFLAGS ?= -std=c99 -pedantic -Wall -W $(OPT)
CLINK ?= -lm

OBJ = pkproto.o utils.o

runtests: tests
	@./tests && echo Tests passed || echo Tests FAILED.

tests: tests.o $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o tests tests.o $(OBJ)

pagekite: pagekite.o $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o pagekite pagekite.o $(OBJ)

all: tests pagekite

clean:
	rm -f tests pagekite *.o

.c.o:
	$(CC) $(CFLAGS) -c $<

pkproto.o: pkproto.h
utils.o: utils.h
