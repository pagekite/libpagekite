/******************************************************************************
pkutils.c - Utility functions for pagekite.

This file is Copyright 2011-2021, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms  of the  Apache  License 2.0  as published by the  Apache  Software
Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the Apache License for more details.

You should have received a copy of the Apache License along with this program.
If not, see: <http://www.apache.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/
#define PAGEKITE_CONSTANTS_ONLY
#include "pagekite.h"
#include "pkcommon.h"

#include "assert.h"
#include "pkutils.h"
#include <fcntl.h>
#include <ctype.h>

#ifndef _MSC_VER
#include <poll.h>
#define HAVE_POLL
#endif

char random_junk[32] = {
  0x95, 0xe9, 0x2a, 0x96, 0x97, 0x73, 0x26, 0xaf,
  0xfc, 0x53, 0xb5, 0x5f, 0x34, 0x57, 0x09, 0xa8,
  0x7a, 0x18, 0x70, 0xe3, 0xe4, 0xe2, 0xae, 0x83,
  0x04, 0x7d, 0xf5, 0xd5, 0xc0, 0x61, 0x0d, 0x00};

/* The CLOCK_MONOTONIC clock_id may be defined but not actually
 * supported. This flag will be cleared to zero if we find out at
 * runtime that CLOCK_MONOTONIC is not actually usable.
 *
 * Note we also need pthread_condattr_setclock atm.
 */
#if defined(HAVE_CLOCK_MONOTONIC) && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK)
static int use_clock_gettime = 1;
#endif

void better_srand(int allow_updates)
{
  static int allow_srand = 0;
  allow_srand = (allow_updates >= 0) ? allow_updates : allow_srand;

  int fd = open("/dev/urandom", O_RDONLY);
  if (fd >= 0) {
    random_junk[0] = '\0';
    while (random_junk[0] == '\0' || random_junk[1] == '\0') {
      if (read(fd, random_junk, 31) < 4) random_junk[0] = '\0';
      random_junk[31] = '\0';
    }
    close(fd);
  }
  else {
    /* FIXME: Are we on Windows? */
  }

  if (allow_srand) {
    /* We don't srand() using our random_junk directly, as that might leak
     * our random state if values from rand() are used unmodified.
     */
    int rj = (random_junk[0] << 9 | random_junk[1] << 18 | random_junk[2]);
    srand(time(0) ^ getpid() ^ rj);
  }
}

/* Source: https://en.wikipedia.org/wiki/MurmurHash
 * Note: Fixed seed for simplicity.
 */
