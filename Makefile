#!/usr/bin/colormake

#OPT ?= -O3
OPT ?= -g
CFLAGS ?= -std=c99 -pedantic -Wall -W $(OPT)
CLINK ?= -lm -lev

TOBJ = pkproto_test.o pkmanager_test.o sha1_test.o utils_test.o
OBJ = pkerror.o pkproto.o pkmanager.o pklogging.o utils.o sha1.o

runtests: tests
	@./tests && echo Tests passed || echo Tests FAILED.

all: runtests pagekite httpkite

tests: tests.o $(OBJ) $(TOBJ)
	$(CC) $(CFLAGS) $(CLINK) -o tests tests.o $(OBJ) $(TOBJ)

httpkite: runtests httpkite.o $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o httpkite httpkite.o $(OBJ)

pagekite: runtests pagekite.o $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o pagekite pagekite.o $(OBJ)

clean:
	rm -f tests pagekite httpkite *.o

.c.o:
	$(CC) $(CFLAGS) -c $<

pkmanager_test.o: pkmanager.h pkproto.h utils.h pkerror.h pklogging.h
pkmanager.o: pkmanager.h pkproto.h utils.h pkerror.h pklogging.h
pkproto_test.o: pkproto.h utils.h pkerror.h pklogging.h
pkproto.o: pkproto.h utils.h pkerror.h pklogging.h
pklogging.o: pkproto.h pklogging.h
utils.o: utils.h
sha1.o: sha1.h types.h
