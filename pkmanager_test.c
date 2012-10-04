/******************************************************************************
pkmanager_test.c - Testing the manager for multiple pagekite connections.

This file is Copyright 2011, 2012, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms  of the  Apache  License 2.0  as published by the  Apache  Software
Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details.

You should have received a copy of the Apache License along with this program.
If not, see: <http://www.apache.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

#include "common.h"
#include "utils.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkstate.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"

int pkmanager_test(void)
{
  void *N = NULL;
  char buffer[PK_MANAGER_MINSIZE];
  struct pk_manager* m;
  struct pk_backend_conn* c;
  struct pk_job j;
  struct addrinfo ai;
  int i;

  /* Are too-small buffers handled correctly? */
  assert(NULL == pkm_manager_init(N, 1000, buffer, 1000, -1, -1, N, N));
  assert(NULL == pkm_manager_init(N, 1000, buffer, -1, 1000, -1, N, N));
  assert(NULL == pkm_manager_init(N, 1000, buffer, -1, -1, 1000, N, N));

  /* Create a real one */
  m = pkm_manager_init(N, PK_MANAGER_MINSIZE, buffer, -1, -1, -1, N, N);
  if (m == NULL) pk_perror("pkmanager.c");
  assert(NULL != m);

  /* Create a real one from heap */
  m = pkm_manager_init(NULL, 0, NULL, -1, -1, -1, NULL, NULL);
  assert(NULL != m);

  /* Ensure the right defaults are used. */
  assert(m->frontend_max == MIN_FE_ALLOC);
  assert(m->kite_max == MIN_KITE_ALLOC);
  assert(m->be_conn_max == MIN_CONN_ALLOC);

  /* Ensure memory regions don't overlap */
  memset(m->be_conns,  3, sizeof(struct pk_backend_conn) * m->be_conn_max);
  memset(m->frontends, 2, sizeof(struct pk_frontend)     * m->frontend_max);
  memset(m->kites,     1, sizeof(struct pk_pagekite)     * m->kite_max);
  assert(0 == *((char*) m->buffer));
  assert(1 == *((char*) m->kites));
  assert(2 == *((char*) m->frontends));
  assert(3 == *((char*) m->be_conns));

  /* Recreate, because those memsets broke things. */
  m = pkm_manager_init(NULL, 0, NULL, -1, -1, -1, NULL, NULL);
  assert(NULL != m);

  /* Test pk_add_job and pk_get_job */
  assert(0 == m->blocking_jobs.count);
  assert(0 < pkb_add_job(&(m->blocking_jobs), PK_QUIT, NULL));
  assert(1 == m->blocking_jobs.count);
  assert(0 < pkb_get_job(&(m->blocking_jobs), &j));
  assert(0 == m->blocking_jobs.count);
  assert(j.job == PK_QUIT);

  /* Test pk_add_frontend_ai */
  bzero(&ai, sizeof(struct addrinfo));
  for (i = 0; i < MIN_FE_ALLOC; i++)
    assert(NULL != pkm_add_frontend_ai(m, &ai, "woot", 123, 1));
  assert(NULL == pkm_add_frontend_ai(m, &ai, "woot", 123, 1));
  assert(ERR_NO_MORE_FRONTENDS == pk_error);

  /* Test pk_add_kite */
  for (i = 0; i < MIN_KITE_ALLOC; i++)
    assert(NULL != pkm_add_kite(m, "http", "foo", 80, "sec", "localhost", 80));
  assert(NULL == pkm_add_kite(m, "http", "foo", 80, "sec", "localhost", 80));
  assert(ERR_NO_MORE_KITES == pk_error);
  assert(NULL != pkm_find_kite(m, "http", "foo", 80));
  assert(NULL == pkm_find_kite(m, "http", "bar", 80));

  /* Test pk_*_be_conn */
  assert(NULL == pkm_find_be_conn(m, NULL, "abc"));
  assert(NULL != pkm_alloc_be_conn(m, NULL, "abc"));
  assert(NULL != pkm_alloc_be_conn(m, NULL, "abcd"));
  assert(NULL != pkm_alloc_be_conn(m, NULL, "abcef"));
  assert(NULL != (c = pkm_find_be_conn(m, NULL, "abc")));
  assert(0 == strcmp(c->sid, "abc"));
  assert(0 == c->conn.read_kb);
  assert(0 == c->conn.read_bytes);
  assert(CONN_IO_BUFFER_SIZE == PKC_IN_FREE(c->conn));
  assert(CONN_IO_BUFFER_SIZE == PKC_OUT_FREE(c->conn));
  pkm_free_be_conn(c);
  assert(NULL == pkm_find_be_conn(m, NULL, "abc"));

  return 1;
}
