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
#  include <ev.h>
#endif

#ifdef __MINGW32__
#  include <mswsock.h>
#  include <io.h>
#  include <getopt.h>
#  include <pthread.h>
#  include <sys/time.h>
#  define sleep(n) Sleep(n*1000)
#  define strncasecmp _strnicmp
#  define strcasecmp _stricmp
#  define snprintf _snprintf
#  define strdup _strdup
#  define getpid rand
#  define SHUT_RD 0
#  define SHUT_WR 1
#  define SHUT_RDWR 2
#  define EWOULDBLOCK WSAEWOULDBLOCK
#  define EINPROGRESS WSAEINPROGRESS
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
