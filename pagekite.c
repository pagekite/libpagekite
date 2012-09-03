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

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

#include "common.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pklogging.h"
#include "pkblocker.h"
#include "pkmanager.h"

#define PK_DDNS        "http://up.pagekite.net/?hostname=%s&myip=%s&sign=%s"
#define PK_V4FRONTENDS "frontends.b5p.us"
#define PK_V6FRONTENDS "v6frontends.b5p.us"

struct pk_global_state pk_state;

void usage(void) {
  printf("Usage: pagekite LPORT PROTO NAME.pagekite.me PPORT SECRET\n");
}

int main(int argc, char **argv) {
  struct pk_manager *m;
  char* proto;
  char* kitename;
  char* secret;
  int pport;
  int lport;
  SSL_CTX* ssl_ctx;

  /* FIXME: Is this too lame? */
  srand(time(0) ^ getpid());

  if ((argc != 6) ||
      (1 != sscanf(argv[1], "%d", &lport)) ||
      (1 != sscanf(argv[4], "%d", &pport))) {
    usage();
    exit(1);
  }
  proto = argv[2];
  kitename = argv[3];
  secret = argv[5];

  pk_state.log_mask = PK_LOG_NORMAL;
  pk_state.log_mask = PK_LOG_ALL;

  INIT_PAGEKITE_SSL(ssl_ctx);

  if ((NULL == (m = pkm_manager_init(NULL, 0, NULL, 10, 10, 100,
                                     PK_DDNS, ssl_ctx))) ||
      (NULL == (pkm_add_kite(m, proto, kitename, 0, secret,
                                "localhost", lport))) ||
      (0 >= (pkm_add_frontend(m, kitename, pport, FE_STATUS_AUTO))) ||
      (0 >= (pkm_add_frontend(m, PK_V4FRONTENDS, 443, FE_STATUS_AUTO))) ||
      (0 >= (pkm_add_frontend(m, PK_V6FRONTENDS, 443, FE_STATUS_AUTO))) ||
      (0 > pkm_run_in_thread(m))) {
    pk_perror(argv[0]);
    exit(1);
  }

  pkm_wait_thread(m);
  return 0;
}

