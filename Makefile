#!/usr/bin/colormake

#OPT ?= -O3
OPT ?= -g
CFLAGS ?= -std=c99 -pedantic -Wall -W $(OPT)
CLINK ?= -lpthread -lm -lev

TOBJ = pkproto_test.o pkmanager_test.o sha1_test.o utils_test.o
OBJ = pkerror.o pkproto.o pkblocker.o pkmanager.o \
      pklogging.o utils.o sha1.o

NDK_PROJECT_PATH ?= "/home/bre/Projects/android-ndk-r8"

runtests: tests
	@./tests && echo Tests passed || echo Tests FAILED.

all: runtests pagekite httpkite

android: clean
	@$(NDK_PROJECT_PATH)/ndk-build

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

pkmanager_test.o: pkmanager.o
pkproto_test.o: pkproto.o
pkblocker.o: pkblocker.h pkmanager.h
pkmanager.o: pkmanager.h pkblocker.o pkproto.o utils.o pkerror.o pklogging.o
pkproto.o: pkproto.h utils.o pkerror.o pklogging.o
pklogging.o: pklogging.h pkproto.h includes.h
pkerror.o: pkerror.h includes.h
utils.o: utils.h includes.h
sha1.o: sha1.h includes.h
