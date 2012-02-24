#!/usr/bin/colormake

OPT ?= -O3
CFLAGS ?= -std=c99 -pedantic -Wall -W $(OPT)
CLINK ?= -lm

TOBJ = pkproto_test.o sha1_test.o
OBJ = pkproto.o pkmanager.o utils.o sha1.o

runtests: tests
	@./tests && echo Tests passed || echo Tests FAILED.

all: runtests pagekite

tests: tests.o $(OBJ) $(TOBJ)
	$(CC) $(CFLAGS) $(CLINK) -o tests tests.o $(OBJ) $(TOBJ)

httpkite: httpkite.o $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o httpkite httpkite.o $(OBJ)

pagekite: pagekite.o $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o pagekite pagekite.o $(OBJ)

clean:
	rm -f tests pagekite *.o

.c.o:
	$(CC) $(CFLAGS) -c $<

pkmanager.o: pkmanager.h pkproto.h utils.h
pkproto.o: pkproto.h utils.h
utils.o: utils.h
sha1.o: sha1.h types.h