int32_t murmur3_32(const uint8_t* key, size_t len) {
  uint32_t h = 0xd3632a4d;
  if (len > 3) {
    const uint32_t* key_x4 = (const uint32_t*) key;
    size_t i = len >> 2;
    do {
      uint32_t k = *key_x4++;
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      h ^= k;
      h = (h << 13) | (h >> 19);
      h += (h << 2) + 0xe6546b64;
    } while (--i);
    key = (const uint8_t*) key_x4;
  }
  if (len & 3) {
    size_t i = len & 3;
    uint32_t k = 0;
    key = &key[i - 1];
    do {
      k <<= 8;
      k |= *key--;
    } while (--i);
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    h ^= k;
  }
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

int zero_first_eol(int length, char* data)
{
  int i;
  for (i = 0; i < length; i++)
  {
    if ((i < length - 1) && (data[i] == '\r') && (data[i+1] == '\n'))
    {
      data[i] = data[i+1] = '\0';
      return i+2;
    }
    else if (data[i] == '\n')
    {
      data[i] = '\0';
      return i+1;
    }
  }
  return 0;
}

int zero_first_crlf(int length, char* data)
{
  int i;
  for (i = 0; i < length-1; i++)
  {
    if ((data[i] == '\r') && (data[i+1] == '\n'))
    {
      data[i] = data[i+1] = '\0';
      return i+2;
    }
  }
  return 0;
}

int zero_first_whitespace(int length, char* data)
{
  int i;
  for (i = 0; i < length; i++)
  {
    if (isspace(data[i]))
    {
      data[i] = '\0';
      return i+1;
    }
  }
  return 0;
}

int zero_nth_char(int n, char c, int length, char* data)
{
  int i;
  for (i = 0; i < length; i++)
  {
    if (data[i] == c)
    {
      if (--n < 1) {
        data[i] = '\0';
        return i+1;
      }
    }
  }
  return 0;
}

/* Use binary search to figure out where a string is (or should be)
 * in a list of sorted strings. The returned offset may be larger
 * than haysize-1, if the string should be added to the end of the
 * list. */
int strcaseindex(char** haystack, const char* needle, int haysize)
{
  int bottom = 0;
  int top = haysize;
  while (bottom < top) {
    int midp = (bottom + top) / 2;
    int cmp = strcasecmp(needle, haystack[midp]);
    if (cmp > 0) {
      /* Needle should be in upper half */
      if (bottom < midp) {
        /* Move bottom up, midpoint will be recalculated */
        bottom = midp;
      }
      else {
        /* We know the needle should be after midp, but there's no
         * more space to search. So return the next slot after midp. */
        return midp + 1;
      }
    }
    else if (cmp < 0) {
      /* Needle should be in lower half */
      if (top > midp) {
        /* Move top down, midpoint will be recalculated */
        top = midp;
      }
      else {
        /* We know the needle should be before midp, but there's no
         * more space to search. So return midp itself. */
        return midp;
      }
    }
    else {
      /* The string itself was found, return the index where we found it. */
      return midp;
    }
  }
  return bottom;
}

char *skip_http_header(int length, const char* data)
{
  int i, lfs;
  int chunked = 0;
  char *r = NULL;
  char *p = "\0";
  for (lfs = i = 0; i < length-1; i++) {
    p = (char*) data + i;
    if (*p == '\n') {
      /*                       12345678901234567890123456 = 26 bytes */
      if (!strncasecmp(p + 1, "Transfer-Encoding: chunked", 26)) chunked = 1;
      lfs++;
      if (lfs == 2) {
        if (chunked) {
          r = strchr(p + 1, '\n');
          if (r != NULL) return r + 1;
        }
        return p + 1;
      }
    }
    else if (*p != '\r') {
      lfs = 0;
    }
  }
  return p;
}

char* collapse_whitespace(char* data)
{
  char* w = data;
  char* r = data;
  while (*r) {
    if (isspace(*r)) {
      *w = ' ';
      r++;
      if (!isspace(*r)) w++;
    }
    else {
      *w++ = *r++;
    }
  }
  *w = '\0';
  return data;
}

int dbg_write(int sockfd, char *buffer, int bytes)
{
  printf(">> %s", buffer);
  return PKS_write(sockfd, buffer, bytes);
}

int set_non_blocking(int sockfd)
{
#ifndef _MSC_VER
  int flags;
  if ((0 <= (flags = fcntl(sockfd, F_GETFL, 0))) &&
      (0 <= fcntl(sockfd, F_SETFL, flags | O_NONBLOCK))) return sockfd;
#else
  ULONG nonBlocking = 1;
  if (ioctlsocket(PKS(sockfd), FIONBIO, &nonBlocking) == NO_ERROR) {
    return sockfd;
  }
#endif
  return -1;
}

int set_blocking(int sockfd)
{
#ifndef _MSC_VER
  int flags;
  if ((0 <= (flags = fcntl(sockfd, F_GETFL, 0))) &&
      (0 <= fcntl(sockfd, F_SETFL, flags & (~O_NONBLOCK)))) return sockfd;
#else
  ULONG blocking = 0;
  if (ioctlsocket(PKS(sockfd), FIONBIO, &blocking) == NO_ERROR) {
    return sockfd;
  }
#endif
  return -1;
}

void sleep_ms(int ms)
{
  struct timeval tv;
  tv.tv_sec = (ms / 1000);
  tv.tv_usec = 1000 * (ms % 1000);
  select(0, NULL, NULL, NULL, &tv);
}

void pk_gettime(struct timespec *tp)
{
#if defined(HAVE_CLOCK_MONOTONIC) && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK)
  if (use_clock_gettime) {
    if (clock_gettime(CLOCK_MONOTONIC, tp) != -1) {
      tp->tv_sec += 1;  /* The +1 avoids ever returning zero */
      return;
    }

    if (errno == EINVAL)
      use_clock_gettime = 0;
  }
#endif
  {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = tv.tv_usec * 1000;
  }
}

void pk_pthread_condattr_setclock(pthread_condattr_t* cond_attr) {
#if defined(HAVE_CLOCK_MONOTONIC) && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK)
  /* Check if the monotonic clock works, if it does, configure the
   * condition variable attributes to use it. */
  struct timespec unused_ts;
  pk_gettime(&unused_ts);
  if (use_clock_gettime) {
    pthread_condattr_setclock(cond_attr, CLOCK_MONOTONIC);
  }
#endif
}

time_t64 pk_time()
{
#if defined(HAVE_CLOCK_MONOTONIC) && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK)
  struct timespec tp;
  if (use_clock_gettime) {
    if (clock_gettime(CLOCK_MONOTONIC, &tp) != -1)
      return (tp.tv_sec + 1); /* The +1 avoids ever returning zero */

    if (errno == EINVAL)
      use_clock_gettime = 0;
  }
#endif
  return time(0);
}

int wait_fd(int fd, int timeout_ms)
{
#ifdef HAVE_POLL
  struct pollfd pfd;

  pfd.fd = fd;
  pfd.events = (POLLIN | POLLPRI | POLLHUP);

  return poll(&pfd, 1, timeout_ms);
#else
  fd_set rfds;
  struct timeval tv;

  FD_ZERO(&rfds);

  FD_SET(PKS(fd), &rfds);

  tv.tv_sec = (timeout_ms / 1000);
  tv.tv_usec = 1000 * (timeout_ms % 1000);

  return select(fd+1, &rfds, NULL, NULL, &tv);
#endif
}

ssize_t timed_read(int sockfd, void* buf, size_t count, int timeout_ms)
{
  ssize_t rv;

  set_non_blocking(sockfd);
  do {
    if (0 <= (rv = wait_fd(sockfd, timeout_ms)))
      rv = PKS_read(sockfd, buf, count);
  } while (errno == EINTR);

  set_blocking(sockfd);

  return rv;
}

/* http://www.beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html */
char *in_ipaddr_to_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
  switch (sa->sa_family) {
    case AF_INET:
      strncpy(s, inet_ntoa(((struct sockaddr_in *)sa)->sin_addr), maxlen);
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                s, maxlen);
      break;
#endif
    default:
      strncpy(s, "Unknown AF", maxlen);
      return NULL;
  }
  return s;
}

