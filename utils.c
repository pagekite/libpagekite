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
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
char *in_addr_to_str(const struct sockaddr *sa, char *s, size_t maxlen)
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
