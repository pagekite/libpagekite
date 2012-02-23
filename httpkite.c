/******************************************************************************
httpkite.c - A high-performance PageKite implementation in C.

Usage: httpkite NAME.pagekite.me SECRET

*******************************************************************************

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
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>

#include "utils.h"
#include "pkproto.h"

void usage(void) {
  printf("Usage: httpkite NAME.pagekite.me SECRET");
  exit(1);
}

int handle_request(int* fd, struct pk_chunk *chunk) {
  unsigned char buffer[4096];
  char *reply = "This is my reply";
  int bytes;

  if (chunk->sid && !chunk->noop) {
    /* Send a reply, and close this channel right away */
    bytes = pk_format_reply(buffer, chunk, strlen(reply), reply);
    bytes += pk_format_eof(buffer+bytes, chunk);
    send(*fd, buffer, bytes, 0);
  }
  if (chunk->ping) {
    bytes = pk_format_pong(buffer, chunk);
    send(*fd, buffer, bytes, 0);
  }
}

int main(int argc, char **argv) {
  int fd;

  if (argc < 3) usage();

  fd = pk_connect(argv[1], 443, argv[1], argv[2]);

  close(fd);

  return 0;
}

