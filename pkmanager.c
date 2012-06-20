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
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
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

void pkm_chunk_cb(struct pk_frontend* fe, struct pk_chunk *chunk)
{
  struct pk_backend_conn* pkc; /* FIXME: What if we are a front-end? */
  char pong[256];
  int bytes;

  pk_log_chunk(chunk);

  if (NULL != chunk->noop) {
    if (NULL != chunk->ping) {
      bytes = pk_format_pong(pong);
      pkm_write_data(&(fe->conn), bytes, pong);
    }
  }
  else {
    if ((NULL != chunk->sid) &&
        ((NULL != (pkc = pkm_find_be_conn(fe->manager, chunk->sid))) ||
         (NULL != (pkc = pkm_connect_be(fe, chunk))))) {
      /* We are happy, pkc should be a valid connection. */
      fprintf(stderr, "Connected?\n");
    }
    else {
      pkc = NULL;
    }
    if (NULL == pkc) {
      /* FIXME: Send back an error */
    }
    else {
      pkm_write_data(&(pkc->conn), chunk->length, chunk->data);
      if ((NULL != chunk->eof) &&
          (NULL == pkm_eof(&(pkc->conn), chunk->eof))) {
        /* FIXME: Completely dead.  Deallocate pk_backend_conn ? */
      }
    }
  }
}

struct pk_backend_conn* pkm_connect_be(struct pk_frontend* fe,
                                       struct pk_chunk* chunk)
{
  /* Connect to the backend, or free the conn object if we fail */
  int sockfd;
  struct sockaddr_in addr_buf;
  struct sockaddr_in* addr;
  struct hostent *backend;
  struct pk_backend_conn* pkc;
  struct pk_pagekite *kite;

  /* FIXME: Better error handling? */

  /* First, search the list of configured back-ends for one that matches
     the request in the chunk.  If nothing is found, there is no point in
     continuing. */
  if (NULL == (kite = pkm_find_kite(fe->manager,
                                    chunk->request_proto,
                                    chunk->request_host,
                                    chunk->request_port)))
    return NULL;

  /* Allocate a connection for this request or die... */
  if (NULL == (pkc = pkm_alloc_be_conn(fe->manager, chunk->sid)))
    return NULL;

  /* Look up the back-end... */
  addr = NULL;
  if (NULL != (backend = gethostbyname(kite->local_domain))) {
    addr = &addr_buf;
    bzero((char *) addr, sizeof(addr));
    addr->sin_family = AF_INET;
    bcopy((char*) backend->h_addr_list[0],
          (char*) &(addr->sin_addr.s_addr),
          backend->h_length);
    addr->sin_port = htons(kite->local_port);
  }

  /* Try to connect and set non-blocking. */
  errno = 0;
  if ((NULL == addr) ||
      (0 > (sockfd = socket(AF_INET, SOCK_STREAM, 0))) ||
      (0 > set_non_blocking(sockfd)) ||
      (0 > connect(sockfd, (struct sockaddr*) addr, sizeof(*addr))))
  {
    if (errno != EINPROGRESS) {
      close(sockfd);
      pkm_free_be_conn(pkc);
      return NULL;
    }
  }

  /* FIXME: This should be non-blocking for use on high volume front-ends,
            but that requires more buffering and fancy logic, so we're
            lazy for now.
            See also: http://developerweb.net/viewtopic.php?id=3196 */

  pkc->kite = kite;
  pkc->frontend = fe;
  pkc->conn.sockfd = sockfd;

  return pkc;
}

struct pk_conn* pkm_eof(struct pk_conn* pkc, char *eof)
{
  /* Shut down to varying degrees, depending on what kind of EOF this is. */
  /* Return NULL if we are completely dead, pkc otherwise */
  return NULL;
}

int pkm_write_data(struct pk_conn* pkc, int length, char* data)
{
  /* 1. Try to flush already buffered data. */
  /* 2. If successful, try to write new data (0 copies!) */
  /* 2a. Buffer any unwritten data. */
  /* 3. Else, if new+old data > buffer size, do a blocking write. */

  return length;
}

int pkm_read_data(struct pk_conn* pkc)
{
  int bytes;
  bytes = read(pkc->sockfd,
               pkc->in_buffer + (CONN_IO_BUFFER_SIZE - pkc->in_buffer_bytes_free),
               pkc->in_buffer_bytes_free);
  pkc->in_buffer_bytes_free -= bytes;
  return bytes;
}

