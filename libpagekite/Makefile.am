lib_LTLIBRARIES = libpagekite.la
bin_PROGRAMS = tests

libpagekite_la_SOURCES = \
        pkerror.c pkproto.c pkconn.c pkblocker.c pkmanager.c pklogging.c \
        pkstate.c pkutils.c pd_sha1.c pkwatchdog.c pkhooks.c \
        pagekite.c
libpagekite_la_CPPFLAGS = -I$(top_srcdir)/include -std=c99 -fno-strict-aliasing
libpagekite_la_CFLAGS = $(LIBEV_CFLAGS)
libpagekite_la_LDFLAGS = -lpthread $(OPENSSL_LIBS) $(LIBEV_LIBS) \
        -lm -no-undefined -version-info 1:0:0

tests_LDADD = libpagekite.la
tests_CFLAGS = $(LIBEV_CFLAGS) -I$(top_srcdir)/include

if HAVE_JAVA
libpagekite_la_SOURCES += pagekite-jni.c
endif

if HAVE_LUA
libpagekite_la_SOURCES += pklua.c
libpagekite_la_CFLAGS += $(LUA_INCLUDES)
libpagekite_la_LDFLAGS += $(LUA_LIBS)
endif

if HAVE_RELAY
libpagekite_la_SOURCES += pkrelay.c
endif
