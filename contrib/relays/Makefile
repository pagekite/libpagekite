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
CLINK ?= -L../../lib -lpthread -lssl -lcrypto \
         -llua5.1 -ldl -lm -lc $(TARGET_CLINK)

HDRS = ../../include/pagekite.h

default: pagekiter

all: pagekiter

windows: pagekiter.exe

.unix:
	@make clean
	@touch .unix

.win32:
	@make clean
	@touch .win32

pagekiter: .unix pagekiter.o
	$(CC) $(CFLAGS) -o pagekiter pagekiter.o -lpagekite-agpl $(CLINK)

pagekiter.exe: .win32 pagekiter.o
	$(CC) $(CFLAGS) -o pagekiter.exe pagekiter.o $(CLINK) -lpagekite_dll

clean:
	rm -vf tests pagekiter *.exe *.o .win32 .unix

allclean: clean
	find . -name '*.o' |xargs rm -vf

.c.o:
	$(CC) $(CFLAGS) $(CWARN) -c $<

pagekiter.o: $(HDRS)
