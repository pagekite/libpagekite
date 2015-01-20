#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])

# libpagekite is Copyright 2011-2014, The Beanstalks Project ehf.

AC_INIT([libpagekite], [0.90.150224], [info@pagekite.net])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([libpagekite/pkconn.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_HOST


use_openssl="yes"
use_libev="yes"

specialssldir=""
AC_ARG_WITH(openssl,[  --with-openssl=DIR    Use optional openssl libs and includes from [DIR]/lib/ and [DIR]/include/openssl/],
[  case "$with_openssl" in
  yes)
  ;;
  no)
    use_openssl="no"
  ;;
  *)
    specialssldir="$with_openssl"
    LDFLAGS="$LDFLAGS -L$with_openssl/lib"
    CPPFLAGS="-I$with_openssl/include $CPPFLAGS"
  ;;
  esac]
)

AC_ARG_WITH(lua,
	[AS_HELP_STRING([--without-lua],
		[Do not compile pagekite lua bindings even if Lua is available])],
		[], [with_lua=yes])


# Checks for programs.
AC_PROG_CC
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_LIBTOOL
#AC_PROG_INSTALL
AM_PROG_CC_C_O

LT_INIT

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h inttypes.h limits.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h strings.h sys/socket.h sys/statfs.h sys/time.h sys/timeb.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
m4_ifdef([AC_CHECK_HEADER_STDBOOL], [AC_CHECK_HEADER_STDBOOL])
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime dup2 gethostbyname gettimeofday inet_ntoa memmove memset select socket strcasecmp strchr strdup strerror strncasecmp strrchr uname])


# Checks for libraries.
# FIXME: Replace `main' with a function in `-lpthread':
AC_CHECK_LIB([pthread], [main])


# OpenSSL requires dlopen on some platforms
AC_SEARCH_LIBS(dlopen, dl)

# If they didn't specify it, we try to find it
if test "$use_openssl" = "yes" -a -z "$specialssldir" ; then
	AC_CHECK_HEADER(openssl/ssl.h,,
			[ use_openssl="no"
        AC_MSG_WARN([Failed to find openssl/ssl.h so OpenSSL will not be used. 
	If it is installed you can try the --with-openssl=DIR argument]) ])

	if test "$use_openssl" = "yes"; then
	     AC_CHECK_HEADER(openssl/err.h,,
	         [ use_openssl="no"
		     AC_MSG_WARN([Failed to find openssl/err.h so OpenSSL will not be used.
		      If it is installed you can try the --with-openssl=DIR argument]) ])
	fi

	if test "$use_openssl" = "yes"; then
		   AC_CHECK_HEADER(openssl/rand.h,,
		       [ use_openssl="no"
		         AC_MSG_WARN([Failed to find openssl/rand.h so OpenSSL will not be used.
			  If it is installed you can try the --with-openssl=DIR argument]) ])
	fi

	if test "$use_openssl" = "yes"; then
	 AC_CHECK_LIB(crypto, BIO_int_ctrl,
	   [],
	   [ use_openssl="no"
	   AC_MSG_WARN([Failed to find libcrypto so OpenSSL will not be used.
	    If it is installed you can try the --with-openssl=DIR argument]) ])
	fi
										     
	if test "$use_openssl" = "yes"; then
	   AC_CHECK_LIB(ssl, SSL_new,
	   [],
	   [ use_openssl="no"
	   AC_MSG_WARN([Failed to find libssl so OpenSSL will not be used. 
If it is installed you can try the --with-openssl=DIR argument]) ])
	fi
fi

OPENSSL_CFLAGS=""
OPENSSL_LIBS=""
if test "$use_openssl" = "yes"; then
  AC_DEFINE([HAVE_OPENSSL], [1], [Define to 1 if you have OpenSSL.])
  OPENSSL_LIBS="-lssl -lcrypto"
  # Define in Makefile also.
  HAVE_OPENSSL=yes
  AC_SUBST(HAVE_OPENSSL)
fi
AC_SUBST([OPENSSL_CFLAGS])
AC_SUBST([OPENSSL_LIBS])



LIBEV_CFLAGS=""
LIBEV_LIBS=""
if test "x${use_libev}" != "xno"; then
  AC_MSG_CHECKING([Enabled libev.])
  # libev
  AC_MSG_CHECKING([for libev support])
  AC_ARG_WITH([libev],
    [AS_HELP_STRING([--with-libev@<:@=PATH@:>@],[Search for libev in PATH/include and PATH/lib])],
    [WITH_LIBEV=$withval],[WITH_LIBEV=yes])
  PKG_CHECK_MODULES([LIBEV], [libev], [], [
    # no pkg-config for libev, searching manually:
    if test "$WITH_LIBEV" != "yes"; then
      LIBEV_CFLAGS="-I$WITH_LIBEV/include"
      LIBEV_LIBS="-L$WITH_LIBEV/lib -lev"
    else
      AC_CHECK_HEADERS([ev.h],[
        AC_CHECK_LIB([ev], [ev_time], [
          LIBEV_LIBS="-lev"
          ],[
          AC_MSG_ERROR([libev not found])
          ]
        )],[
          AC_MSG_ERROR([libev not found])
        ]
        )
    fi
  ])
fi
AC_SUBST([LIBEV_CFLAGS])
AC_SUBST([LIBEV_LIBS])


# Lua support

AS_IF([test "x$with_lua" != 'xno'],
	[ AX_PROG_LUA([5.1], [], [lua_interp='yes'], [lua_interp='no'])

    AS_IF([test "x$lua_interp" = 'xyes'],
      [ AX_LUA_HEADERS([lua_headers='yes'], [lua_headers='no'])
        AX_LUA_LIBS([lua_libs='yes'], [lua_libs='no'])
      ])

    AS_IF([test "x$lua_interp" = 'xyes' &&
           test "x$lua_headers" = 'xyes' &&
           test "x$lua_libs" = 'xyes'],
      [with_lua='yes'],
      [with_lua='no'])
  ])
AM_CONDITIONAL(HAVE_LUA, test "x$with_lua" = "xyes")
AC_SUBST([LUA_INCLUDE])
AC_SUBST([LUA_LIBS])


AC_CONFIG_FILES([Makefile
  libpagekite/Makefile
  libpagekite/libpagekite.pc
  contrib/backends/Makefile
])
AC_OUTPUT
