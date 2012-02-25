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
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
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
  char *reply = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nThis is my reply\r\n";
  int *fd = data;
  int bytes;

  if (chunk->ping) {
    bytes = pk_format_pong(buffer);
    fprintf(stderr, "\n>> %s\n<< ", buffer);
    write(*fd, buffer, bytes);
  }
  else if (chunk->sid) {
    if (chunk->eof) {
      fprintf(stderr, "\n[EOF:%s]\n<< ", chunk->sid);
    }
    else if (!chunk->noop) {
      /* Send a reply, and close this channel right away */
      bytes = pk_format_reply(buffer, chunk->sid, strlen(reply), reply);
      fprintf(stderr, "\n>> %s\n<< ", buffer);
      write(*fd, buffer, bytes);

      bytes = pk_format_eof(buffer, chunk->sid);
      fprintf(stderr, "\n>> %s\n<< ", buffer);
      write(*fd, buffer, bytes);
    }
  }
  else {
    fprintf(stderr, "\n[BOGON]\n<< ");
  }
}

int main(int argc, char **argv) {
  int fd;
  char pbuffer[64000], rbuffer[8192];
  struct pk_parser* pkp;
  struct pk_kite_request kite;
  struct pk_kite_request* kitep;

  if (argc < 4) usage();

  kite.kitename = argv[2];
  kite.secret = argv[3];
  kite.proto = "http";
  kite.port = 0;

  kite.bsalt = NULL;
  kite.fsalt = NULL;
  kitep = &kite;

  srand(time(0) ^ getpid());
  fd = pk_connect(argv[1], 443, NULL, 1, &kitep);
  if (fd < 0) {
    perror(argv[1]);
    return 1;
  }

  pkp = pk_parser_init(sizeof(pbuffer), pbuffer, &handle_request, &fd);
  fprintf(stderr, "<< ");
  while (read(fd, rbuffer, 1) == 1) {
    fprintf(stderr, "%c", rbuffer[0]);
    pk_parser_parse(pkp, 1, rbuffer);
  }
  close(fd);

  return 0;
}