static void pkm_tunnel_readable_cb(EV_P_ ev_io *w, int revents)
{
  struct pk_frontend* fe;

  fe = (struct pk_frontend*) w->data;
  if (0 < pkm_read_data(&(fe->conn))) {
    if (0 > pk_parser_parse(fe->parser,
                            (CONN_IO_BUFFER_SIZE-fe->conn.in_buffer_bytes_free),
                            (char *) fe->conn.in_buffer))
    {
      /* Parse failed: remote end is a borked: should kill this connection. */
      pk_perror("pkmanager.c");
    }
    fe->conn.in_buffer_bytes_free = CONN_IO_BUFFER_SIZE;
  }
  else {
    /* EOF, do something more sane than this. */
    ev_io_stop(EV_A_ w);
  }
}

static void pkm_tunnel_writable_cb(EV_P_ ev_io *w, int revents)
{
  struct pk_frontend* fe;

  fe = (struct pk_frontend*) w->data;
  fprintf(stderr, "Writable: %s:%d\n", fe->fe_hostname, fe->fe_port);
  ev_io_stop(EV_A_ w);
}

static void pkm_be_conn_readable_cb(EV_P_ ev_io *w, int revents)
{
}

static void pkm_be_conn_writable_cb(EV_P_ ev_io *w, int revents)
{
}