char *in_addr_to_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
  char* p;
  switch (sa->sa_family) {
    case AF_INET:
      p = s;
      *p++ = '[';
      strncpy(p, inet_ntoa(((struct sockaddr_in *)sa)->sin_addr), maxlen-8);
      p = s+strlen(s);
      *p++ = ']';
      *p++ = ':';
      sprintf(p, "%d", ntohs(((struct sockaddr_in* )sa)->sin_port));
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      p = s;
      *p++ = '[';
      inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                p, maxlen-8);
      p = s+strlen(s);
      *p++ = ']';
      *p++ = ':';
      sprintf(p, "%d", ntohs(((struct sockaddr_in6* )sa)->sin6_port));
      break;
#endif
    default:
      strncpy(s, "Unknown AF", maxlen);
      return NULL;
  }
  return s;
}

struct addrinfo* copy_addrinfo_data(struct addrinfo* dst, struct addrinfo* src)
{
  free_addrinfo_data(dst);
  if (src == NULL) return dst;

  struct sockaddr* addr_buffer = malloc(src->ai_addrlen);
  if (addr_buffer != NULL) {
    memcpy(addr_buffer, src->ai_addr, src->ai_addrlen);
    dst->ai_flags = src->ai_flags;
    dst->ai_family = src->ai_family;
    dst->ai_socktype = src->ai_socktype;
    dst->ai_protocol = src->ai_protocol;
    dst->ai_addrlen = src->ai_addrlen;
    dst->ai_addr = addr_buffer;
    dst->ai_canonname = (src->ai_canonname) ? strdup(src->ai_canonname) : NULL;
    dst->ai_next = NULL;
    return dst;
  }
  return NULL;
}

void free_addrinfo_data(struct addrinfo* dst)
{
  if (dst->ai_canonname != NULL) free(dst->ai_canonname);
  if (dst->ai_addr != NULL) free(dst->ai_addr);
  dst->ai_addrlen = 0;
  dst->ai_addr = NULL;
  dst->ai_canonname = NULL;
}

int addrcmp(const struct sockaddr *a, const struct sockaddr *b)
{
  if (a == NULL || b == NULL) return 3;
  if (a->sa_family != b->sa_family) return 1;
  switch (a->sa_family) {
    case AF_INET:
      return memcmp(&(((struct sockaddr_in*) a)->sin_addr),
                    &(((struct sockaddr_in*) b)->sin_addr),
                    sizeof(struct in_addr));
#ifdef HAVE_IPV6
    case AF_INET6:
      return memcmp(&(((struct sockaddr_in6*) a)->sin6_addr),
                    &(((struct sockaddr_in6*) b)->sin6_addr),
                    sizeof(struct in6_addr));
#endif
  }
  return 2;
}

