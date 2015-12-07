ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = autogen.sh

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libpagekite/libpagekite.pc

pkginclude_HEADERS = include/pagekite.h

SUBDIRS = libpagekite contrib/backends

if HAVE_RELAY
SUBDIRS += contrib/relays
endif
