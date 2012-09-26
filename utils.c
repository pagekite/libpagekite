/******************************************************************************
utils.c - Utility functions for pagekite.

This file is Copyright 2011, 2012, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms of the  GNU  Affero General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more
details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see: <http://www.gnu.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

#include "common.h"
#include <fcntl.h>
#include <poll.h>

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

int dbg_write(int sockfd, char *buffer, int bytes)
{
  printf(">> %s", buffer);
  return write(sockfd, buffer, bytes);
}

int set_non_blocking(int sockfd)
{
  int flags;
  if ((0 <= (flags = fcntl(sockfd, F_GETFL, 0))) &&
      (0 <= fcntl(sockfd, F_SETFL, flags | O_NONBLOCK))) return sockfd;
  return -1;
}

int set_blocking(int sockfd)
{
  int flags;
  if ((0 <= (flags = fcntl(sockfd, F_GETFL, 0))) &&
      (0 <= fcntl(sockfd, F_SETFL, flags & (~O_NONBLOCK)))) return sockfd;
  return -1;
}

ssize_t timed_read(int sockfd, void* buf, size_t count, int timeout)
{
  struct pollfd pfd;
  ssize_t rv;

  set_non_blocking(sockfd);
  pfd.fd = sockfd;
  pfd.events = (POLLIN | POLLPRI | POLLHUP);
  do {
    if (0 <= (rv = poll(&pfd, 1, timeout)))
      rv = read(sockfd, buf, count);
  } while (errno == EINTR);
  set_blocking(sockfd);

  return rv;
}

/* http://www.beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html */
char *in_ipaddr_to_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
  switch (sa->sa_family) {
    case AF_INET:
      inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                s, maxlen);
      break;

    case AF_INET6:
      inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                s, maxlen);
      break;

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
      inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                p, maxlen-8);
      p = s+strlen(s);
      *p++ = ']';
      *p++ = ':';
      sprintf(p, "%d", ntohs(((struct sockaddr_in* )sa)->sin_port));
      break;

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

    default:
      strncpy(s, "Unknown AF", maxlen);
      return NULL;
  }
  return s;
}

int addrcmp(const struct sockaddr *a, const struct sockaddr *b)
{
  if (a->sa_family != b->sa_family) return 1;
  switch (a->sa_family) {
    case AF_INET:
      return memcmp(&(((struct sockaddr_in*) a)->sin_addr),
                    &(((struct sockaddr_in*) b)->sin_addr),
                    sizeof(struct in_addr));
    case AF_INET6:
      return memcmp(&(((struct sockaddr_in6*) a)->sin6_addr),
                    &(((struct sockaddr_in6*) b)->sin6_addr),
                    sizeof(struct in6_addr));
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
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (0 == getaddrinfo(hostname, port, &hints, &result)) {
    for (rp = result; rp != NULL; rp = rp->ai_next) {
      if ((0 > (sockfd = socket(rp->ai_family, rp->ai_socktype,
                                rp->ai_protocol))) ||
          (0 > connect(sockfd, rp->ai_addr, rp->ai_addrlen)) ||
          (0 > write(sockfd, request, rlen)))
      {
        if (sockfd >= 0) close(sockfd);
      }
      else {
        total_bytes = 0;
        bp = result_buffer;
        do {
          bytes = timed_read(sockfd, bp, maxlen-(1+total_bytes), 1000);
          if (bytes > 0) {
            bp += bytes;
            total_bytes += bytes;
          }
        } while (bytes > 0);
        *bp = '\0';
        close(sockfd);
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