int http_get(const char* url, char* result_buffer, size_t maxlen)
{
  char *urlparse, *hostname, *port, *path;
  struct addrinfo hints, *result, *rp;
  char request[10240], *bp;
  int sockfd, rlen, bytes, total_bytes;

  /* http://hostname:port/foo */
  urlparse = strdup(url);
  hostname = urlparse+7;
  while (*hostname && *hostname == '/') hostname++;
  port = hostname;
  while (*port && *port != '/' && *port != ':') port++;
  if (*port == '/') {
    path = port;
    *path++ = '\0';
    port = (url[5] == 's') ? "443" : "80";
  }
  else {
    *port++ = '\0';
    path = port;
    while (*path && *path != '/') path++;
    *path++ = '\0';
  }

  rlen = snprintf(request, 10240,
                  "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n", path, hostname);
  if (10240 == rlen)
  {
    free(urlparse);
    return -1;
  }

  total_bytes = 0;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (0 == getaddrinfo(hostname, port, &hints, &result)) {
    for (rp = result; rp != NULL; rp = rp->ai_next) {
      if ((0 > (sockfd = PKS_socket(rp->ai_family, rp->ai_socktype,
                                    rp->ai_protocol))) ||
          PKS_fail(PKS_connect(sockfd, rp->ai_addr, rp->ai_addrlen)) ||
          PKS_fail(PKS_write(sockfd, request, rlen))) {
        if (sockfd >= 0) PKS_close(sockfd);
      }
      else {
        total_bytes = 0;
        bp = result_buffer;
        do {
          bytes = timed_read(sockfd, bp, maxlen-(1+total_bytes), 5000);
          if (bytes > 0) {
            bp += bytes;
            total_bytes += bytes;
          }
        } while (bytes > 0);
        *bp = '\0';
        PKS_close(sockfd);
        break;
      }
    }
    freeaddrinfo(result);
  }
  free(urlparse);
  return total_bytes;
}

void digest_to_hex(const unsigned char* digest, char *output)
{
    int i,j;
    char *c = output;

    /* SHA1_DIGEST_SIZE == 20 */
    for (i = 0; i < 20/4; i++) {
        for (j = 0; j < 4; j++) {
            sprintf(c,"%02x", digest[i*4+j]);
            c += 2;
        }
    }
    *c = '\0';
}

int printable_binary(char* dest, size_t dlen, const char* src, size_t slen)
{
  char* p = (char *) src;
  int copied = 0;
  while (slen > copied) {
    if (*p == '\0') {
      /* Ensure we have space for two bytes plus null */
      if (dlen < 3) {
        *dest++ = '\0';
        return copied;
      }
      *dest++ = '\\';
      *dest++ = '0';
      dlen -= 2;
    }
    else if (isgraph(*p) || (*p == ' ')) {
      /* Ensure we have space for one byte plus null */
      if (dlen < 2) {
        *dest++ = '\0';
        return copied;
      }
      *dest++ = *p;
      dlen--;
    }
    else {
      /* Ensure we have space for four bytes plus null */
      if (dlen < 5) {
        *dest++ = '\0';
        return copied;
      }
      /* The cast to unsigned char is required, otherwise 'negative' byte
       * values will be sign-extended to int and will take more than four
       * bytes; e.g. 255 is printed as \xffffffff */
      int wrote = sprintf(dest, "\\x%2.2x", (unsigned char) *p);
      dest += wrote;
      dlen -= wrote;
    }
    copied++;
    p++;
  }

  *dest++ = '\0';
  return copied;
}

void pk_rlock_init(pk_rlock_t* rlock)
{
  rlock->count = 0;
  rlock->owner = pthread_self();
  pthread_mutex_init(&(rlock->check_lock), NULL);
  pthread_mutex_init(&(rlock->lock_lock), NULL);
}

void pk_rlock_lock(pk_rlock_t* rlock)
{
  pthread_mutex_lock(&(rlock->check_lock));
  if (!pthread_equal(pthread_self(), rlock->owner)) {
    pthread_mutex_unlock(&(rlock->check_lock));
    pthread_mutex_lock(&(rlock->lock_lock));
    pthread_mutex_lock(&(rlock->check_lock));
    rlock->owner = pthread_self();
    rlock->count = 0;
  }
  rlock->count += 1;
  pthread_mutex_unlock(&(rlock->check_lock));
}

