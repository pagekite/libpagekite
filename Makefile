#!/usr/bin/colormake

#OPT ?= -O3
OPT ?= -g
CFLAGS ?= -std=c99 -pedantic -Wall -W $(OPT)
CLINK ?= -lpthread -lssl -lm -lev

TOBJ = pkproto_test.o pkmanager_test.o sha1_test.o utils_test.o
OBJ = pkerror.o pkproto.o pkconn.o pkblocker.o pkmanager.o \
      pklogging.o utils.o sha1.o
HDRS = common.h utils.h pkstate.h pkconn.h pkerror.h pkproto.h pklogging.h \
       pkmanager.h sha1.h

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

httpkite.o: $(HDRS)
pagekite.o: $(HDRS)
pagekite-jni.o: $(HDRS)
pkblocker.o: $(HDRS)
pkconn.o: common.h utils.h pkerror.h pklogging.h
pkerror.o: common.h utils.h pkerror.h pklogging.h
pklogging.o: common.h pkstate.h pkconn.h pkproto.h pklogging.h
pkmanager.o: $(HDRS)
pkmanager_test.o: $(HDRS)
pkproto.o: common.h sha1.h utils.h pkconn.h pkproto.h pklogging.h pkerror.h
pkproto_test.o: common.h pkerror.h pkconn.h pkproto.h utils.h
sha1.o: common.h sha1.h
sha1_test.o: common.h sha1.h
tests.o: pkstate.h
utils.o: common.h
utils_test.o: utils.h
