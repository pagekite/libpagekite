ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = autogen.sh

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libpagekite/libpagekite.pc

pkginclude_HEADERS = include/pagekite.h

SUBDIRS = libpagekite

if HAVE_JAVA
SUBDIRS += bindings/java
endif

if HAVE_RELAY
SUBDIRS += contrib/relays
endif

SUBDIRS += contrib/backends
