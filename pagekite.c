/******************************************************************************
pagekite.c - A high-performance PageKite implementation in C.

Usage: pagekite LPORT PROTO NAME.pagekite.me PPORT SECRET

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
#include <ev.h>

#include "utils.h"
#include "pkerror.h"
#include "pkproto.h"
#include "pkmanager.h"

#define PK_BUFFER_SIZE  128 * 1024

void usage(void) {
  printf("Usage: pagekite LPORT PROTO NAME.pagekite.me PPORT SECRET\n");
}

int main(int argc, char **argv) {
  struct ev_loop *loop = EV_DEFAULT;
  struct pk_manager *m;
  unsigned char buffer[PK_BUFFER_SIZE];
  char* proto;
  char* kitename;
  char* secret;
  int pport;
  int lport;

  if ((argc != 6) ||
      (1 != sscanf(argv[1], "%d", &lport)) ||
      (1 != sscanf(argv[4], "%d", &pport))) {
    usage();
    exit(1);
  }
  proto = argv[2];
  kitename = argv[3];
  secret = argv[5];

  if (NULL == (m = pk_manager_init(loop, PK_BUFFER_SIZE, buffer, 1, 4, 16))) {
    pk_perror(argv[0]);
    exit(1);
  }
  pk_add_kite(m, proto, secret, kitename, 0, lport);
  pk_add_frontend(m, kitename, pport, 1);
  ev_loop(loop, 0);

  return 0;
}

