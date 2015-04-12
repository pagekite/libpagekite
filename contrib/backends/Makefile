#!/usr/bin/colormake

# Cross-compiling magic happens here (or not)
CC=$(CROSS)gcc
LD=$(CROSS)ld
AR=$(CROSS)ar
PKG_CONFIG=$(CROSS)pkg-config
TARGET_OBJ ?= 

LIB_CFLAGS:= $(CFLAGS) -std=c99 -fno-strict-aliasing -I../../include/ -fpic
LIB_LDFLAGS:= ${LDFLAGS} -L../../lib
LIB_LIBS:= -lpthread -lssl -lcrypt -ldl -lm -lev
LIB_CFLAGS+=$(shell $(PKG_CONFIG) --cflags libev)
# lua distro maintainers are teh suck.
# http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=498416
# http://lua-users.org/lists/lua-l/2008-09/msg00184.html
ifeq ($(shell $(PKG_CONFIG) lua5.1 --exists && echo yes),yes)
  LUA_PKG=lua5.1
else
 ifeq ($(shell $(PKG_CONFIG) lua-5.1 --exists && echo yes),yes)
   LUA_PKG=lua-5.1
 else
  ifeq ($(shell $(PKG_CONFIG) lua --exists && echo yes),yes)
    LUA_PKG=lua
  endif
 endif
endif
LIB_CFLAGS+=$(shell $(PKG_CONFIG) --cflags $(LUA_PKG))
LIB_LIBS+=$(shell $(PKG_CONFIG) --libs $(LUA_PKG))

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
	$(CC) $(LIB_CFLAGS) -o httpkite httpkite.o $(LIB_LDFLAGS) $(LIB_LIBS) -lpagekite

hello: .unix hello.o
	$(CC) $(LIB_CFLAGS) -o hello hello.o $(LIB_LDFLAGS) $(LIB_LIBS) -lpagekite

pagekitec: .unix pagekitec.o
	$(CC) $(LIB_CFLAGS) -o pagekitec pagekitec.o -lpagekite $(LIB_LDFLAGS) $(LIB_LIBS)

pagekitec.exe: .win32 pagekitec.o
	$(CC) $(LIB_CFLAGS) -o pagekitec.exe pagekitec.o $(LIB_LDFLAGS) $(LIB_LIBS) -lpagekite_dll

clean:
	rm -vf tests pagekite[cr] hello httpkite *.exe *.o .win32 .unix

allclean: clean
	find . -name '*.o' |xargs rm -vf

httpkite.o: httpkite.c
	$(CC) $(LIB_CFLAGS) -I../../libpagekite -c $<

.c.o:
	$(CC) $(LIB_CFLAGS) -c $<

httpkite.o: $(HDRS)
pagekite.o: $(HDRS)
pagekitec.o: $(HDRS)
