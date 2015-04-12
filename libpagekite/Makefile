#!/usr/bin/colormake

# Cross-compiling magic happens here (or not)
CC=$(CROSS)gcc
LD=$(CROSS)ld
AR=$(CROSS)ar
PKG_CONFIG=$(CROSS)pkg-config
#TARGET_CFLAGS ?= -fpic -I/usr/include/libev
#TARGET_CLINK ?= -lev -llua-5.1
TARGET_OBJ ?= 

LIB_CFLAGS:=${CFLAGS} ${CPPFLAGS} -std=c99 -I../include -fpic
LIB_LDFLAGS:=${LDFLAGS} -L.
LIB_LIBS:= -lm -lpthread -lssl -lcrypto -lev
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


TOBJ = sha1_test.o

OBJ = pkerror.o pkproto.o pkconn.o pkblocker.o pkmanager.o pklua.o \
      pklogging.o pkstate.o utils.o pd_sha1.o pkwatchdog.o pagekite.o \
      $(TARGET_OBJ)
HDRS = common.h utils.h pkstate.h pkconn.h pkerror.h pkproto.h pklogging.h \
       pkmanager.h pklua.h pd_sha1.h pkwatchdog.h Makefile \
       ../include/pagekite.h

ROBJ = pkrelay.o
RHDRS = pkrelay.h

PK_TRACE ?= 0
PK_MEMORY_CANARIES ?= 0
PK_TESTS ?= 1
HAVE_OPENSSL ?= 1
HAVE_IPV6 ?= 1
HAVE_LUA ?= 1

DEFINES=-DHAVE_IPV6=$(HAVE_IPV6) \
        -DHAVE_OPENSSL=$(HAVE_OPENSSL) \
        -DHAVE_LUA=$(HAVE_LUA) \
        -DPK_MEMORY_CANARIES=$(PK_MEMORY_CANARIES) \
        -DPK_TRACE=$(PK_TRACE) \
        -DPK_TESTS=$(PK_TESTS)

NDK_PROJECT_PATH ?= "/home/bre/Projects/android-ndk-r8"

default: libpagekite.so

relay: pagekiter

all: runtests libpagekite.so

runtests: tests
	@./tests && echo Tests passed || echo Tests FAILED.

#android: clean
android:
	@$(NDK_PROJECT_PATH)/ndk-build

windows: .win32 libpagekite.dll

.win32:
	@echo Switching to win32 build mode
	@make clean
	@touch .win32

.unix:
	@echo Switching to unix build mode
	@make clean
	@touch .unix

tests: .unix tests.o $(OBJ) $(TOBJ)
	$(CC) $(LIB_CFLAGS) -o tests tests.o $(OBJ) $(TOBJ) $(LIB_LDFLAGS)

libpagekite.a: .unix $(OBJ)
	$(AR) rcs libpagekite.a $(OBJ)

libpagekite.so: .unix $(OBJ)
	#$(CC) $(LIB_CFLAGS) -shared -o libpagekite.so $(OBJ) $(LIB_LDFLAGS)
	$(CC) -shared $(LIB_LDFLAGS) $(OBJ) -o $@ ${LIB_LIBS}


libpagekite-full: .unix $(OBJ) $(ROBJ)
	$(CC) -shared $(LIB_LDFLAGS) -o libpagekite.so $(OBJ) $(ROBJ) $(LIB_LIBS)

pklualua.h: pklua.lua Makefile ../contrib/plugins
	echo 'static const char* pklualua = (' >pklualua.h
	(for FN in pklua.lua ../contrib/plugins/*.lua; do \
	  grep -v -e '^return ' <$$FN |\
	  sed -e 's/\\/\\\\/g; s/"/\\"/g; s/^/"/; s/$$/\\n"/' ;\
	done) >>pklualua.h
	echo '"return pklua"' >>pklualua.h
	echo ');' >>pklualua.h

pagekiter: pagekiter.o $(OBJ) $(ROBJ)
	$(CC) $(LIB_CFLAGS) -o pagekiter pagekiter.o $(OBJ) $(ROBJ) $(LIB_LDFLAGS) $(LIB_LIBS)

libpagekite.dll: .win32 $(OBJ)
	$(CC) -shared -o libpagekite.dll $(OBJ) $(LIB_LDFLAGS) $(LIB_LIBS) \
              -Wl,--out-implib,libpagekite_dll.a

evwrap.o: mxe/evwrap.c
	$(CC) $(LIB_CFLAGS) -w -c mxe/evwrap.c

pagekite.o: pagekite.c
	$(CC) $(LIB_CFLAGS) $(DEFINES) -DBUILDING_PAGEKITE_DLL=1 -c $<

version: pagekite.h.in
	sed -e "s/@DATE@/`date '+%y%m%d'`/g" <pagekite.h.in >../include/pagekite.h

clean:
	rm -vf tests pagekiter *.[oa] *.so *.exe *.dll .unix .win32

allclean: clean
	find . -name '*.o' |xargs rm -vf

.c.o:
	$(CC) $(LIB_CFLAGS) $(CWARN) $(DEFINES) -c $<

pagekite.o: $(HDRS)
pagekiter.o: $(HDRS) $(RHDRS)
pagekite-jni.o: $(HDRS)
pkblocker.o: $(HDRS)
pklua.o: $(HDRS) pklualua.h
pkconn.o: common.h utils.h pkerror.h pklogging.h
pkerror.o: common.h utils.h pkerror.h pklogging.h
pklogging.o: common.h pkstate.h pkconn.h pkproto.h pklogging.h
pkmanager.o: $(HDRS)
pkproto.o: common.h pd_sha1.h utils.h pkconn.h pkproto.h pklogging.h pkerror.h
pd_sha1.o: common.h pd_sha1.h
sha1_test.o: common.h pd_sha1.h
tests.o: pkstate.h
utils.o: common.h
evwrap.o: mxe/evwrap.h
