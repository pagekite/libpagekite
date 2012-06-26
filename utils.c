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

******************************************************************************/
#include <fcntl.h>
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
