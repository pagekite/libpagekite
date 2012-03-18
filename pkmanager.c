/******************************************************************************
pkmanager.c - A manager for multiple pagekite connections.

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

#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <ev.h>

#include "utils.h"
#include "pkproto.h"
#include "pkmanager.h"

void pkm_chunk_cb(struct pk_frontend* frontend, struct pk_chunk *chunk) {
  /* FIXME */
}

struct pk_manager* pk_manager_init(struct ev_loop* loop,
                                   int buffer_size, unsigned char* buffer,
                                   int kites, int frontends, int conns)
{
  struct pk_manager* pkm;
  unsigned int i, parse_buffer_bytes;

  memset(buffer, 0, buffer_size);

  pkm = (struct pk_manager*) buffer;
  pkm->buffer_bytes_free = buffer_size;

  pkm->buffer = buffer + sizeof(struct pk_manager);
  pkm->buffer_bytes_free -= sizeof(struct pk_manager);
  pkm->buffer_base = pkm->buffer;
  pkm->loop = loop;
  if (pkm->buffer_bytes_free < 0) return NULL;

  if (kites < MIN_KITE_ALLOC) kites = MIN_KITE_ALLOC;
  if (frontends < MIN_FE_ALLOC) frontends = MIN_FE_ALLOC;
  if (conns < MIN_CONN_ALLOC) conns = MIN_CONN_ALLOC;

  /* Allocate space for the kites */
  pkm->buffer_bytes_free -= sizeof(struct pk_pagekite) * kites;
  if (pkm->buffer_bytes_free < 0) return NULL;
  pkm->kites = (struct pk_pagekite *) pkm->buffer;
  pkm->kite_count = kites;
  pkm->buffer += sizeof(struct pk_pagekite) * kites;

  /* Allocate space for the frontends */
  pkm->buffer_bytes_free -= sizeof(struct pk_frontend) * frontends;
  if (pkm->buffer_bytes_free < 0) return NULL;
  pkm->frontends = (struct pk_frontend *) pkm->buffer;
  pkm->frontend_count = frontends;
  pkm->buffer += sizeof(struct pk_frontend) * frontends;

  /* Allocate space for the backend connections */
  pkm->buffer_bytes_free -= sizeof(struct pk_backend_conn) * conns;
  if (pkm->buffer_bytes_free < 0) return NULL;
  pkm->be_conns = (struct pk_backend_conn *) pkm->buffer;
  pkm->be_conn_count = conns;
  pkm->buffer += sizeof(struct pk_backend_conn) * conns;

  /* Whatever is left, we divide evenly between the protocol parsers... */
  parse_buffer_bytes = (pkm->buffer_bytes_free-1) / frontends;
  /* ... within reason. */
  if (parse_buffer_bytes > PARSER_BYTES_MAX)
    parse_buffer_bytes = PARSER_BYTES_MAX;
  if (parse_buffer_bytes < PARSER_BYTES_MIN)
    return NULL;

  /* Initialize the frontend structs... */
  for (i = 0; i < frontends; i++) {
    (pkm->frontends+i)->manager = pkm;
    (pkm->frontends+i)->parser = pk_parser_init(parse_buffer_bytes,
                                                (char *) pkm->buffer,
                                                (pkChunkCallback*) &pkm_chunk_cb,
                                                (pkm->frontends+i));
    pkm->buffer += parse_buffer_bytes;
    pkm->buffer_bytes_free -= parse_buffer_bytes;
  }

  return pkm;
}

int pk_add_kite(struct pk_manager* pkm,
                char* protocol, char* auth_secret,
                char* public_domain, int public_port,
                int local_port)
{
  return 1;
}

int pk_add_frontend(struct pk_manager* pkm,
                    char* hostname, int port, int priority)
{
  return 1;
}


/**[ TESTS ]******************************************************************/

int pkmanager_test(void)
{
  unsigned char buffer[64000];
  struct pk_manager* m;

  /* Are too-small buffers handled correctly? */
  assert(NULL == pk_manager_init(NULL, 1000, buffer, 1000, -1, -1));
  assert(NULL == pk_manager_init(NULL, 1000, buffer, -1, 1000, -1));
  assert(NULL == pk_manager_init(NULL, 1000, buffer, -1, -1, 1000));

  /* Create a real one */
  assert(NULL != (m = pk_manager_init(NULL, 64000, buffer, -1, -1, -1)));

  /* Ensure the right defaults are used. */
  assert(m->frontend_count == MIN_FE_ALLOC);
  assert(m->kite_count == MIN_KITE_ALLOC);
  assert(m->be_conn_count == MIN_CONN_ALLOC);

  /* Ensure memory regions don't overlap */
  memset(m->be_conns,  3, sizeof(struct pk_backend_conn) * m->be_conn_count);
  memset(m->frontends, 2, sizeof(struct pk_frontend)     * m->frontend_count);
  memset(m->kites,     1, sizeof(struct pk_pagekite)     * m->kite_count);
  assert(0 == *((char*) m->buffer));
  assert(1 == *((char*) m->kites));
  assert(2 == *((char*) m->frontends));
  assert(3 == *((char*) m->be_conns));

  return 1;
}

