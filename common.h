#define _GNU_SOURCE 1

#ifndef _MSC_VER
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libeay32.lib")
#pragma comment(lib, "ssleay32.lib")
#include <ws2tcpip.h>
#endif

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
/*
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
*/

#ifndef __STDC__
#define __STDC__ 1
#endif

#ifdef _MSC_VER
typedef intptr_t ssize_t;
#include <gettimeofday.h>
#include <getopt.h>
#include <evwrap.h>
#include <pthreads.h>
#define sleep Sleep
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define snprintf _snprintf // might cause problems?
#define strdup _strdup
#else
#include <ev.h>
#endif

#ifndef ANDROID
typedef signed char               int8_t;
typedef short int                 int16_t;
typedef int                       int32_t;

typedef unsigned char             uint8_t;
typedef unsigned short int        uint16_t;
typedef unsigned int              uint32_t;
#endif

#if defined(HAVE_IPV6) && (HAVE_IPV6 == 0)
#undef HAVE_IPV6
#endif

/* FIXME: "probably" not the right way to define HAVE_OPENSSL*/
#define HAVE_OPENSSL 1

#if defined(HAVE_OPENSSL) && (HAVE_OPENSSL != 0)
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#ifndef SSL_MODE_RELEASE_BUFFERS
#define SSL_MODE_RELEASE_BUFFERS 0
#endif
#define PKS_SSL_INIT(ctx)       { SSL_load_error_strings(); \
                                  ERR_load_BIO_strings(); \
                                  OpenSSL_add_all_algorithms(); \
                                  SSL_library_init(); \
                         sk_SSL_COMP_zero(SSL_COMP_get_compression_methods()); \
                         ctx = SSL_CTX_new(TLSv1_method()); \
                         SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS); }
#else
#define SSL_CTX                   void
#define PKS_SSL_INIT(ctx)         { ctx = NULL; }
#define SSL_ERROR_NONE            0
#undef HAVE_OPENSSL
#endif

#define PARSER_BYTES_MIN   1 * 1024
#define PARSER_BYTES_AVG   2 * 1024
#define PARSER_BYTES_MAX   4 * 1024  /* <= CONN_IO_BUFFER_SIZE */