void pk_rlock_unlock(pk_rlock_t* rlock)
{
  pthread_mutex_lock(&(rlock->check_lock));
  if (pthread_equal(pthread_self(), rlock->owner)) {
    rlock->count -= 1;
    if (rlock->count < 1) {
      rlock->count = 0;
      pthread_mutex_unlock(&(rlock->lock_lock));
    }
  }
  else {
    /* Allow other threads to force the release of the lock */
    pthread_mutex_unlock(&(rlock->lock_lock));
    rlock->count = 0;
  }
  pthread_mutex_unlock(&(rlock->check_lock));
}

#if PK_MEMORY_CANARIES
#define MAX_CANARIES 102400
void** canaries[MAX_CANARIES];
int canary_max = 0;
pthread_mutex_t canary_lock;
#endif

void remove_memory_canary(void** canary) {
#if PK_MEMORY_CANARIES
    pthread_mutex_lock(&canary_lock);
    for (int i = 0; i < canary_max; i++) {
        if (canaries[i] == canary) {
            if (canary_max > 1) {
                canaries[i] = canaries[--canary_max];
            }
            else {
                canary_max -= 1;
            }
            break;
        }
    }
    pthread_mutex_unlock(&canary_lock);
#endif
    (void) canary;
}

void add_memory_canary(void** canary) {
#if PK_MEMORY_CANARIES
    if (*canary && (*canary == canary)) {
        remove_memory_canary(canary);
    }
    pthread_mutex_lock(&canary_lock);
    *canary = canary;
    canaries[canary_max++] = canary;
    assert(canary_max < MAX_CANARIES);
    pthread_mutex_unlock(&canary_lock);
#endif
    (void) canary;
}

int check_memory_canaries() {
#if PK_MEMORY_CANARIES
    int i, bad;
    pthread_mutex_lock(&canary_lock);
    for (bad = i = 0; i < canary_max; i++) {
        if (canaries[i] != *canaries[i]) {
            fprintf(stderr, "%p != %p\n",
                    (void *) canaries[i], *canaries[i]);
            bad++;
        }
    }
    pthread_mutex_unlock(&canary_lock);
    return bad;
#else
    return 0;
#endif
}

void reset_memory_canaries() {
#if PK_MEMORY_CANARIES
    pthread_mutex_lock(&canary_lock);
    canary_max = 0;
    pthread_mutex_unlock(&canary_lock);
#endif
}

void init_memory_canaries() {
#if PK_MEMORY_CANARIES
    pthread_mutex_init(&canary_lock, NULL);
#endif
}


/* *** pk_dict ************************************************************** */

#define _PK_DICT_MINBITS   10  /* Default start: 1024 entries */
#define _PK_DICT_MAXBITS   27  /* Default max size: 134 million entries */
#define _PK_DICT_MAXBUCKET 40  /* How many entries can ever be the same */
#define _PK_DICT_HASH(d, m3)  ((d->seed ^ m3 ^ (m3 >> d->bits)) & ((1 << d->bits) - 1))
#define _PK_DICT_MAXP(d)      (d->entries + (1 << d->bits))
#define _PK_DICT_TOOBIG(d, b) ((b > 23) && ((1 << b) > (3 * d->count)))
#define _PK_DICT_SHRINK(d)    ((d->bits > 16) && (d->shrink_at > d->count))

int _pk_dict_set_size(pk_dict_t* dict, size_t bits) {
  assert(bits <= 31);
  assert(bits >= _PK_DICT_MINBITS);
  dict->bits = min(bits, dict->maxbits);
  dict->count = 0;
  dict->shrink_at = (1 << bits) / 5;
  dict->entries = calloc(1 << bits, sizeof(pk_dict_ent_t));
  return (dict->entries != NULL);
}

int pk_dict_init(pk_dict_t* dict, int maxbits, int maxbucket) {
  dict->maxbits = maxbits;
  dict->maxbucket = maxbucket;
  dict->seed = rand();
  if (dict->maxbits < 1) dict->maxbits = _PK_DICT_MAXBITS;
  if (dict->maxbucket < 1) dict->maxbucket = _PK_DICT_MAXBUCKET;
  _pk_dict_set_size(dict, _PK_DICT_MINBITS);
  pthread_mutex_init(&(dict->lock), NULL);
  return (dict->entries != NULL);
}

