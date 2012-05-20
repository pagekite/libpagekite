/******************************************************************************
pkmanager_test.c - Testing the manager for multiple pagekite connections.

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

#define _GNU_SOURCE 1
#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <ev.h>

#include "utils.h"
#include "pkerror.h"
#include "pkproto.h"
#include "pkmanager.h"

int pkmanager_test(void)
{
  char buffer[PK_MANAGER_MINSIZE];
  struct pk_manager* m;
  struct pk_backend_conn* c;
  int i;

  /* Are too-small buffers handled correctly? */
  assert(NULL == pkm_manager_init(NULL, 1000, buffer, 1000, -1, -1));
  assert(NULL == pkm_manager_init(NULL, 1000, buffer, -1, 1000, -1));
  assert(NULL == pkm_manager_init(NULL, 1000, buffer, -1, -1, 1000));

  /* Create a real one */
  m = pkm_manager_init(NULL, PK_MANAGER_MINSIZE, buffer, -1, -1, -1);
  if (m == NULL) pk_perror("pkmanager.c");
  assert(NULL != m);

  /* Create a real one from heap */
  m = pkm_manager_init(NULL, 0, NULL, -1, -1, -1);
  assert(NULL != m);

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

  /* Test pk_add_frontend */
  assert(NULL == pkm_add_frontend(m, "woot", 123, 1));
  assert(ERR_NO_MORE_FRONTENDS == pk_error);
  memset(m->frontends, 0, sizeof(struct pk_frontend) * m->frontend_count);
  for (i = 0; i < MIN_FE_ALLOC; i++)
    assert(NULL != pkm_add_frontend(m, "woot", 123, 1));
  assert(NULL == pkm_add_frontend(m, "woot", 123, 1));

  /* Test pk_add_kite */
  assert(NULL == pkm_add_kite(m, "http", "foo", "sec", 80, 80));
  assert(ERR_NO_MORE_KITES == pk_error);
  memset(m->kites, 0, sizeof(struct pk_pagekite) * m->kite_count);
  for (i = 0; i < MIN_KITE_ALLOC; i++)
    assert(NULL != pkm_add_kite(m, "http", "foo", "sec", 80, 80));
  assert(NULL == pkm_add_kite(m, "http", "foo", "sec", 80, 80));

  /* Test pk_*_be_conn */
  assert(NULL == pkm_find_be_conn(m, "abc"));
  assert(NULL != pkm_alloc_be_conn(m, "abc"));
  assert(NULL != pkm_alloc_be_conn(m, "abcd"));
  assert(NULL != pkm_alloc_be_conn(m, "abcef"));
  assert(NULL != (c = pkm_find_be_conn(m, "abc")));
  assert(0 == strcmp(c->sid, "abc"));
  pkm_free_be_conn(c);
  assert(NULL == pkm_find_be_conn(m, "abc"));

  return 1;
}