static void pkm_timer_cb(EV_P_ ev_timer *w, int revents)
{
  struct pk_manager* pkm;
  struct pk_frontend *fe;
  struct pk_kite_request *kite_r;
  int i, j, reconnect;

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
      fe->request_count = pkm->kite_count;
      bzero(fe->requests, pkm->kite_count * sizeof(struct pk_kite_request));
      for (kite_r = fe->requests, j = 0; j < pkm->kite_count; j++, kite_r++) {
        kite_r->kite = (pkm->kites + j);
        kite_r->status = PK_STATUS_UNKNOWN;
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
      if ((0 <= (fe->conn.sockfd = pk_connect(fe->fe_hostname,
                                              fe->fe_port, NULL,
                                              fe->request_count,
                                              fe->requests))) &&
          (0 <= set_non_blocking(fe->conn.sockfd))) {
        fprintf(stderr, "Connected to %s:%d\n", fe->fe_hostname, fe->fe_port);

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

struct pk_manager* pkm_manager_init(struct ev_loop* loop,
                                    int buffer_size, char* buffer,
                                    int kites, int frontends, int conns)
{
  struct pk_manager* pkm;
  int i;
  unsigned int parse_buffer_bytes;

  if (kites < MIN_KITE_ALLOC) kites = MIN_KITE_ALLOC;
  if (frontends < MIN_FE_ALLOC) frontends = MIN_FE_ALLOC;
  if (conns < MIN_CONN_ALLOC) conns = MIN_CONN_ALLOC;

  if (buffer == NULL) {
    buffer_size = PK_MANAGER_BUFSIZE(kites, frontends, conns, PARSER_BYTES_AVG);
    buffer = malloc(buffer_size);
  }

  memset(buffer, 0, buffer_size);

  pkm = (struct pk_manager*) buffer;
  pkm->buffer_bytes_free = buffer_size;

  pkm->buffer = buffer + sizeof(struct pk_manager);
  pkm->buffer_bytes_free -= sizeof(struct pk_manager);
  pkm->buffer_base = pkm->buffer;
  pkm->loop = loop;
  if (pkm->buffer_bytes_free < 0) return pk_err_null(ERR_TOOBIG_MANAGER);

  /* Allocate space for the kites */
  pkm->buffer_bytes_free -= sizeof(struct pk_pagekite) * kites;
  if (pkm->buffer_bytes_free < 0) return pk_err_null(ERR_TOOBIG_KITES);
  pkm->kites = (struct pk_pagekite *) pkm->buffer;
  pkm->kite_count = kites;
  pkm->buffer += sizeof(struct pk_pagekite) * kites;

  /* Allocate space for the frontends */
  pkm->buffer_bytes_free -= (sizeof(struct pk_frontend) * frontends);
  pkm->buffer_bytes_free -= (sizeof(struct pk_kite_request) * kites * frontends);
  if (pkm->buffer_bytes_free < 0) return pk_err_null(ERR_TOOBIG_FRONTENDS);
  pkm->frontends = (struct pk_frontend *) pkm->buffer;
  pkm->frontend_count = frontends;
  pkm->buffer += sizeof(struct pk_frontend) * frontends;
  for (i = 0; i < frontends; i++) {
    (pkm->frontends + i)->requests = (struct pk_kite_request*) pkm->buffer;
    pkm->buffer += (sizeof(struct pk_kite_request) * kites);
  }

  /* Allocate space for the backend connections */
  pkm->buffer_bytes_free -= sizeof(struct pk_backend_conn) * conns;
  if (pkm->buffer_bytes_free < 0) return pk_err_null(ERR_TOOBIG_BE_CONNS);
  pkm->be_conns = (struct pk_backend_conn *) pkm->buffer;
  pkm->be_conn_count = conns;
  pkm->buffer += sizeof(struct pk_backend_conn) * conns;

  /* Whatever is left, we divide evenly between the protocol parsers... */
  parse_buffer_bytes = (pkm->buffer_bytes_free-1) / frontends;
  /* ... within reason. */
  if (parse_buffer_bytes > PARSER_BYTES_MAX)
    parse_buffer_bytes = PARSER_BYTES_MAX;
  if (parse_buffer_bytes < PARSER_BYTES_MIN)
    return pk_err_null(ERR_TOOBIG_PARSERS);

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

struct pk_pagekite* pkm_find_kite(struct pk_manager* pkm,
                                  const char* protocol,
                                  const char* domain,
                                  int port)
{
  int which;
  struct pk_pagekite* kite;
  struct pk_pagekite* found;

  /* FIXME: This is O(N), we'll need a nicer data structure for frontends */
  found = NULL;
  for (which = 0; which < pkm->kite_count; which++) {
    kite = pkm->kites+which;
    if (kite->protocol != NULL) {
      if ((0 == strcasecmp(domain, kite->public_domain)) &&
          (0 == strcasecmp(protocol, kite->protocol))) {
        if (kite->public_port <= 0)
          found = kite;
        else if (kite->public_port == port)
          return kite;
      }
    }
  }
  return found;
}

struct pk_pagekite* pkm_add_kite(struct pk_manager* pkm,
                                 const char* protocol,
                                 const char* public_domain, int public_port,
                                 const char* auth_secret,
                                 const char* local_domain, int local_port)
{
  int which;
  struct pk_pagekite* kite;

  /* FIXME: This is O(N), we'll need a nicer data structure for frontends */
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
  kite->local_domain = local_domain;
  kite->local_port = local_port;

  return kite;
}

struct pk_frontend* pkm_add_frontend(struct pk_manager* pkm,
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

  return fe;
}

unsigned char pkm_sid_shift(char *sid)
{
  unsigned char shift;
  char *c;

  for (c = sid, shift = 0; *c != '\0'; c++) {
    shift = (shift << 3) | (shift >> 5);
    shift ^= *c;
  }
  return shift;
}

struct pk_backend_conn* pkm_alloc_be_conn(struct pk_manager* pkm, char *sid)
{
  int i;
  unsigned char shift;
  struct pk_backend_conn* pkc;

  shift = pkm_sid_shift(sid);
  for (i = 0; i < pkm->be_conn_count; i++) {
    pkc = (pkm->be_conns + ((i + shift) % pkm->be_conn_count));
    if (!(pkc->conn.status & CONN_STATUS_ALLOCATED)) {
      pkc->conn.status = CONN_STATUS_ALLOCATED;
      strncpy(pkc->sid, sid, BE_MAX_SID_SIZE);
      return pkc;
    }
  }
  return NULL;
}

void pkm_free_be_conn(struct pk_backend_conn* pkc)
{
  pkc->conn.status = CONN_STATUS_UNKNOWN;
}

struct pk_backend_conn* pkm_find_be_conn(struct pk_manager* pkm, char* sid)
{
  int i;
  unsigned char shift;
  struct pk_backend_conn* pkc;

  shift = pkm_sid_shift(sid);
  for (i = 0; i < pkm->be_conn_count; i++) {
    pkc = (pkm->be_conns + ((i + shift) % pkm->be_conn_count));
    if ((pkc->conn.status & CONN_STATUS_ALLOCATED) &&
        (0 == strncmp(pkc->sid, sid, BE_MAX_SID_SIZE))) {
      return pkc;
    }
  }
  return NULL;
}