void pk_dict_free(pk_dict_t* dict) {
  pthread_mutex_lock(&(dict->lock));
  if (dict->entries) {
    pk_dict_ent_t* p;
    pk_dict_ent_t* max_p = _PK_DICT_MAXP(dict);
    for (p = dict->entries; p < max_p; p++) {
      if (p->key) free(p->key);
    }
    free(dict->entries);
  }
  dict->entries = NULL;
  dict->bits = 0;
  dict->count = 0;
  pthread_mutex_unlock(&(dict->lock));
}

#if PK_TESTS
void _pk_dump_dict(pk_dict_t* dict) {
  pk_dict_ent_t* p;
  pk_dict_ent_t* max_p = _PK_DICT_MAXP(dict);
  for (p = dict->entries; p < max_p; p++) {
    fprintf(stderr, "%c", p->key ? '#' : '.');
  }
  fprintf(stderr, "\n");
}
#endif

void* _pk_dict_add(pk_dict_t*, int32_t, char*, void*);
int _pk_dict_resize(pk_dict_t* dict, int bits) {

  /* Refuse to resize if we are at our max size or wasting lots of RAM. */
  if (bits > dict->maxbits) return 0;
  if (_PK_DICT_TOOBIG(dict, bits)) return 0;

#if PK_TESTS
  fprintf(stderr, "pk_dict resize: from %d to %d\n", dict->bits, bits);
#endif

  pk_dict_ent_t* p;
  pk_dict_ent_t* max_p = _PK_DICT_MAXP(dict);
  void* old_entries = dict->entries;
  size_t old_count = dict->count;
  size_t old_shrink_at = dict->shrink_at;
  int old_bits = dict->bits;
  int failed = 0;
  if (_pk_dict_set_size(dict, bits)) {
    for (p = old_entries; p < max_p; p++) {
      if (p->val && p->key) {
        if (p->val != _pk_dict_add(dict, p->m3, p->key, p->val)) {
          failed++;
          break;
        }
      }
    }
  }
  else {
    failed++;
  }
  if (failed) {
#if PK_TESTS
    //_pk_dump_dict(dict);
    fprintf(stderr, "pk_dict resize: failed, rolling back\n");
#endif
    if (dict->entries) free(dict->entries);
    dict->bits = old_bits;
    dict->count = old_count;
    dict->shrink_at = old_shrink_at;
    dict->entries = old_entries;
  }
  else {
    dict->shrink_at = min(dict->shrink_at, dict->count - (dict->count/3));
#if PK_TESTS
    fprintf(stderr,
      "pk_dict resize: space=%d old_count=%zd new=%zd shrink_at=%zd\n",
      (1 << dict->bits), old_count, dict->count, dict->shrink_at);
    assert(old_count == dict->count);
#endif
    free(old_entries);
  }
  return (!failed);
}

int _pk_dict_key_hashlen(const char* key) {
  int len = strlen(key);

  // Hash xyz.foo.bar as xyz.f
  int ep = len-1;
  while ((key[ep] != '.') && (ep > 4)) ep--;
  if (key[ep] == '.') {
    ep--;
    while ((key[ep] != '.') && (ep > 4)) ep--;
    if (key[ep] == '.') len = ep+2;
  }
  return len;
}

void* pk_dict_find(pk_dict_t* dict, char* key) {
  if (!dict->entries || !dict->count) return NULL;
  int32_t m3 = murmur3_32(key, _pk_dict_key_hashlen(key));
  pthread_mutex_lock(&(dict->lock));
  pk_dict_ent_t* p = dict->entries + _PK_DICT_HASH(dict, m3);
  pk_dict_ent_t* max_p = _PK_DICT_MAXP(dict);
  for (int i = 0; i < dict->maxbucket; i++, p++) {
    if (p >= max_p) p = dict->entries;
    if ((p->m3 == m3) && (p->key) && (0 == strcmp(key, p->key))) {
      void* v = p->val;
      pthread_mutex_unlock(&(dict->lock));
      return v;
    }
  }
  pthread_mutex_unlock(&(dict->lock));
  return NULL;
}

void* _pk_dict_add(pk_dict_t* dict, int32_t m3, char* key, void* val) {
  pk_dict_ent_t* p = dict->entries + _PK_DICT_HASH(dict, m3);
  pk_dict_ent_t* max_p = _PK_DICT_MAXP(dict);
  for (int i = 0; i < dict->maxbucket; i++, p++) {
    if (p >= max_p) p = dict->entries;
    if (NULL == p->key) {
      p->m3 = m3;
      p->key = key;
      p->val = val;
      dict->count++;
      return val;
    }
  }
  return NULL;
}

