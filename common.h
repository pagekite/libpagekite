#define _GNU_SOURCE 1
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <ev.h>

#define HAVE_OPENSSL 1

#ifndef ANDROID
typedef signed char               int8_t;
typedef short int                 int16_t;
typedef int                       int32_t;

typedef unsigned char             uint8_t;
typedef unsigned short int        uint16_t;
typedef unsigned int              uint32_t;
#endif

#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#define INIT_PAGEKITE_SSL(ctx)    SSL_load_error_strings(); \
                                  ERR_load_BIO_strings(); \
                                  OpenSSL_add_all_algorithms(); \
                                  SSL_library_init(); \
                                  ctx = SSL_CTX_new(TLSv1_method());
#else
#define SSL_CTX                   void
#define INIT_PAGEKITE_SSL(ctx)    ctx = NULL
#endif

#define PARSER_BYTES_MIN   1 * 1024
#define PARSER_BYTES_AVG   2 * 1024
#define PARSER_BYTES_MAX   4 * 1024  /* <= CONN_IO_BUFFER_SIZE */

