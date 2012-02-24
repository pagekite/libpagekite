/******************************************************************************
httpkite.c - A trivial (example) PageKite HTTP server.

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
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

#include "utils.h"
#include "pkproto.h"

void usage(void) {
  printf("Usage: httpkite NAME.pagekite.me SECRET\n");
  exit(1);
}

void handle_request(void* data, struct pk_chunk *chunk) {
  char buffer[4096];
  char *reply = "This is my reply";
  int *fd = data;
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
  char pbuffer[64000], rbuffer[8192];
  struct pk_parser* pkp;
  struct pk_kite_request kites[1];

  if (argc < 3) usage();

  kites[0].kitename = argv[2];
  kites[0].secret = argv[3];
  kites[0].port = 0;
  kites[0].proto = "http";
  kites[0].bsalt = NULL;
  kites[0].fsalt = NULL;

  fd = pk_connect(argv[1], 443, 1, (struct pk_kite_request **) &kites);
  if (fd < 0) {
    perror(argv[1]);
    return 1;
  }

  pkp = pk_parser_init(sizeof(pbuffer), pbuffer, &handle_request, &fd);

  close(fd);
  return 0;
}