void* pk_dict_add(pk_dict_t* dict, char* key, void* val) {
  void* rv;
  assert(NULL != dict->entries);
  assert(NULL != val);
  assert(NULL != key);

  key = strdup(key);
  if (key == NULL) return NULL;
  int32_t m3 = murmur3_32(key, _pk_dict_key_hashlen(key));

  pthread_mutex_lock(&(dict->lock));
  for (int tries = 0; tries < 2; tries++) {
    rv = _pk_dict_add(dict, m3, key, val);
    if (rv != NULL) break;
    if (!_pk_dict_resize(dict, dict->bits + 1)) break;
  }
  pthread_mutex_unlock(&(dict->lock));

  if (rv == NULL) free(key);
  return rv;
}

void* pk_dict_rm(pk_dict_t* dict, char* key) {
  if (!dict->entries || !dict->count) return NULL;
  int32_t m3 = murmur3_32(key, _pk_dict_key_hashlen(key));
  pthread_mutex_lock(&(dict->lock));
  pk_dict_ent_t* p = dict->entries + _PK_DICT_HASH(dict, m3);
  pk_dict_ent_t* max_p = _PK_DICT_MAXP(dict);
  for (int i = 0; i < dict->maxbucket; i++, p++) {
    if (p >= max_p) p = dict->entries;
    if ((p->m3 == m3) && (p->key) && (0 == strcmp(key, p->key))) {
      void* v = p->val;
      free(p->key);
      p->m3 = 0;
      p->key = NULL;
      p->val = NULL;
      dict->count--;
      if (_PK_DICT_SHRINK(dict)) {
        if (!_pk_dict_resize(dict, dict->bits - 1)) {
          dict->shrink_at -= (dict->shrink_at / 5);
        }
      }
      pthread_mutex_unlock(&(dict->lock));
      return v;
    }
  }
  pthread_mutex_unlock(&(dict->lock));
  return NULL;
}

int pk_dict_rm_val(pk_dict_t* dict, void* val) {
  if (!dict->entries || !dict->count) return 0;
  int removed = 0;
  pthread_mutex_lock(&(dict->lock));
  pk_dict_ent_t* max_p = _PK_DICT_MAXP(dict);
  for (pk_dict_ent_t* p = dict->entries; p < max_p; p++) {
    if (p->key && (p->val == val)) {
      free(p->key);
      p->m3 = 0;
      p->key = NULL;
      p->val = NULL;
      dict->count--;
      removed++;
      if (_PK_DICT_SHRINK(dict)) {
        if (!_pk_dict_resize(dict, dict->bits - 1)) {
          dict->shrink_at -= (dict->shrink_at / 5);
        }
      }
    }
  }
  pthread_mutex_unlock(&(dict->lock));
  return removed;
}


/* *** Tests *************************************************************** */

#if PK_TESTS
#include <time.h>
#endif

