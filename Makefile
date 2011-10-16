# Makefile!

OPT ?= -O3
CFLAGS ?= -std=c99 -pedantic -Wall -W $(OPT)
CLINK ?= -lm

OBJ = pkproto.o utils.o

tests: tests.o $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o tests tests.o $(OBJ)
	@./tests && echo Tests passed || echo Tests FAILED.

pkite: pkite.o $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o pkite pkite.o $(OBJ)

all: tests pkite

clean:
	rm -f pkite *.o

.c.o:
	$(CC) $(CFLAGS) -c $<

pkproto.o: pkproto.h
utils.o: utils.h
