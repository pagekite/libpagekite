#!/usr/bin/colormake

# Cross-compiling magic happens here (or not)
CC=$(CROSS)gcc
LD=$(CROSS)ld
AR=$(CROSS)ar
PKG_CONFIG=$(CROSS)pkg-config
TARGET_CFLAGS ?= -fpic -I/usr/include/libev
TARGET_CLINK ?= -lev
TARGET_OBJ ?= 

OPT ?= -g -O3
CFLAGS ?= -std=c99 -fno-strict-aliasing -I../../include/ $(TARGET_CFLAGS) $(OPT)
CWARN ?= -pedantic -Wall -W
CLINK ?= -L../../lib -lpthread -lssl -lcrypto -lm $(TARGET_CLINK)

HDRS = ../../include/pagekite.h

default: pagekitec

all: pagekitec httpkite

windows: pagekitec.exe

.unix:
	@make clean
	@touch .unix

.win32:
	@make clean
	@touch .win32

httpkite: .unix httpkite.o
	$(CC) $(CFLAGS) -o httpkite httpkite.o $(CLINK) -lpagekite

hello: .unix hello.o
	$(CC) $(CFLAGS) -o hello hello.o $(CLINK) -lpagekite

pagekitec: .unix pagekitec.o
	$(CC) $(CFLAGS) -o pagekitec pagekitec.o $(CLINK) -lpagekite

pagekitec.exe: .win32 pagekitec.o
	$(CC) $(CFLAGS) -o pagekitec.exe pagekitec.o $(CLINK) -lpagekite_dll

clean:
	rm -vf tests pagekite[cr] hello httpkite *.exe *.o .win32 .unix

allclean: clean
	find . -name '*.o' |xargs rm -vf

httpkite.o: httpkite.c
	$(CC) $(CFLAGS) -I../../libpagekite $(CWARN) -c $<

.c.o:
	$(CC) $(CFLAGS) $(CWARN) -c $<

httpkite.o: $(HDRS)
pagekite.o: $(HDRS)
pagekitec.o: $(HDRS)
