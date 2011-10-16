# Makefile!

OPT ?= -O3
CFLAGS ?= -std=c99 -pedantic -Wall -W $(OPT)
CLINK ?= -lm

OBJ = pkproto.o utils.o

tests: tests.o $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o tests tests.o $(OBJ)
	@./tests && echo Tests passed || echo Tests FAILED.

pkite: pagekite.o $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o pagekite pagekite.o $(OBJ)

all: tests pagekite

clean:
	rm -f tests pagekite *.o

.c.o:
	$(CC) $(CFLAGS) -c $<

pkproto.o: pkproto.h
utils.o: utils.h
