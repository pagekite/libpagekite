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

struct pk_conn* pkm_alloc_be_conn(struct pk_manager* pkm) {
  int i;
  struct pk_conn* pkc;
  for (i = 0, pkc = pkm->be_conns; i < pkm->be_conn_count; i++, pkc++) {
    if (!(pkc->status & CONN_STATUS_ALLOCATED)) {
      pkc->status = CONN_STATUS_ALLOCATED;
      return pkc;
    }
  }
  return NULL;
}

void pkm_chunk_cb(struct pk_frontend* fe, struct pk_chunk *chunk) {
  /* FIXME */
  fprintf(stderr, "A chunk!\n");
}

int pkm_write_data(struct pk_conn* pkc, int length, unsigned char* data) {
  int bytes;
  /* FIXME */
}

int pkm_read_data(struct pk_conn* pkc) {
  int bytes;
  bytes = read(pkc->sockfd,
               pkc->in_buffer + (CONN_IO_BUFFER_SIZE - pkc->in_buffer_bytes_free),
               pkc->in_buffer_bytes_free);
  pkc->in_buffer_bytes_free -= bytes;
  return bytes;
}

static void pkm_tunnel_readable_cb(EV_P_ ev_io *w, int revents) {
  struct pk_frontend* fe;
  int leftovers;
  fe = (struct pk_frontend*) w->data;
  fprintf(stderr, "Readable: %s:%d\n", fe->fe_hostname, fe->fe_port);
  if (0 < pkm_read_data(&(fe->conn))) {
    if (0 > pk_parser_parse(fe->parser,
                            (CONN_IO_BUFFER_SIZE - fe->conn.in_buffer_bytes_free),
                            fe->conn.in_buffer))
    {
      /* FIXME: What does this mean??? */
      pk_perror("pkmanager.c");
    }
    fe->conn.in_buffer_bytes_free = CONN_IO_BUFFER_SIZE;
  }
  else {
    /* EOF, do something more sane than this. */
    ev_io_stop(EV_A_ w);
  }
}

static void pkm_tunnel_writable_cb(EV_P_ ev_io *w, int revents) {
  struct pk_frontend* fe;
  fe = (struct pk_frontend*) w->data;
  fprintf(stderr, "Writable: %s:%d\n", fe->fe_hostname, fe->fe_port);
  ev_io_stop(EV_A_ w);
}

static void pkm_be_conn_readable_cb(EV_P_ ev_io *w, int revents) {
}

static void pkm_be_conn_writable_cb(EV_P_ ev_io *w, int revents) {

}

static void pkm_timer_cb(EV_P_ ev_timer *w, int revents) {
  struct pk_manager* pkm;
  struct pk_frontend *fe;
  struct pk_kite_request *kite_r;
  int i, j, reconnect, flags;

  pkm = (struct pk_manager*) w->data;
  fprintf(stderr, "Tick %p %p %x\n", (void *) loop, (void *) pkm, revents);

  /* Loop through all configured tunnels:
   *   - if idle, queue a ping.
   *   - if dead, shut 'em down.
   */

  /* Loop through all configured kites:
   *   - if missing a front-end, tear down tunnels and reconnect.
   */
  for (i = 0; i < pkm->frontend_count; i++) {
    fe = (pkm->frontends + i);
    if (fe->fe_hostname == NULL) continue;

    if (fe->requests == NULL || fe->request_count != pkm->kite_count) {

      if (fe->requests != NULL) free(fe->requests);
      fe->requests = malloc(pkm->kite_count * sizeof(struct pk_kite_request));
      bzero(fe->requests, pkm->kite_count * sizeof(struct pk_kite_request));
      fe->request_count = 0;
      for (kite_r = fe->requests, j = 0; j < pkm->kite_count; j++, kite_r++) {
        kite_r->kite = (pkm->kites + j);
        kite_r->status = PK_STATUS_UNKNOWN;
        fe->request_count++;
      }
    }

    reconnect = 0;
    for (kite_r = fe->requests, j = 0; j < pkm->kite_count; j++, kite_r++) {
      if (kite_r->status == PK_STATUS_UNKNOWN) reconnect++;
    }
    if (reconnect) {
      fprintf(stderr, "Reconnecting to %s\n", fe->fe_hostname);
      if (fe->conn.sockfd) {
        ev_io_stop(pkm->loop, &(fe->conn.watch_r));
        ev_io_stop(pkm->loop, &(fe->conn.watch_w));
        close(fe->conn.sockfd);
      }
      fe->conn.status = CONN_STATUS_ALLOCATED;
      fe->conn.activity = time(0);
      fe->conn.in_buffer_bytes_free = CONN_IO_BUFFER_SIZE;
      fe->conn.out_buffer_bytes_free = CONN_IO_BUFFER_SIZE;
      if (0 < (fe->conn.sockfd = pk_connect(fe->fe_hostname, fe->fe_port, NULL,
                                            fe->request_count, fe->requests))) {
        fprintf(stderr, "Connected to %s:%d\n", fe->fe_hostname, fe->fe_port);

        /* Set non-blocking */
        flags = fcntl(fe->conn.sockfd, F_GETFL, 0);
        fcntl(fe->conn.sockfd, F_SETFL, flags|O_NONBLOCK);

        ev_io_init(&(fe->conn.watch_r), pkm_tunnel_readable_cb,
                   fe->conn.sockfd, EV_READ);
        ev_io_start(pkm->loop, &(fe->conn.watch_r));
        ev_io_init(&(fe->conn.watch_w), pkm_tunnel_writable_cb,
                   fe->conn.sockfd, EV_WRITE);
        fe->conn.watch_r.data = fe->conn.watch_w.data = (void *) fe;
      }
      else {
        pk_perror("pkmanager.c");
      }
    }
  }

}

