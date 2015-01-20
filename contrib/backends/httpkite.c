/******************************************************************************
httpkite.c - A trivial (example) PageKite HTTP server.

Usage: httpkite NAME.pagekite.me SECRET

*******************************************************************************

This file is Copyright 2011-2015, The Beanstalks Project ehf.

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

#include <pagekite.h>
#include "pkcommon.h"
#include "pkutils.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkstate.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"
#include "pkwatchdog.h"


void usage(void) {
  printf("Usage: httpkite your.kitename.com SECRET\n\n");
  printf("Note: DNS needs to already be configured for the kite name, and\n");
  printf("      a running front-end on the IP address it points to. This\n");
  printf("      is easiest to do by using the pagekite.net service and\n");
  printf("      creating the kite first using pagekite.py.\n");
}

void handle_request(void* data, struct pk_chunk *chunk) {
  char buffer[4096];
  char *hi = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello World\n";
  struct pk_conn* pkc = data;
  int bytes;

  pk_log_chunk(chunk);
  if (chunk->ping) {
    bytes = pk_format_pong(buffer);
    pkc_write(pkc, buffer, bytes);
  }
  else if (chunk->sid) {
    if (chunk->eof) {
      /* Ignored, for now */
    }
    else if (!chunk->noop) {

      /* Send a reply, and close this channel right away */
      bytes = pk_format_reply(buffer, chunk->sid, strlen(hi), hi);
      pkc_write(pkc, buffer, bytes);

      bytes = pk_format_eof(buffer, chunk->sid, PK_EOF);
      pkc_write(pkc, buffer, bytes);
    }
  }
  else {
    /* Weirdness ... */
  }
}

int main(int argc, char **argv) {
  char pbuffer[64000];
  struct pk_conn pkc;
  struct pk_parser* pkp;
  struct pk_pagekite kite;
  struct pk_kite_request kite_r;
  struct pk_kite_request* kite_rp;
  SSL_CTX* ctx;

  if (argc < 3) {
    usage();
    exit(1);
  }

  pks_global_init(PK_LOG_ALL);
  PKS_SSL_INIT(ctx);

  kite_r.kite = &kite;
  strcpy(kite.protocol, "http");
  strncpyz(kite.public_domain, argv[1], PK_DOMAIN_LENGTH);
  kite.public_port = 0;
  strncpyz(kite.auth_secret, argv[2], PK_SECRET_LENGTH);

  kite_r.bsalt[0] = '\0';
  kite_r.fsalt[0] = '\0';
  kite_rp = &kite_r;

  srand(time(0) ^ getpid());
  if (0 > pk_connect(&pkc, argv[1], 443, 1, &kite_r, NULL, ctx)) {
    pk_perror(argv[1]);
    usage();
    return 1;
  }

  pkp = pk_parser_init(sizeof(pbuffer), pbuffer, &handle_request, &pkc);
  if (NULL == pkp) {
    pk_perror(argv[1]);
    usage();
    return 1;
  }

  fprintf(stderr, "*** Connected! ***\n");
  while (pkc_wait(&pkc, -1)) {
    pkc_read(&pkc);
    pk_parser_parse(pkp, pkc.in_buffer_pos, (char *) pkc.in_buffer);
    pkc.in_buffer_pos = 0;
  }
  pkc_reset_conn(&pkc, 0);

  /* -Wall dislikes unused variables */
  kite_rp++;

  return 0;
}
