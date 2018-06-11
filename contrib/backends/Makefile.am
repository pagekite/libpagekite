AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libpagekite

bin_PROGRAMS = httpkite pagekitec sshkite

httpkite_SOURCES = httpkite.c
httpkite_LDADD = $(top_builddir)/libpagekite/libpagekite.la
httpkite_CFLAGS = $(LIBEV_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/libpagekite

sshkite_SOURCES = sshkite.c
sshkite_LDADD = $(top_builddir)/libpagekite/libpagekite.la
sshkite_CFLAGS = $(LIBEV_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/libpagekite

pagekitec_SOURCES = pagekitec.c
pagekitec_CFLAGS = $(LIBEV_CFLAGS) $(LUA_INCLUDE) -I$(top_srcdir)/include -I$(top_srcdir)/libpagekite
pagekitec_LDADD = $(top_builddir)/libpagekite/libpagekite.la
pagekitec_LDFLAGS = $(LUA_LIBS)

if HAVE_JAVA
dist_noinst_JAVA = PageKiteTest.java
endif