struct pk_manager* pk_manager_init(struct ev_loop* loop,
                                   int buffer_size, unsigned char* buffer,
                                   int kites, int frontends, int conns)
{
  struct pk_manager* pkm;
  int i;
  unsigned int parse_buffer_bytes;

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

  /* Set up our event-loop callbacks */
  if (loop != NULL) {
    ev_timer_init(&(pkm->timer), pkm_timer_cb, 0, PK_HOUSEKEEPING_INTERVAL);
    pkm->timer.data = (void *) pkm;
    ev_timer_start(loop, &(pkm->timer));
  }

  return pkm;
}

struct pk_pagekite* pk_add_kite(struct pk_manager* pkm,
                                const char* protocol,
                                const char* auth_secret,
                                const char* public_domain, int public_port,
                                int local_port)
{
  int which;
  struct pk_pagekite* kite;

  for (which = 0; which < pkm->kite_count; which++) {
    kite = pkm->kites+which;
    if (kite->protocol == NULL) break;
  }
  if (which >= pkm->kite_count)
    return pk_err_null(ERR_NO_MORE_KITES);

  kite->protocol = strdup(protocol);
  kite->auth_secret = strdup(auth_secret);
  kite->public_domain = strdup(public_domain);
  kite->public_port = public_port;
  kite->local_port = local_port;

  return kite;
}

struct pk_frontend* pk_add_frontend(struct pk_manager* pkm,
                                    const char* hostname,
                                    int port, int priority)
{
  int which;
  struct pk_frontend* fe;

  for (which = 0; which < pkm->frontend_count; which++) {
    fe = pkm->frontends+which;
    if (fe->fe_hostname == NULL) break;
  }
  if (which >= pkm->frontend_count)
    return pk_err_null(ERR_NO_MORE_FRONTENDS);

  fe->fe_hostname = strdup(hostname);
  fe->fe_port = port;
  fe->priority = priority;
  fe->request_count = 0;
  fe->requests = NULL;

  return fe;
}


/**[ TESTS ]******************************************************************/

int pkmanager_test(void)
{
  unsigned char buffer[64000];
  struct pk_manager* m;
  int i;

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

  /* Test pk_add_frontend */
  assert(NULL == pk_add_frontend(m, "woot", 123, 1));
  assert(ERR_NO_MORE_FRONTENDS == pk_error);
  memset(m->frontends, 0, sizeof(struct pk_frontend) * m->frontend_count);
  for (i = 0; i < MIN_FE_ALLOC; i++)
    assert(NULL != pk_add_frontend(m, "woot", 123, 1));
  assert(NULL == pk_add_frontend(m, "woot", 123, 1));

  /* Test pk_add_kite */
  assert(NULL == pk_add_kite(m, "http", "foo", "sec", 80, 80));
  assert(ERR_NO_MORE_KITES == pk_error);
  memset(m->kites, 0, sizeof(struct pk_pagekite) * m->kite_count);
  for (i = 0; i < MIN_KITE_ALLOC; i++)
    assert(NULL != pk_add_kite(m, "http", "foo", "sec", 80, 80));
  assert(NULL == pk_add_kite(m, "http", "foo", "sec", 80, 80));

  return 1;
}

