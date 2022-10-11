#include "config.h"

/* Just use 64bit time everywhere! */
typedef long long time_t64;

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#ifndef __STDC__
#  define __STDC__ 1
#endif

#ifdef __MINGW32__
#  define _MSC_VER mingw32
#  define _WIN32_WINNT 0x0501
#  undef __STRICT_ANSI__
#elif defined(_MSC_VER)
#  pragma comment(lib, "ws2_32.lib")
#  pragma comment(lib, "libeay32.lib")
#  pragma comment(lib, "ssleay32.lib")
#endif
#ifdef _MSC_VER
#  include <ws2tcpip.h>
#  include <assert.h>
#  include <errno.h>
#  include <signal.h>
#  include <stdarg.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <sys/types.h>
#  include <time.h>
#else
#  include <assert.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <strings.h>
#  include <stdarg.h>
#  include <unistd.h>
#  include <errno.h>
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <signal.h>
#  include <pthread.h>
#  include <time.h>
#  include <sys/time.h>
#  include <ev.h>
#endif

#ifdef HAVE_SYSLOG_H
#  include <syslog.h>
#endif

#ifdef __MINGW32__
#  include <mswsock.h>
#  include <io.h>
#  include <getopt.h>
#  include <pthread.h>
#  include <sys/time.h>
#  include <mxe/evwrap.h>
#  define sleep(n) Sleep(n*1000)
#  define strncasecmp _strnicmp
#  define strcasecmp _stricmp
#  define snprintf _snprintf
#  define strdup _strdup
#  define getpid rand
#  define SHUT_RD 0
#  define SHUT_WR 1
#  define SHUT_RDWR 2
#elif defined(_MSC_VER)
#  include <mswsock.h>
#  include <gettimeofday.h>
#  include <getopt.h>
#  include <evwrap.h>
#  include <pthreads.h>
#  define sleep(n) Sleep(n*1000)
#  define strncasecmp _strnicmp
#  define strcasecmp _stricmp
#  define snprintf _snprintf /* This is probably okay */
#  define strdup _strdup
#  define HAVE_OPENSSL 1 /* The Windows port doesn't work w/o OpenSSL yet */
#  define SHUT_RD 0
#  define SHUT_WR 1
#  define SHUT_RDWR 2
typedef SSIZE_T ssize_t;
#endif
#include <ctype.h>

#ifndef ANDROID
typedef signed char               int8_t;
typedef short int                 int16_t;
typedef int                       int32_t;

typedef unsigned char             uint8_t;
typedef unsigned short int        uint16_t;
typedef unsigned int              uint32_t;
#endif

#if defined(HAVE_IPV6) && (HAVE_IPV6 == 0)
#  undef HAVE_IPV6
#endif

#ifdef _MSC_VER
#  define PKS(s)                _get_osfhandle(s)
#  define PKS_fail(stuff)       (SOCKET_ERROR == stuff)
#  define PKS_socket(f, t, p)   _open_osfhandle(socket(f, t, p), 0)
#  define PKS_connect(s, d, l)  connect(_get_osfhandle(s), d, l)
#  define PKS_read(s, d, l)     recv(_get_osfhandle(s), d, l, 0)
#  define PKS_peek(s, d, l)     recv(_get_osfhandle(s), d, l, MSG_PEEK)
#  define PKS_write(s, d, l)    send(_get_osfhandle(s), d, l, 0)
#  define PKS_close(s)          closesocket(_get_osfhandle(s))
#  define PKS_shutdown(s, how)  shutdown(_get_osfhandle(s), how)
#  define PKS_bind(s, d, l)     bind(_get_osfhandle(s), d, l)
#  define PKS_listen(s, bl)     listen(_get_osfhandle(s), bl)
#  define PKS_accept(s, d, l)   accept(_get_osfhandle(s), d, l)
#  define PKS_setsockopt(f, l, o, v, s)  setsockopt(_get_osfhandle(f), l, o, v, s)
#  define PKS_EV_FD(s)          s
#else
#  define PKS(s)                s
#  define PKS_fail(stuff)       (0 > stuff)
#  define PKS_socket(f, t, p)   socket(f, t, p)
#  define PKS_connect(s, d, l)  connect(s, d, l)
#  define PKS_read(s, d, l)     read(s, d, l)
#  define PKS_peek(s, d, l)     recv(s, d, l, MSG_PEEK)
#  define PKS_write(s, d, l)    write(s, d, l)
#  define PKS_close(s)          close(s)
#  define PKS_shutdown(s, how)  shutdown(s, how)
#  define PKS_bind(s, d, l)     bind(s, d, l)
#  define PKS_listen(s, bl)     listen(s, bl)
#  define PKS_accept(s, d, l)   accept(s, d, l)
#  define PKS_setsockopt(f, l, o, v, s) setsockopt(f, l, o, v, s)
#  define PKS_EV_FD(s)          s
#endif

#if defined(HAVE_OPENSSL) && (HAVE_OPENSSL != 0)
#  include <openssl/ssl.h>
#  include <openssl/bio.h>
#  include <openssl/err.h>
#  ifndef SSL_MODE_RELEASE_BUFFERS
#    define SSL_MODE_RELEASE_BUFFERS 0
#  endif
#  ifndef SSL_OP_NO_COMPRESSION
#    define SSL_OP_NO_COMPRESSION 0
#  endif
#  define PKS_DEFAULT_CIPHERS "HIGH:!aNULL:!eNULL:!LOW:!MD5:!EXP:!PSK:!SRP:!DSS"
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#  define PKS_SSL_INIT(ctx) {\
              SSL_load_error_strings(); \
              ERR_load_BIO_strings(); \
              OpenSSL_add_all_algorithms(); \
              SSL_library_init(); \
              sk_SSL_COMP_zero(SSL_COMP_get_compression_methods()); \
              ctx = SSL_CTX_new(SSLv23_method()); \
              SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION); \
              SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS); }
#else
#  define PKS_SSL_INIT(ctx) {\
              sk_SSL_COMP_zero(SSL_COMP_get_compression_methods()); \
              ctx = SSL_CTX_new(TLS_method()); \
              SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION); \
              SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS); }
#endif
#else
#  define SSL_CTX                   void
#  define PKS_DEFAULT_CIPHERS       NULL
#  define PKS_SSL_INIT(ctx)         { ctx = NULL; }
#  define SSL_ERROR_NONE            0
#  undef HAVE_OPENSSL
#endif

typedef struct {
  int count;
  pthread_t owner;
  pthread_mutex_t check_lock;
  pthread_mutex_t lock_lock;
} pk_rlock_t;

#define PARSER_BYTES_MIN   1 * 1024
#define PARSER_BYTES_AVG   4 * 1024
#define PARSER_BYTES_MAX  16 * 1024  /* <= CONN_IO_BUFFER_SIZE */

#ifndef HAVE_RELAY
#define HAVE_RELAY 0
#endif
#ifndef PK_MEMORY_CANARIES
#define PK_MEMORY_CANARIES 0
#endif
#ifndef PK_TRACE
#define PK_TRACE 0
#endif
#ifndef PK_TESTS
#define PK_TESTS 0
#endif
