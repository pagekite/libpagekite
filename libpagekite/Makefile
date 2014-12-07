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
CFLAGS ?= -std=c99 -fno-strict-aliasing -I../include/ $(TARGET_CFLAGS) $(OPT)
CWARN ?= -pedantic -Wall -W
CLINK ?= -L. -lpthread -lssl -lcrypto -lm $(TARGET_CLINK)

TOBJ = pkproto_test.o sha1_test.o utils_test.o

OBJ = pkerror.o pkproto.o pkconn.o pkblocker.o pkmanager.o \
      pklogging.o pkstate.o utils.o pd_sha1.o pkwatchdog.o pagekite.o \
      $(TARGET_OBJ)
HDRS = common.h utils.h pkstate.h pkconn.h pkerror.h pkproto.h pklogging.h \
       pkmanager.h pd_sha1.h pkwatchdog.h Makefile

ROBJ = pkrelay.o
RHDRS = pkrelay.h

PK_TRACE ?= 0
PK_MEMORY_CANARIES ?= 0
HAVE_OPENSSL ?= 1
HAVE_IPV6 ?= 1

DEFINES=-DHAVE_IPV6=$(HAVE_IPV6) \
        -DHAVE_OPENSSL=$(HAVE_OPENSSL) \
        -DPK_MEMORY_CANARIES=$(PK_MEMORY_CANARIES) \
        -DPK_TRACE=$(PK_TRACE)

NDK_PROJECT_PATH ?= "/home/bre/Projects/android-ndk-r8"

default: libpagekite.so pagekitec

relay: pagekiter

all: runtests libpagekite.so pagekitec pagekiter httpkite

runtests: tests
	@./tests && echo Tests passed || echo Tests FAILED.

#android: clean
android:
	@$(NDK_PROJECT_PATH)/ndk-build

tests: tests.o $(OBJ) $(TOBJ)
	$(CC) $(CFLAGS) -o tests tests.o $(OBJ) $(TOBJ) $(CLINK)

libpagekite.so: $(OBJ)
	$(CC) $(CFLAGS) -shared -o libpagekite.so $(OBJ) $(CLINK)

libpagekite-full: $(OBJ) $(ROBJ)
	$(CC) $(CFLAGS) -shared -o libpagekite.so $(OBJ) $(ROBJ) $(CLINK)

httpkite: httpkite.o $(OBJ)
	$(CC) $(CFLAGS) -o httpkite httpkite.o $(OBJ) $(CLINK)

pagekitec: pagekitec.o libpagekite.so
	$(CC) $(CFLAGS) -o pagekitec pagekitec.o $(CLINK) -lpagekite

pagekiter: pagekiter.o $(OBJ) $(ROBJ)
	$(CC) $(CFLAGS) -o pagekiter pagekiter.o $(OBJ) $(ROBJ) $(CLINK)

pagekitec.exe: pagekitec.o libpagekite.dll
	$(CC) $(CFLAGS) -o pagekitec.exe pagekitec.o $(CLINK) -lpagekite_dll

libpagekite.dll: $(OBJ)
	$(CC) -shared -o libpagekite.dll $(OBJ) $(CLINK) \
              -Wl,--out-implib,libpagekite_dll.a

evwrap.o: mxe/evwrap.c
	$(CC) $(CFLAGS) -w -c mxe/evwrap.c

pagekite.o: pagekite.c
	$(CC) $(CFLAGS) $(CWARN) $(DEFINES) -DBUILDING_PAGEKITE_DLL=1 -c $<

version:
	@sed -e "s/@DATE@/`date '+%y%m%d'`/g" <version.h.in >version.h
	@touch pkproto.h

clean:
	rm -vf tests pagekite[cr] httpkite *.[oa] *.so *.exe *.dll

allclean: clean
	find . -name '*.o' |xargs rm -vf

.c.o:
	$(CC) $(CFLAGS) $(CWARN) $(DEFINES) -c $<

httpkite.o: $(HDRS)
pagekite.o: $(HDRS)
pagekitec.o: $(HDRS)
pagekiter.o: $(HDRS) $(RHDRS)
pagekite-jni.o: $(HDRS)
pkblocker.o: $(HDRS)
pkconn.o: common.h utils.h pkerror.h pklogging.h
pkerror.o: common.h utils.h pkerror.h pklogging.h
pklogging.o: common.h pkstate.h pkconn.h pkproto.h pklogging.h
pkmanager.o: $(HDRS)
pkproto.o: common.h pd_sha1.h utils.h pkconn.h pkproto.h pklogging.h pkerror.h
pkproto_test.o: common.h pkerror.h pkconn.h pkproto.h utils.h
pd_sha1.o: common.h pd_sha1.h
sha1_test.o: common.h pd_sha1.h
tests.o: pkstate.h
utils.o: common.h
utils_test.o: utils.h
evwrap.o: mxe/evwrap.h
