/******************************************************************************
pagekite.c - A high-performance PageKite implementation in C.

Usage: pagekite PORT PROTO NAME.pagekite.me SECRET

*******************************************************************************

This file is Copyright 2011, The Beanstalks Project ehf.

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
#include <ev.h>

#include "pkproto.h"
#include "pkmanager.h"

void usage(void) {
  printf("Usage: pagekite PORT PROTO NAME.pagekite.me SECRET");
  exit(1);
}

int main(int argc, char **argv) {
  struct ev_loop *loop = EV_DEFAULT;
  int port;

  if ((argc != 4) || (1 != sscanf(argv[1], "%d", &port))) usage();

  m = pk_manager_init(loop, 128*1024);
  pk_manage_kite(m, port, argv[2], argv[3], argv[4]);

  ev_run(loop, 0);

  return 0;
}

