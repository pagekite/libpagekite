AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libpagekite

bin_PROGRAMS = httpkite pagekitec

httpkite_SOURCES = httpkite.c
httpkite_LDADD = $(top_srcdir)/libpagekite/libpagekite.la
httpkite_CFLAGS = $(LIBEV_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/libpagekite

pagekitec_SOURCES = pagekitec.c
pagekitec_CFLAGS = $(LIBEV_CFLAGS) $(LUA_INCLUDE) -I$(top_srcdir)/include -I$(top_srcdir)/libpagekite
pagekitec_LDADD = $(top_srcdir)/libpagekite/libpagekite.la
pagekitec_LDFLAGS = $(LUA_LIBS)