int utils_test(void)
{
#if PK_TESTS
  char binary[] = {'b', 'i', 'n', '\r', '\n', 0};
  char* haystack[] = {"b", "c", "d"};
  char buffer1[60];
  PK_MEMORY_CANARY;

  strcpy(buffer1, "\r\n\r\n");
  assert(2 == zero_first_crlf(4, buffer1));

  strcpy(buffer1, "abcd\r\n\r\ndefghijklmnop");
  int length = zero_first_crlf(strlen(buffer1), buffer1);

  assert(length == 6);
  assert((buffer1[4] == '\0') && (buffer1[5] == '\0') && (buffer1[6] == '\r'));
  assert(strcmp(buffer1, "abcd") == 0);

  strcpy(buffer1, "abcd\n\ndefghijklmnop");
  length = zero_first_eol(strlen(buffer1), buffer1);
  assert(length == 5);
  assert((buffer1[4] == '\0') && (buffer1[5] == '\n'));
  assert(strcmp(buffer1, "abcd") == 0);

  strcpy(buffer1, "abcd\r\nfoo\r\n\r\ndef");
  assert(strcmp(skip_http_header(strlen(buffer1), buffer1), "def") == 0);

  assert(0 == strcaseindex((char**) haystack, "a", 3));
  assert(1 == strcaseindex((char**) haystack, "BB", 3));
  assert(2 == strcaseindex((char**) haystack, "cC", 3));
  assert(3 == strcaseindex((char**) haystack, "E", 3));

  assert(printable_binary(buffer1, 7, binary, 6) == 3);
  assert(strcasecmp(buffer1, "bin") == 0);
  assert(printable_binary(buffer1, 8, binary, 6) == 4);
  assert(strcasecmp(buffer1, "bin\\x0d") == 0);
  assert(printable_binary(buffer1, 60, binary, 6) == 6);
  assert(strcasecmp(buffer1, "bin\\x0d\\x0a\\0") == 0);

  char* world = "world";
  pk_dict_t pkd;
  pk_dict_init(&pkd, 0, 0);
  assert(_pk_dict_key_hashlen("12345678.pagekite.me") == 10);
  assert(_pk_dict_key_hashlen("1234.302.is") == 6);
  assert(_pk_dict_key_hashlen("1.pagekite.me") == 13);
  assert(_pk_dict_key_hashlen("123456789012345") == 15);
  fprintf(stderr,
    "pk_dict: bits=%d count=%zd entries=%d@%p\n",
    pkd.bits, pkd.count, 1 << pkd.bits, pkd.entries);
  for (int i = 0; i < (_PK_DICT_MAXBUCKET / 2); i++) {
    assert(world == pk_dict_add(&pkd, "hello", world));
    assert(world == pk_dict_add(&pkd, "ohai", world));
  }
  fprintf(stderr,
    "pk_dict: bits=%d count=%zd entries=%d@%p\n",
    pkd.bits, pkd.count, 1 << pkd.bits, pkd.entries);
  for (int i = 0; i < (_PK_DICT_MAXBUCKET / 2); i++) {
    assert(world == pk_dict_rm(&pkd, "hello"));
  }
  assert(NULL == pk_dict_rm(&pkd, "hello"));
  assert(NULL == pk_dict_find(&pkd, "hello"));
  assert(world == pk_dict_find(&pkd, "ohai"));
  for (int i = 0; i < 102400; i++) {
    char hello[30];
    sprintf(hello, "%d.pagekite.me", i);
    assert(world == pk_dict_add(&pkd, hello, world));
  }
  for (int i = 0; i < 102400; i++) {
    char hello[30];
    sprintf(hello, "%d.pagekite.me", i);
    assert(world == pk_dict_rm(&pkd, hello));
  }
  fprintf(stderr,
    "pk_dict: bits=%d count=%zd entries=%d@%p\n",
    pkd.bits, pkd.count, 1 << pkd.bits, pkd.entries);
  assert(pkd.count == pk_dict_rm_val(&pkd, world));
  fprintf(stderr,
    "pk_dict: bits=%d count=%zd entries=%d@%p (after pk_dict_rm_val())\n",
    pkd.bits, pkd.count, 1 << pkd.bits, pkd.entries);

  char key[16];
  struct timespec t0, t1, t2;
  for (int l = 0; l < 10; l++) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);
    for (int i = 0; i < 1e6; i++) {
      sprintf(key, "%x", i);
    }
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
    for (int i = 0; i < 1e6; i++) {
      sprintf(key, "%x", i);
      pk_dict_rm(&pkd, key);
      pk_dict_add(&pkd, key, (i+1));
    }
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t2);
    long ns0 = (t0.tv_sec * (long)1e9) + t0.tv_nsec;
    long ns1 = (t1.tv_sec * (long)1e9) + t1.tv_nsec;
    long ns2 = (t2.tv_sec * (long)1e9) + t2.tv_nsec;
    float timed_ns = (ns2 - ns1) - (ns1 - ns0);
    fprintf(stderr,
      "pk_dict: TIMING DATA  %ld - %ld = %.3fs Mops/s=%.3f\n",
      ns2-ns1, ns1-ns0, timed_ns / ((float) 1e9), 2 * (float)1e9 / timed_ns);
  }
  fprintf(stderr,
    "pk_dict: bits=%d count=%zd entries=%d@%p MB=%zd\n",
    pkd.bits, pkd.count, 1 << pkd.bits, pkd.entries, (1<<(pkd.bits-20)) * sizeof(*pkd.entries));
  pk_dict_free(&pkd);
  fprintf(stderr,
    "pk_dict: bits=%d count=%zd entries=%d@%p\n",
    pkd.bits, pkd.count, 1 << pkd.bits, pkd.entries);


#if PK_MEMORY_CANARIES
  add_memory_canary(&canary);
  PK_CHECK_MEMORY_CANARIES;
  canary = (void*) 0;
  assert(check_memory_canaries() == 1);
  canary = &canary;
  remove_memory_canary(&canary);
#endif
#endif
  return 1;
}
