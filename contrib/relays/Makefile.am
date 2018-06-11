AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libpagekite

bin_PROGRAMS = pagekiter

pagekiter_SOURCES = pagekiter.c
pagekiter_CFLAGS = $(LIBEV_CFLAGS) $(LUA_INCLUDE) -I$(top_srcdir)/include -I$(top_srcdir)/libpagekite
pagekiter_LDADD = $(top_builddir)/libpagekite/libpagekite.la
pagekiter_LDFLAGS = $(LUA_LIBS)
