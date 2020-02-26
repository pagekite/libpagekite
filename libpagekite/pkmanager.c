/******************************************************************************
pkmanager.c - A manager for multiple pagekite connections.

This file is Copyright 2011-2020, The Beanstalks Project ehf.

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

#define PAGEKITE_CONSTANTS_ONLY
#include "pagekite.h"

#include "pkcommon.h"
#include <fcntl.h>

#include "pkutils.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkstate.h"
#include "pkhooks.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"
#include "pkwatchdog.h"
#include "opensslthreadlock.h"

#ifdef __MINGW32__
#include <mxe/evwrap.c>
#endif

/* Forward declarations of all the functions we don't want made public. */
static void pkm_yield(struct pk_manager *pkm);
static void pkm_interrupt_cb(EV_P_ ev_async *w, int revents);
static void pkm_interrupt(struct pk_manager *pkm);
static void pkm_block(struct pk_manager *pkm);
static void pkm_unblock(struct pk_manager *pkm);
static void pkm_quit_cb(EV_P_ ev_async *w, int revents);
static void pkm_quit(struct pk_manager* pkm);
static void pkm_chunk_cb(struct pk_tunnel*, struct pk_chunk*);
static struct pk_backend_conn* pkm_connect_be(struct pk_tunnel*,
                                              struct pk_chunk*);
static ssize_t pkm_write_chunked(struct pk_tunnel*, struct pk_backend_conn*,
                                 ssize_t, char*);
static int pkm_update_io(struct pk_tunnel*, struct pk_backend_conn*, int);
static void pkm_flow_control_tunnel(struct pk_tunnel*, flow_op, int);
static void pkm_flow_control_conn(struct pk_conn*, flow_op);
static void pkm_parse_eof(struct pk_backend_conn* pkb, char *eof);
static void pkm_tunnel_readable_cb(EV_P_ ev_io*, int);
static void pkm_tunnel_writable_cb(EV_P_ ev_io*, int);
static void pkm_be_conn_readable_cb(EV_P_ ev_io*, int);
static void pkm_be_conn_writable_cb(EV_P_ ev_io*, int);
static void pkm_listener_cb(EV_P_ ev_io*, int);
static void pkm_tick_cb(EV_P_ ev_async*, int);
static void pkm_timer_cb(EV_P_ ev_timer*, int);
static void pkm_reset_timer(struct pk_manager*);
static void pkm_reset_manager(struct pk_manager*);
static struct pk_pagekite* pkm_find_kite(struct pk_manager*,
                                         const char*, const char*, int);
static unsigned int pkm_sid_shift(char *);

#ifndef HAVE_PTHREAD_YIELD
#  ifdef HAVE_PTHREAD_YIELD_NP
#    define pthread_yield pthread_yield_np
#  elif HAVE_SCHED_YIELD
#    include <sched.h>
#    define pthread_yield sched_yield
#  elif defined (_MSC_VER)
#    define pthread_yield()
#  else
#    error "Missing pthread_yield() equivalent."
#  endif
#endif

static void pkm_yield_start(struct pk_manager *pkm)
{
  pthread_mutex_unlock(&(pkm->loop_lock));
  pthread_yield();
}
static void pkm_yield_stop(struct pk_manager *pkm)
{
  pthread_yield();
  pthread_mutex_lock(&(pkm->loop_lock));
}
static void pkm_yield(struct pk_manager *pkm)
{
  pkm_yield_start(pkm);
  pkm_yield_stop(pkm);
}
static void pkm_interrupt_cb(EV_P_ ev_async *w, int revents)
{
  struct pk_manager* pkm = (struct pk_manager*) w->data;

  /* This interleaving of locks guarantees that the event loop will be idle
     and the caller of pkm_interrupt() will be holding the loop_lock when
     it returns, and the event loop will not resume until the lock has been
     handed over to, and returned by, the caller. */
  pkm_yield_start(pkm);
  pthread_mutex_lock(&(pkm->intr_lock));
  pkm_yield_stop(pkm);
  pthread_mutex_unlock(&(pkm->intr_lock));

  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) (revents);
}
static void pkm_interrupt(struct pk_manager *pkm)
{
  /* Prevent multiple interrupts from running at once and trade locks
     with the event loop thread. */
  pthread_mutex_lock(&(pkm->intr_lock));
  ev_async_send(pkm->loop, &(pkm->interrupt));
  pthread_mutex_lock(&(pkm->loop_lock));
  pthread_mutex_unlock(&(pkm->intr_lock));
}

static void pkm_block(struct pk_manager *pkm)
{
  if (!pthread_equal(pthread_self(), pkm->main_thread)) {
    if (0 != pthread_mutex_trylock(&(pkm->loop_lock))) pkm_interrupt(pkm);
  }
}
static void pkm_unblock(struct pk_manager *pkm)
{
  if (!pthread_equal(pthread_self(), pkm->main_thread)) {
    pthread_mutex_unlock(&(pkm->loop_lock));
  }
}
int pkm_reconfig_start(struct pk_manager *pkm)
{
  return pthread_mutex_trylock(&(pkm->config_lock));
}
int pkm_reconfig_blocking_start(struct pk_manager *pkm)
{
  return pthread_mutex_lock(&(pkm->config_lock));
}
void pkm_reconfig_stop(struct pk_manager *pkm)
{
  pthread_mutex_unlock(&(pkm->config_lock));
}

static void pkm_quit_cb(EV_P_ ev_async *w, int revents)
{
  ev_unloop(EV_A_ EVUNLOOP_ALL);
  /* -Wall dislikes unused arguments */
  (void) w;
  (void) revents;
}
static void pkm_quit(struct pk_manager* pkm)
{
  ev_async_send(pkm->loop, &(pkm->quit));
}


static void pkm_chunk_cb(struct pk_tunnel* fe, struct pk_chunk *chunk)
{
  struct pk_backend_conn* pkb; /* FIXME: What if we are a front-end? */
  char reply[PK_REJECT_MAXSIZE], rej[PK_REJECT_MAXSIZE];
  size_t bytes;

  PK_TRACE_FUNCTION;
  pk_log_chunk(fe, chunk);

  pkb = NULL;
  if (NULL != chunk->sid) {
    if ((NULL != (pkb = pkm_find_be_conn(fe->manager, fe, chunk->sid))) ||
        (NULL != chunk->noop) ||
        (NULL != chunk->eof) ||
        (NULL != (pkb = pkm_connect_be(fe, chunk)))) {
      /* We are happy, pkb should be a valid connection. */
      pkm_yield_start(fe->manager);
      /* Zero the chunk chunk request-host to suppress log spam, it was
       * usd in pkm_connect_be above and we don't need it anymore. */
      chunk->request_host = NULL;
    }
    else {
      pkm_yield_start(fe->manager);

      /* FIXME: Send back a nicer error */
      if ((NULL != chunk->request_proto) &&
          (0 == strncasecmp(chunk->request_proto, "https", 5))) {
        bytes = pk_format_reply(reply, chunk->sid, PK_REJECT_TLS_LEN,
                                                   PK_REJECT_TLS_DATA);
        pkc_write(&(fe->conn), reply, bytes);
      }
      else {
        bytes = pk_format_http_rejection(rej,
          PK_REJECT_BACKEND,
          fe->manager->fancy_pagekite_net_rejection_url,
          chunk->request_proto,
          chunk->request_host);
        bytes = pk_format_reply(reply, chunk->sid, bytes, rej);
        pkc_write(&(fe->conn), reply, bytes);
      }

      bytes = pk_format_eof(reply, chunk->sid, PK_EOF);
      pkc_write(&(fe->conn), reply, bytes);
      pk_log(PK_LOG_TUNNEL_CONNS, "No stream found: %s, %s://%s", chunk->sid,
                                  chunk->request_proto, chunk->request_host);
    }
  }
  else
    pkm_yield_start(fe->manager);

  if (fe->manager->enable_http_forwarding_headers) {
    pk_http_forwarding_headers_hook(chunk, pkb);
  }

  PK_HOOK(PK_HOOK_CHUNK_INCOMING, 0, chunk, pkb);

  if (NULL != chunk->noop) {
    if (NULL != chunk->ping) {
      bytes = pk_format_pong(reply);
      pkc_write(&(fe->conn), reply, bytes);
      pk_log(PK_LOG_TUNNEL_DATA, "> --- > Pong!");
      /* Record this ping, even if not initiated by us. This allows us to
       * use pings as a metric of whether a tunnel is in use, to prevent
       * premature disconnections. */
      fe->last_ping = pk_time();
    }
  }
  else if (NULL != pkb) {
    if (NULL == chunk->eof) {
      if (PK_HOOK(PK_HOOK_DATA_OUTGOING, chunk->length, chunk->data, pkb)) {
        pkc_write(&(pkb->conn), chunk->data, chunk->length);
      }
    }
    else {
      pkm_parse_eof(pkb, chunk->eof);
    }
  }

  pkm_yield_stop(fe->manager);
  if (NULL != pkb) {
    /* Flow control: we have recieved a chunk telling us how much has
     * been written to the remote end of the connection. We record this
     * to track progress and tweak our sending window. */

    if (0 < chunk->throttle_spd) {
      /* Integer-safe way to reduce by about 20% */
      pkb->conn.send_window_kb -= (1 + pkb->conn.send_window_kb / 5);
    }

    if (0 < chunk->remote_sent_kb) {
      pkb->conn.sent_kb = chunk->remote_sent_kb;
      if (pkb->conn.send_window_kb == CONN_WINDOW_SIZE_KB_INITIAL) {
        int window_kb = pkb->conn.read_kb - pkb->conn.sent_kb;
        pkb->conn.send_window_kb = window_kb + (window_kb / 8);
      }
      else {
        /* Ramp up our sending speed by default. Push-back from the
         * tunnel blocking will lower this if it gets too high. */
        pkb->conn.send_window_kb += 1;
      }
    }

    if (pkb->conn.send_window_kb > CONN_WINDOW_SIZE_KB_MAXIMUM) {
      pkb->conn.send_window_kb = CONN_WINDOW_SIZE_KB_MAXIMUM;
    }
    else if (pkb->conn.send_window_kb < CONN_WINDOW_SIZE_KB_MINIMUM) {
      pkb->conn.send_window_kb = CONN_WINDOW_SIZE_KB_MINIMUM;
    }

    pkm_update_io(fe, pkb, 0);
  }
}

struct pk_backend_conn* pkm_connect_be(struct pk_tunnel* fe,
                                       struct pk_chunk* chunk)
{
  /* Connect to the backend, or free the conn object if we fail */
  int sockfd;
  struct sockaddr_in addr_buf;
  struct sockaddr_in* addr;
  struct hostent *backend;
  struct pk_backend_conn* pkb;
  struct pk_pagekite *kite;

  PK_TRACE_FUNCTION;

  /* FIXME: Better error handling? */
  if ((NULL == chunk->request_proto) || (NULL == chunk->request_host)) {
    pk_log(PK_LOG_TUNNEL_CONNS, "pkm_connect_be: Request details missing.");
    return NULL;
  }

  /* First, search the list of configured back-ends for one that matches
     the request in the chunk.  If nothing is found, there is no point in
     continuing. */
  if (NULL == (kite = pkm_find_kite(fe->manager,
                                    chunk->request_proto,
                                    chunk->request_host,
                                    chunk->request_port))) {
    pk_log(PK_LOG_TUNNEL_CONNS, "pkm_connect_be: No such kite %s://%s:%d",
                                chunk->request_proto, chunk->request_host,
                                chunk->request_port);
    return NULL;
  }

  /* Allocate a connection for this request or die... */
  if (NULL == (pkb = pkm_alloc_be_conn(fe->manager, fe, chunk->sid))) {
    pk_log(PK_LOG_TUNNEL_CONNS|PK_LOG_ERROR,
           "pkm_connect_be: BE alloc failed for %s://%s:%d",
           chunk->request_proto, chunk->request_host, chunk->request_port);
    return NULL;
  }
  pkm_yield_start(fe->manager);

  /* Look up the back-end... */
  addr = NULL;
  if (NULL != (backend = gethostbyname(kite->local_domain))) {
    addr = &addr_buf;
    memset((char *) addr, 0, sizeof(addr));
    addr->sin_family = AF_INET;
    memmove((char*) &(addr->sin_addr.s_addr),
            (char*) backend->h_addr_list[0],
            backend->h_length);
    addr->sin_port = htons(kite->local_port);
  }

  /* Try to connect and set non-blocking. */
  struct timeval to;
  to.tv_sec = pk_state.socket_timeout_s;
  to.tv_usec = 0;
  errno = 0;
  sockfd = -1;
  if ((NULL == addr) ||
      (0 > (sockfd = PKS_socket(AF_INET, SOCK_STREAM, 0))) ||
      PKS_fail(PKS_connect(sockfd, (struct sockaddr*) addr, sizeof(*addr))) ||
      PKS_fail(PKS_setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &to, sizeof(to))) ||
      PKS_fail(PKS_setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *) &to, sizeof(to))) ||
      (0 > set_non_blocking(sockfd)))
  {
    if ((errno != EINPROGRESS) || /* See comment below */
        (sockfd < 0))             /* (NULL == addr) or PKS_socket() failed */
    {
      /* FIXME:
       *   EINPROGRESS shouldn't happen until we swap connect/set_non_blocking
       *   above - which would be desirable from a performance point of view.
       *   Do that later once we've figured out error handling; see below.
       */
      if (sockfd > -1)
        PKS_close(sockfd);

      pkm_yield_stop(fe->manager);
      pkm_free_be_conn(pkb);
      pk_log(PK_LOG_TUNNEL_CONNS, "pkm_connect_be: Failed to connect %s:%d",
                                  kite->local_domain, kite->local_port);
      return NULL;
    }
  }

  /* FIXME: The above should be non-blocking for high volume front-ends,
   *        but that requires more buffering and fancy logic, so we're
   *        lazy for now. Until that works, busy relays should probably
   *        never configure any direct backends of their own.
   *        See also: http://developerweb.net/viewtopic.php?id=3196 */

  pkm_yield_stop(fe->manager);

  chunk->first_chunk = 1;
  pkb->kite = kite;
  pkb->conn.sockfd = sockfd;

  int ev_sock = PKS_EV_FD(sockfd);
  ev_io_init(&(pkb->conn.watch_r), pkm_be_conn_readable_cb, ev_sock, EV_READ);
  ev_io_init(&(pkb->conn.watch_w), pkm_be_conn_writable_cb, ev_sock, EV_WRITE);

  pkb->conn.watch_r.data = pkb->conn.watch_w.data = (void *) pkb;
  ev_io_start(fe->manager->loop, &(pkb->conn.watch_r));
  ev_io_start(fe->manager->loop, &(pkb->conn.watch_w));

  pkb->conn.status &= ~CONN_STATUS_CHANGING;  /* Change complete */
  PKS_STATE(pk_state.live_streams += 1);

  return pkb;
}

static ssize_t pkm_write_chunked(struct pk_tunnel* fe,
                                 struct pk_backend_conn* pkb,
                                 ssize_t length, char* data)
{
  ssize_t overhead = 0;
  struct pk_conn* pkc = &(fe->conn);

  PK_TRACE_FUNCTION;
  /* FIXME: Better error handling */

  /* Make sure there is space in our output buffer for the header. */
  overhead = pk_reply_overhead(pkb->sid, length);
  if (PKC_OUT_FREE(*pkc) < overhead)
    if (0 > pkc_flush(pkc, NULL, 0, BLOCKING_FLUSH, "pkm_write_chunked"))
      return -1;

  /* Write the chunk header to the output buffer */
  pkc->out_buffer_pos += pk_format_reply(PKC_OUT(*pkc), pkb->sid, length, NULL);

  /* Write the data (will pick up the header automatically) */
  return pkc_write(pkc, data, length);
}

static int pkm_update_io(
  struct pk_tunnel* fe,
  struct pk_backend_conn* pkb,
  int recursion)
{
  int i;
  int bytes;
  int loglevel, loglevelclose;
  char buffer[1024];
  int eof = 0;
  int flows = 2;
  struct pk_conn* pkc;
  struct pk_manager* pkm = fe->manager;
  flow_op tunnel_flow_op = FLOW_OP_NONE;

  PK_TRACE_FUNCTION;

  recursion += 1;
  if (recursion > 3) {
    pk_log(PK_LOG_ERROR, "BUG: Too much pkm_update_io recursion!");
    return 0;
  }

  if (pkb != NULL) {
    pkc = &(pkb->conn);
    loglevel = loglevelclose = PK_LOG_BE_DATA;
  }
  else {
    pkc = &(fe->conn);
    loglevel = PK_LOG_TUNNEL_DATA;
    loglevelclose =  PK_LOG_TUNNEL_DATA | PK_LOG_MANAGER_INFO;
  }
  if (0 >= pkc->sockfd)
    return 0;

  if (pkb != NULL) {
    pkc_report_progress(&(pkb->conn), pkb->sid, &(pkb->tunnel->conn));
    if (pkc->read_kb > pkc->sent_kb + pkc->send_window_kb)
      pkm_flow_control_conn(pkc, CONN_DEST_BLOCKED);
    else
      pkm_flow_control_conn(pkc, CONN_DEST_UNBLOCKED);
  }

  if (pkc->status & (CONN_STATUS_CLS_READ|CONN_STATUS_END_READ)) {
    if (pkc->status & CONN_STATUS_END_READ) {
      /* They know, we know, we know they know... */
    }
    else {
      /* Other end doesn't know we're closing, send an EOF. */
      eof |= PK_EOF_READ;
    }
    /* Not going to read anymore, stop listening. */
    pkc->status |= (CONN_STATUS_END_READ | CONN_STATUS_CLS_READ);
    ev_io_stop(pkm->loop, &(pkc->watch_r));
    PKS_shutdown(pkc->sockfd, SHUT_RD);

    flows -= 1;
    pk_log(loglevel, "%d: Closed for reading.", pkc->sockfd);
    if (pkb == NULL) {
      /* Frontend: If we can't read the tunnel, we can't write it either. */
      pkc->status |= CONN_STATUS_CLS_WRITE;
    }
  }
  else if ((pkc->status & CONN_STATUS_BLOCKED) &&
           !(pkc->status & CONN_STATUS_WANT_READ)) {
    pk_log(loglevel, "%d: Throttled input.", pkc->sockfd);
    ev_io_stop(pkm->loop, &(pkc->watch_r));
  }
  else {
    pk_log(loglevel, "%d: Watching for input.", pkc->sockfd);
    ev_io_start(pkm->loop, &(pkc->watch_r));
  }

  if (pkc->status & CONN_STATUS_CLS_WRITE) {
    /* Writing is impossible, discard buffer and shutdown. */
    if (pkc->status & CONN_STATUS_END_WRITE) {
      /* They know, we know, we know they know... */
    }
    else {
      /* Other end doesn't know we're closing, send an EOF. */
      eof |= PK_EOF_WRITE;
    }
    pkc->status |= (CONN_STATUS_END_WRITE | CONN_STATUS_CLS_WRITE);
    pkc->out_buffer_pos = 0;
    PKS_shutdown(pkc->sockfd, SHUT_WR);
    ev_io_stop(pkm->loop, &(pkc->watch_w));
    flows -= 1;
    pk_log(loglevel, "%d: Closed for writing.", pkc->sockfd);
  }
  else if ((0 < pkc->out_buffer_pos) ||
           (pkc->status & CONN_STATUS_WANT_WRITE)) {
    /* Blocked: activate write listener */
    ev_io_start(pkm->loop, &(pkc->watch_w));
    pk_log(loglevel, "%d: Blocked output!", pkc->sockfd);
    if (NULL == pkb) tunnel_flow_op = CONN_TUNNEL_BLOCKED;
  }
  else {
    if (pkc->status & CONN_STATUS_END_WRITE) {
      /* Not blocked, no more data (sources closed), shutdown. */
      pkc->status |= CONN_STATUS_CLS_WRITE;
      PKS_shutdown(pkc->sockfd, SHUT_WR);
      flows -= 1;
      pk_log(loglevel, "%d: Closed for writing (remote).", pkc->sockfd);
    }
    else {
      pk_log(loglevel, "%d: Waiting for output.", pkc->sockfd);
      if (NULL == pkb) tunnel_flow_op = CONN_TUNNEL_UNBLOCKED;
    }
    ev_io_stop(pkm->loop, &(pkc->watch_w));
  }

  if (eof) {
    if (pkb != NULL) {
      /* This is a backend conn, forcibly send EOF over tunnel. */
      bytes = pk_format_eof(buffer, pkb->sid, eof);
      pkc_write(&(fe->conn), buffer, bytes);
      pkc_flush(&(fe->conn), NULL, 0, BLOCKING_FLUSH, "tunnel");
      pk_log(loglevel, "%d: Sent and flushed EOF (0x%x)", pkc->sockfd, eof);
    }
    else {
      /* This is a tunnel, send EOF to all backends, mark for reconnection. */
      /* FIXME: This is O(n), but rare.  Maybe OK? */
      pk_log(loglevel, "%d: Shutting down tunnel.", pkc->sockfd);
      for (i = 0; i < pkm->be_conn_max; i++) {
        pkb = (pkm->be_conns+i);
        if ((pkb->tunnel == fe) && (pkb->conn.status != CONN_STATUS_UNKNOWN)) {
          pkb->conn.status |= (CONN_STATUS_END_WRITE|CONN_STATUS_END_READ);
          pkm_update_io(fe, pkb, recursion);
        }
      }
      tunnel_flow_op = FLOW_OP_NONE;
      pkb = NULL;
    }
  }

  if (0 >= flows) {
    /* Nothing to read or write, close and clean up. */
    if (0 <= pkc->sockfd) {
      pk_log(loglevelclose, "%d: Disconnected, closing.", pkc->sockfd);
      PKS_close(pkc->sockfd);
    }
    if (pkb != NULL) {
      pkm_free_be_conn(pkb);
      PKS_STATE(pk_state.live_streams -= 1);
    }
    else {
      /* FIXME: Is this the right way to clean up dead tunnels? */
      PKS_STATE(pk_state.live_tunnels -= 1;
                pkm->status = PK_STATUS_PROBLEMS);
      pkc_reset_conn(&(fe->conn), CONN_STATUS_ALLOCATED);
      fe->request_count = 0;
      if (pk_state.live_tunnels < 1) {
        pkm->next_tick = 1 + pkm->housekeeping_interval_min;
      }
      pkm_tick(pkm);
    }
    pkc->sockfd = -1;
  }
  else if (tunnel_flow_op != FLOW_OP_NONE) {
    pkm_flow_control_tunnel(fe, tunnel_flow_op, recursion);
  }

  pkm_yield(pkm);
  return flows;
}

static void pkm_flow_control_tunnel(struct pk_tunnel* fe, flow_op op, int rec)
{
  int i;
  struct pk_backend_conn* pkb;
  struct pk_manager* pkm = fe->manager;

  PK_TRACE_FUNCTION;

  /* FIXME: This is inefficient.
   *   1) we should only evaluate tunnels that are blocked / not blocked
   *   2) we should only evaluate backends linked to this tunnel
   */

  for (i = 0; i < pkm->be_conn_max; i++) {
    pkb = (pkm->be_conns + i);
    unsigned int old_status = pkb->conn.status;
    if ((pkb->tunnel == fe) && (pkb->conn.sockfd >= 0)) {
      if (pkb->conn.status & CONN_STATUS_TNL_BLOCKED) {
        if (op == CONN_TUNNEL_UNBLOCKED) {
          pk_log(PK_LOG_TUNNEL_DATA, "%d: Tunnel unblocked.", pkb->conn.sockfd);
          pkb->conn.status &= ~CONN_STATUS_TNL_BLOCKED;
        }
      }
      else if (op == CONN_TUNNEL_BLOCKED) {
        pk_log(PK_LOG_TUNNEL_DATA, "%d: Tunnel blocked.", pkb->conn.sockfd);
        pkb->conn.status |= CONN_STATUS_TNL_BLOCKED;
      }
    }
    if (old_status != pkb->conn.status) {
      if (op == CONN_TUNNEL_BLOCKED) {
        /* Oops, writing too fast! Reduce window size by about a third */
        pkb->conn.send_window_kb -= (1 + pkb->conn.send_window_kb / 3);
        if (pkb->conn.send_window_kb < CONN_WINDOW_SIZE_KB_MINIMUM)
          pkb->conn.send_window_kb = CONN_WINDOW_SIZE_KB_MINIMUM;
      }
      pkm_update_io(fe, pkb, rec);
    }
  }
}

static void pkm_flow_control_conn(struct pk_conn* pkc, flow_op op)
{
  PK_TRACE_FUNCTION;
  if (pkc->status & CONN_STATUS_DST_BLOCKED) {
    if (op == CONN_DEST_UNBLOCKED) {
      pk_log(PK_LOG_BE_DATA, "%d: Destination unblocked.", pkc->sockfd);
      pkc->status &= ~CONN_STATUS_DST_BLOCKED;
    }
  }
  else
    if (op == CONN_DEST_BLOCKED) {
      pkc->status |= CONN_STATUS_DST_BLOCKED;
      pk_log(PK_LOG_BE_DATA,
        "%d: Destination blocked; read_kb=%d; sent_kb=%d; send_window_kb=%d",
        pkc->sockfd, pkc->read_kb, pkc->sent_kb, pkc->send_window_kb);
    }
}

static void pkm_parse_eof(struct pk_backend_conn* pkb, char *eof)
{
  struct pk_conn* pkc = &(pkb->conn);
  int eof_read = 0;
  int eof_write = 0;
  char *p;

  PK_TRACE_FUNCTION;

  /* Figure out what kind of EOF this is */
  for (p = eof; (p != NULL) && (*p != '\0'); p++) {
    if (*p == 'R' || *p == 'r')
      eof_read = 1;
    else if (*p == 'W' || *p == 'w')
      eof_write = 1;
  }
  if (!eof_write && !eof_read) /* Legacy EOF support */
    eof_write = eof_read = 1;

  /* If we cannot write anymore, there is no point in reading. */
  if (eof_write) {
    pkc->status |= CONN_STATUS_END_READ;
  }

  /* If we cannot read anymore, we won't be writing either. */
  if (eof_read) {
    pkc->status |= CONN_STATUS_END_WRITE;
  }
}

static void pkm_tunnel_readable_cb(EV_P_ ev_io *w, int revents)
{
  int rv, read_bytes;
  struct pk_tunnel* fe = (struct pk_tunnel*) w->data;
  PK_TRACE_FUNCTION;

  fe->conn.status &= ~CONN_STATUS_WANT_READ;
  do {
    if (0 < (read_bytes = pkc_read(&(fe->conn)))) {
      if (0 > (rv = pk_parser_parse(fe->parser,
                                    fe->conn.in_buffer_pos,
                                    (char *) fe->conn.in_buffer)))
      {
        /* Parse failed: remote is borked: should kill this conn. */
        fe->conn.status |= CONN_STATUS_BROKEN;
        pk_log(PK_LOG_TUNNEL_HEADERS,
               "pkm_tunnel_readable_cb(): parse error = %d", rv);
        pk_dump_state(fe->manager);
        if (pk_state.log_mask & PK_LOG_TUNNEL_DATA) {
          int bytes = fe->conn.in_buffer_pos;
          pk_log_raw_data(PK_LOG_TUNNEL_DATA, "data",
                          fe->conn.sockfd, fe->conn.in_buffer, bytes);
        }
      }
    }
    /* pk_parser_parse always processes the entire buffer. */
    fe->conn.in_buffer_pos = 0;

  /* Keep processing while OpenSSL has more data buffered and waiting */
  } while ((read_bytes > 0) && (pkc_pending(&(fe->conn)) > 0));

  PK_CHECK_MEMORY_CANARIES;
  pkm_update_io(fe, NULL, 0);
  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) revents;
}

static void pkm_tunnel_writable_cb(EV_P_ ev_io* w, int revents)
{
  struct pk_tunnel* fe = (struct pk_tunnel*) w->data;

  /* This is necessary for SSL handshakes and the like. */
  if (fe->conn.status & CONN_STATUS_WANT_WRITE) {
    fe->conn.status &= ~CONN_STATUS_WANT_WRITE;
    if (0 == fe->conn.out_buffer_pos)
      pkc_raw_write(&(fe->conn), NULL, 0);
  }
  pkc_flush(&(fe->conn), NULL, 0, NON_BLOCKING_FLUSH, "tunnel");
  PK_CHECK_MEMORY_CANARIES;

  pkm_update_io(fe, NULL, 0);
  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) revents;
}

static void pkm_be_conn_readable_cb(EV_P_ ev_io* w, int revents)
{
  struct pk_backend_conn* pkb = (struct pk_backend_conn*) w->data;
  size_t bytes;

  PK_TRACE_FUNCTION;

  if (pkb->conn.status & CONN_STATUS_TNL_BLOCKED) {
    pk_log(PK_LOG_BE_DATA, ">%5.5s> BLOCKED: Tunnel is blocked.", pkb->sid);
  }
  else {
    pkb->conn.status &= ~CONN_STATUS_WANT_READ;
    bytes = pkc_read(&(pkb->conn));
    if ((0 < bytes) &&
        (0 <= pkm_write_chunked(pkb->tunnel, pkb,
                                pkb->conn.in_buffer_pos,
                                pkb->conn.in_buffer))) {
      pkb->conn.in_buffer_pos = 0;
      pk_log(PK_LOG_BE_DATA, ">%5.5s> DATA: %d bytes", pkb->sid, bytes);
    }
    else if (bytes == 0) {
      pk_log(PK_LOG_BE_DATA, ">%5.5s> EOF: read", pkb->sid);
    }
  }

  PK_CHECK_MEMORY_CANARIES;
  pkm_update_io(pkb->tunnel, pkb, 0);
  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) revents;
}

static void pkm_be_conn_writable_cb(EV_P_ ev_io* w, int revents)
{
  struct pk_backend_conn* pkb = (struct pk_backend_conn*) w->data;

  PK_TRACE_FUNCTION;

  /* This is necessary for SSL handshakes and the like. */
  if (pkb->conn.status & CONN_STATUS_WANT_WRITE) {
    pkb->conn.status &= ~CONN_STATUS_WANT_WRITE;
    if (0 == pkb->conn.out_buffer_pos)
      pkc_raw_write(&(pkb->conn), NULL, 0);
  }

  pkc_flush(&(pkb->conn), NULL, 0, NON_BLOCKING_FLUSH, "be_conn");
  if (pkb->conn.out_buffer_pos == 0)
  {
    pk_log(PK_LOG_BE_DATA, "Flushed: %s:%d (done)",
           pkb->kite->local_domain, pkb->kite->local_port);
  }
  else {
    pk_log(PK_LOG_BE_DATA, "Flushed: %s:%d\n",
           pkb->kite->local_domain, pkb->kite->local_port);
  }
  PK_CHECK_MEMORY_CANARIES;
  pkm_update_io(pkb->tunnel, pkb, 0);
  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) revents;
}

static void pkm_listener_cb(EV_P_ ev_io* w, int revents)
{
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int client_fd;
  struct pk_backend_conn* pkl = (struct pk_backend_conn*) w->data;

  ev_io_stop(EV_A_ w);
  while (0 <= (client_fd = PKS_accept(pkl->conn.sockfd,
                                      (struct sockaddr*) &client_addr,
                                      &client_len))) {
    if (pkl->callback_func != NULL) {
      (pkl->callback_func)(client_fd, pkl->callback_data);
    }
    else {
      PKS_close(client_fd);
    }
  }
  ev_io_start(EV_A_ w);

  (void) loop;
  (void) revents;
}

int pkm_reconnect_all(struct pk_manager* pkm, int ignore_errors) {
  struct pk_kite_request *kite_r;
  unsigned int status;
  int i, j, reconnect, tried, connected;

  PK_TRACE_FUNCTION;
  tried = connected = 0;

  /* The lock must already be held, or we may have nasty bugs. */
  assert(0 != pkm_reconfig_start(pkm));

  /* Loop through all configured kites:
   *   - if missing a desired front-end, tear down tunnels and reconnect.
   */
  pkm_block(pkm);
  PK_TUNNEL_ITER(pkm, fe) {
    PK_ADD_MEMORY_CANARY(fe);

    if (fe->fe_hostname == NULL || fe->ai.ai_addr == NULL) continue;
    if (!(fe->conn.status & (FE_STATUS_WANTED|FE_STATUS_IN_DNS))) continue;

    /* Ignore tunnels that are changing state, but log that we did so since
     * tunnels stuck in a long-running change may indicate other problems. */
    if (fe->conn.status & CONN_STATUS_CHANGING) {
      pk_log(PK_LOG_MANAGER_DEBUG,
             "%d: pkm_reconnect_all: Ignored, changes still in flight",
             fe->conn.sockfd);
      continue;
    }

    /* Ignore this relay if we already have a live connection to the same
     * one using a different IP (recognized by identical UUIDs).
     */
    if (fe->fe_uuid != NULL) {
      struct pk_tunnel* live = NULL;
      PK_TUNNEL_ITER(pkm, other_fe) {
        if ((fe != other_fe) &&
            (other_fe->fe_uuid != NULL) &&
            (other_fe->conn.sockfd > -1) &&
            (0 == strcmp(other_fe->fe_uuid, fe->fe_uuid)))
          live = other_fe;
      }
      if (live != NULL) {
        pk_log(PK_LOG_MANAGER_DEBUG,
               "Not making second connection to %s, %d is live.",
               fe->fe_uuid, live->conn.sockfd);
        continue;
      }
    }

    if ((fe->requests == NULL) ||
        (fe->request_count != pkm->kite_max) ||
        (fe->conn.sockfd < 0)) {
      /* Reset the list of kites we will request from this relay. */
      fe->request_count = pkm->kite_max;
      memset(fe->requests, 0, pkm->kite_max * sizeof(struct pk_kite_request));
      for (kite_r = fe->requests, j = 0; j < pkm->kite_max; j++, kite_r++) {
        kite_r->kite = (pkm->kites + j);
        kite_r->status = PK_KITE_UNKNOWN;
      }
    }

    reconnect = 0;
    for (kite_r = fe->requests, j = 0; j < pkm->kite_max; j++, kite_r++) {
      if (kite_r->status == PK_KITE_UNKNOWN) reconnect++;
    }

    if (reconnect) {
      tried++;
      PKS_STATE(pkm->status = PK_STATUS_CONNECTING);
      if (0 <= fe->conn.sockfd) {
        ev_io_stop(pkm->loop, &(fe->conn.watch_r));
        ev_io_stop(pkm->loop, &(fe->conn.watch_w));
        PKS_close(fe->conn.sockfd);
        fe->conn.sockfd = -1;
      }
      status = fe->conn.status;
      pkc_reset_conn(&(fe->conn), 0);
      /* Note: Do not set CONN_STATUS_CHANGING in pkc_reset_conn, that
       *       could suppress errors. */
      fe->conn.status = (CONN_STATUS_CHANGING |
                         CONN_STATUS_ALLOCATED |
                         (status & FE_STATUS_BITS));

      /* Unblock the event loop and blockers while we attempt to connect. */
      pkm_reconfig_stop(pkm);
      pkm_unblock(pkm);

      if ((0 <= pk_connect_ai(&(fe->conn), &(fe->ai), 0,
                              fe->request_count, fe->requests,
                              (fe->fe_session), fe->manager->ssl_ctx,
                              fe->fe_hostname)) &&
          (0 < set_non_blocking(fe->conn.sockfd))) {
        pk_log(PK_LOG_MANAGER_INFO, "%d: Connected!", fe->conn.sockfd);

        /* Note: order matters here, otherwise we can end up deadlocked
         *       due to the pkm_block() at the top of this function. */
        pkm_reconfig_blocking_start(pkm);
        pkm_block(pkm);

        pk_parser_reset(fe->parser);

        int ev_sock = PKS_EV_FD(fe->conn.sockfd);
        ev_io_init(&(fe->conn.watch_r),
                   pkm_tunnel_readable_cb, ev_sock, EV_READ);
        ev_io_init(&(fe->conn.watch_w),
                   pkm_tunnel_writable_cb, ev_sock, EV_WRITE);

        fe->conn.watch_r.data = fe->conn.watch_w.data = (void *) fe;
        ev_io_start(pkm->loop, &(fe->conn.watch_r));

        PKS_STATE(pk_state.live_tunnels += 1);
        fe->conn.status &= ~CONN_STATUS_CHANGING;  /* Change complete */
        fe->error_count = 0;
        connected++;
      }
      else {
        /* Note: order matters here, otherwise we can end up deadlocked
         *       due to the pkm_block() at the top of this function. */
        pkm_reconfig_blocking_start(pkm);
        pkm_block(pkm);

        /* FIXME: Is this the right behavior? */
        pk_log(PK_LOG_MANAGER_INFO, "Connect failed: %d", fe->conn.sockfd);
        fe->request_count = 0;

        if (!ignore_errors && fe->error_count < 999)
          fe->error_count += 1;

        status = fe->conn.status;
        if (pk_error == ERR_CONNECT_REJECTED) {
          status |= FE_STATUS_REJECTED;
          PKS_STATE(pkm->status = PK_STATUS_REJECTED);
        }
        else if (pk_error == ERR_CONNECT_DUPLICATE) {
          status |= FE_STATUS_LAME;
          tried -= 1;
        }
        fe->conn.status &= ~CONN_STATUS_CHANGING;  /* Change failed */
        pkc_reset_conn(&(fe->conn), 0);
        fe->conn.status = (CONN_STATUS_ALLOCATED | (status & FE_STATUS_BITS));

        pk_perror("pkmanager.c");
      }
    }
  }
  PK_CHECK_MEMORY_CANARIES;
  pkm_unblock(pkm);
  return (tried - connected);
}

int pkm_disconnect_unused(struct pk_manager* pkm) {
  struct pk_backend_conn* pkb;
  char buffer[1025];
  unsigned int status;
  int i, j, pass, disconnect, disconnected, live, ping_window;

  PK_TRACE_FUNCTION;
  ping_window = pk_time() - 4*pkm->housekeeping_interval_min;

  /* The lock must already be held, or we may have nasty bugs. */
  assert(0 != pkm_reconfig_start(pkm));

  /* Loop through all configured tunnels:
   *   1st pass: just count how many we would like to disconnect
   *   2nd pass: actually disconnect idle and unwanted tunnels
   */
  pkm_block(pkm);
  for (pass = 1; pass <= 2; pass++) {
    live = disconnect = disconnected = 0;
    PK_TUNNEL_ITER(pkm, fe) {
      if (fe->fe_hostname == NULL) continue;

      /* Check this before incrementing the live count, as we're not sure
       * this is actually live. */
      if (fe->conn.status & CONN_STATUS_CHANGING) continue;
      if (fe->conn.sockfd <= 0) continue;
      live += 1;

      if (fe->conn.status & (FE_STATUS_WANTED|FE_STATUS_IN_DNS)) continue;

      /* If we haven't been sending/seeing pings, then that means there
       * is recent traffic, so disconnecting would be bad. Note, we cannot
       * check conn.activity directly because pings reset that! */
      if (fe->last_ping < ping_window) continue;

      /* Ignore tunnels that are changing state, but log that we did so since
       * tunnels stuck in a long-running change may indicate other problems. */
      if (fe->conn.status & CONN_STATUS_CHANGING) {
        pk_log(PK_LOG_MANAGER_DEBUG,
               "%d: pkm_disconnect_unused: Ignored, changes still in flight",
               fe->conn.sockfd);
        continue;
      }

      /* Check if there are any live streams... */
      disconnect++;
      for (j = 0; j < pkm->be_conn_max; j++) {
        pkb = (pkm->be_conns + j);
        if (pkb->conn.sockfd > 0 && pkb->tunnel == fe) {
          disconnect--;
          break;
        }
      }

      if ((2 == pass) && disconnect) {
        pk_log(PK_LOG_MANAGER_INFO, "Disconnecting: %s",
                                    in_addr_to_str(fe->ai.ai_addr, buffer, 1024));

        ev_io_stop(pkm->loop, &(fe->conn.watch_r));
        ev_io_stop(pkm->loop, &(fe->conn.watch_w));
        PKS_close(fe->conn.sockfd);
        fe->conn.sockfd = -1;
        disconnected += 1;

        status = fe->conn.status;
        pkc_reset_conn(&(fe->conn), 0);
        fe->conn.status = (CONN_STATUS_ALLOCATED | (status & FE_STATUS_BITS));
        fe->request_count = 0;

        disconnect = 0; /* Reset, to prevent cascading. */
      }
    }

    /* If we're about to go disconnecting all our live tunnels, abort. */
    if (disconnect >= live) break;
  }
  PK_CHECK_MEMORY_CANARIES;
  PKS_STATE(pk_state.live_tunnels = (live - disconnected));
  pkm_unblock(pkm);
  return disconnected;
}


void pkm_tick(struct pk_manager* pkm)
{
  ev_async_send(pkm->loop, &(pkm->tick));
}
static void pkm_tick_cb(EV_P_ ev_async* w, int revents)
{
  int i, pingsize;
  char ping[PK_REJECT_MAXSIZE];
  struct pk_manager* pkm = (struct pk_manager*) w->data;
  time_t next_tick = pkm->next_tick;
  time_t max_tick;
  time_t now = pk_time();
  time_t increment = (next_tick / 3);
  time_t inactive = now - PK_HOUSEKEEPING_INTERVAL_MAX_MIN;

  PK_TRACE_FUNCTION;
  pkw_pet_watchdog();

  PK_HOOK(PK_HOOK_TICK, now, pkm, NULL);

  /* First, we look at the state of the world and schedule (or cancel)
   * our next tick. */
  if (pkm->enable_timer || (pkm->status != PK_STATUS_NO_NETWORK &&
                            pkm->status != PK_STATUS_REJECTED &&
                            pkm->status != PK_STATUS_FLYING))
  {
    pkm->timer.repeat = pkm->next_tick;
    ev_timer_again(pkm->loop, &(pkm->timer));
    pk_log(PK_LOG_MANAGER_INFO,
           "Tick!  [repeating=%s, next=%d, status=%d, tunnels=%d, v=%s]",
           pkm->enable_timer ? "yes" : "no", pkm->next_tick,
           pkm->status, pk_state.live_tunnels, PK_VERSION);

    /* We slow down exponentially by default... */
    next_tick += increment;
    max_tick = pkm->housekeeping_interval_max + pkm->interval_fudge_factor;

    /* Fallback is tuned for normal operation: if the relay rejected
       us that is not normal anymore and we'll happily go idle until it
       is time to re-check the state of the world. */
    if (pkm->status == PK_STATUS_REJECTED)
      max_tick += pkm->check_world_interval;

    /* Similarly, if there's no network AND we are relying on the built-in
       timer and not external app logic, then retry more frequently. Power
       constrained apps shouldn't be using the timer anyway! */
    if (pkm->status == PK_STATUS_NO_NETWORK)
      max_tick = (PK_HOUSEKEEPING_INTERVAL_MIN +
                  PK_HOUSEKEEPING_INTERVAL_MAX_MIN) / 2;

    if (next_tick > max_tick)
      next_tick = max_tick;
  }
  else {
    ev_timer_stop(pkm->loop, &(pkm->timer));
    pk_log(PK_LOG_MANAGER_INFO,
           "Tick!  [repeating=%s, stopped, status=%d, v=%s]",
           pkm->enable_timer ? "yes" : "no", pkm->status, PK_VERSION);
    /* Reset interval. */
    next_tick = 1 + pkm->housekeeping_interval_min;
  }

  /* Loop through all configured tunnels, unless another reconfiguration
   * process is already running. */
  pingsize = 0;
  if (0 == pkm_reconfig_start(pkm)) {
    PK_TUNNEL_ITER(pkm, fe) {
      if ((fe->conn.sockfd >= 0) && !(fe->conn.status & CONN_STATUS_CHANGING)) {
        /* If dead, shut 'em down. */
        if (fe->conn.activity < fe->last_ping - 4*pkm->housekeeping_interval_min)
        {
          pk_log(PK_LOG_TUNNEL_DATA, "%d: Idle, shutting down.", fe->conn.sockfd);
          fe->conn.status |= CONN_STATUS_BROKEN;
          pkm_update_io(fe, NULL, 0);
        }
        /* If idle, send a ping. */
        else if (fe->conn.activity < inactive) {
          if (pingsize == 0) pingsize = pk_format_ping(ping);
          fe->last_ping = now;
          pkc_write(&(fe->conn), ping, pingsize);
          pk_log(PK_LOG_TUNNEL_DATA,
              "%d: Sent PING (idle=%ds>%ds)",
              fe->conn.sockfd, now - fe->conn.activity, now - inactive);
          next_tick = 1 + pkm->housekeeping_interval_min;
        }
      }
    }
    pkm_reconfig_stop(pkm);
  }
  pkm_yield_start(pkm);

  /* Finally, trigger the tunnel check on the blocking thread. */
  if (pkm->last_world_update + pkm->check_world_interval < pk_time()) {
    pkb_add_job(&(pkm->blocking_jobs), PK_CHECK_WORLD, 0, pkm);
    /* After checking the state of the world, we are a bit more aggressive
     * about following up on things, reset the fallback. */
    next_tick = 1 + pkm->housekeeping_interval_min;
  }
  else {
    pkb_add_job(&(pkm->blocking_jobs), PK_CHECK_FRONTENDS, 0, pkm);
  }

  PK_CHECK_MEMORY_CANARIES;

  pkm->next_tick = next_tick;
  pkm_yield_stop(pkm);

  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) revents;
}

static void pkm_timer_cb(EV_P_ ev_timer* w, int revents)
{
  struct pk_manager* pkm = (struct pk_manager*) w->data;
  pkm_tick(pkm);
  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) revents;
}
static void pkm_reset_timer(struct pk_manager* pkm) {
  ev_timer_set(&(pkm->timer), 0.0, 1 + pkm->housekeeping_interval_min);
  ev_timer_start(pkm->loop, &(pkm->timer));
  pkm->next_tick = 1 + pkm->housekeeping_interval_min;
}
void pkm_set_timer_enabled(struct pk_manager* pkm, int enabled) {
  pkm_block(pkm);
  pkm->enable_timer = (enabled > 0);
  if (pkm->enable_timer) {
    pkm_reset_timer(pkm);
  }
  else ev_timer_stop(pkm->loop, &(pkm->timer));
  pkm_unblock(pkm);
}

static void pkm_reset_manager(struct pk_manager* pkm) {
  struct pk_conn* pkc;

  PK_TRACE_FUNCTION;

  PK_KITE_ITER(pkm, kite) {
    pk_reset_pagekite(kite);
  }
  PK_TUNNEL_ITER(pkm, fe) {
    pkc = &(fe->conn);
    if (pkc->status != CONN_STATUS_UNKNOWN) {
      ev_io_stop(pkm->loop, &(pkc->watch_r));
      ev_io_stop(pkm->loop, &(pkc->watch_w));
      pkc->status = CONN_STATUS_ALLOCATED;
      pkc_reset_conn(pkc, CONN_STATUS_ALLOCATED);
    }
  }
  for (int i = 0; i < pkm->be_conn_max; i++) {
    pkc = &((pkm->be_conns+i)->conn);
    if (pkc->status != CONN_STATUS_UNKNOWN) {
      ev_io_stop(pkm->loop, &(pkc->watch_r));
      ev_io_stop(pkm->loop, &(pkc->watch_w));
      pkc->status = 0;  /* Avoid bogus change detection in reset_conn */
      pkc_reset_conn(pkc, 0);
    }
  }
  ev_async_stop(pkm->loop, &(pkm->quit));
}

static struct pk_pagekite* pkm_find_kite(struct pk_manager* pkm,
                                         const char* protocol,
                                         const char* domain,
                                         int port)
{
  struct pk_pagekite* found;

  PK_TRACE_FUNCTION;

  /* NOTE: This is O(N), which we deem OK since backends should rarely
   *       have a huge number of kites configured. */
  found = NULL;
  PK_KITE_ITER(pkm, kite) {
    if (kite->protocol[0] != '\0') {
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
  char *pp;
  int which = 0;
  struct pk_pagekite* kite = NULL;

  PK_TRACE_FUNCTION;

  if ((strcasecmp(protocol, "raw") == 0) && (public_port < 1))
    return pk_err_null(ERR_RAW_NEEDS_PUBPORT);

  /* FIXME: This is O(N), we'll need a nicer data structure for tunnels */
  PK_KITE_ITER(pkm, k) {
    which++;
    if (k->protocol[0] == '\0') {
      kite = k;
      break;
    }
  }
  if (which >= pkm->kite_max)
    return pk_err_null(ERR_NO_MORE_KITES);

  if (kite == NULL)
    return pk_err_null(ERR_NO_KITE);

  strncpyz(kite->protocol, protocol, PK_PROTOCOL_LENGTH);
  strncpyz(kite->auth_secret, auth_secret, PK_SECRET_LENGTH);
  strncpyz(kite->public_domain, public_domain, PK_DOMAIN_LENGTH);
  kite->public_port = public_port;
  kite->local_domain[0] = '\0';
  if (local_domain != NULL)
    strncpyz(kite->local_domain, local_domain, PK_DOMAIN_LENGTH);
  kite->local_port = local_port;

  /* Allow the public port to be specified as part of the protocol */
  if ((0 == public_port) && (NULL != (pp = strchr(kite->protocol, '-')))) {
    *pp++ = '\0';
    sscanf(pp, "%d", &(kite->public_port));
  }

  PK_CHECK_MEMORY_CANARIES;
  return kite;
}

int pkm_add_listener(struct pk_manager* pkm,
                     const char* hostname,
                     int port,
                     pagekite_callback_t callback_func,
                     void* callback_data)
{
  int ok, errors, lport, rv;
  struct addrinfo hints;
  struct addrinfo* result;
  struct addrinfo* rp;
  struct pk_backend_conn* pkl = NULL;
  char printip[128], sport[128];

  PK_TRACE_FUNCTION;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  sprintf(sport, "%d", port);

  lport = ok = errors = 0;
  if (0 != (rv = getaddrinfo(hostname, sport, &hints, &result))) {
    pk_log(PK_LOG_BE_CONNS|PK_LOG_ERROR,
           "pkm_add_listener: getaddrinfo(%s, %s) failed:",
           hostname, sport, gai_strerror(rv));
    errors++;
  }
  else {
    for (rp = result; rp != NULL; rp = rp->ai_next) {
      sprintf(sport, "!LSTN:%d", port);

      if (NULL == (pkl = pkm_alloc_be_conn(pkm, NULL, sport))) {
        pk_log(PK_LOG_BE_CONNS|PK_LOG_ERROR,
               "pkm_add_listener: BE alloc failed for %s",
               in_addr_to_str(rp->ai_addr, printip, 128));
        errors++;
      }
      else if ((0 > (lport = pkc_listen(&(pkl->conn), rp, 50 /* FIXME */)) ||
               (0 > set_non_blocking(pkl->conn.sockfd)))) {
        pkl->conn.status &= ~CONN_STATUS_CHANGING;  /* Change failed */
        pkc_reset_conn(&(pkl->conn), 0);
        pk_log(PK_LOG_BE_CONNS|PK_LOG_ERROR,
               "pkm_add_listener: pkc_listen() failed for %s",
               in_addr_to_str(rp->ai_addr, printip, 128));
        errors++;
      }
      else {
        int ev_sock = PKS_EV_FD(pkl->conn.sockfd);
        ev_io_init(&(pkl->conn.watch_r), pkm_listener_cb, ev_sock, EV_READ);
        pkl->conn.watch_r.data = (void *) pkl;
        pkl->callback_func = callback_func;
        pkl->callback_data = callback_data;
        ev_io_start(pkm->loop, &(pkl->conn.watch_r));
        pk_log(PK_LOG_MANAGER_INFO,
               "Listening on %s (port %d, sockfd %d)",
               in_addr_to_str(rp->ai_addr, printip, 128), lport,
               pkl->conn.sockfd);
        pkl->conn.status &= ~CONN_STATUS_CHANGING;  /* Change complete! */
        ok++;
      }
    }
  }
  freeaddrinfo(result);

  PK_CHECK_MEMORY_CANARIES;
  return lport - (errors * 0x10000);
}

int pkm_add_frontend(struct pk_manager* pkm,
                     const char* hostname, int port,
                     int conn_status_flags)
{
  return pkm_lookup_and_add_frontend(pkm, hostname, port,
                                     conn_status_flags, 0);
}

int pkm_lookup_and_add_frontend(struct pk_manager* pkm,
                                const char* hostname, int port,
                                int conn_status_flags,
                                int add_null_record)
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  char printip[128], sport[128];
  int rv, count;

  PK_TRACE_FUNCTION;

  /* Default port is 443 */
  if (port < 1) port = 443;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  sprintf(sport, "%d", port);

  rv = getaddrinfo(hostname, sport, &hints, &result);
  count = 0;
  if (rv == 0) {
    for (rp = result; rp != NULL; rp = rp->ai_next) {
      if (!pk_state.use_ipv4 && (rp->ai_addr->sa_family == AF_INET)) continue;
#ifdef HAVE_IPV6
      if (!pk_state.use_ipv6 && (rp->ai_addr->sa_family == AF_INET6)) continue;
#endif
      if (NULL != pkm_add_frontend_ai(pkm, rp, hostname, port,
                                      conn_status_flags)) {
        pk_log(PK_LOG_MANAGER_DEBUG, "Front-end IP: %s",
               in_addr_to_str(rp->ai_addr, printip, 128));
        count++;
      }
    }
    freeaddrinfo(result);
  }
  else {
    pk_log(PK_LOG_BE_CONNS|PK_LOG_ERROR,
           "pkm_lookup_and_add_frontend: getaddrinfo(%s, %s) failed: %s",
           hostname, sport, gai_strerror(rv));
  }

  if (!count && add_null_record) {
    if (NULL != pkm_add_frontend_ai(pkm, NULL, hostname, port,
                                    conn_status_flags)) {
      pk_log(PK_LOG_MANAGER_DEBUG, "Front-end placeholder: %s", hostname);
      count++;
    }
  }

  PK_CHECK_MEMORY_CANARIES;
  return count;
}

struct pk_tunnel* pkm_add_frontend_ai(struct pk_manager* pkm,
                                      struct addrinfo *ai,
                                      const char* hostname, int port,
                                      int conn_status_flags)
{
  struct pk_tunnel* adding = NULL;

  PK_TRACE_FUNCTION;

  /* Scan the front-end list to see if we already have this IP or,
   * if not, find an available slot.
   */
  PK_TUNNEL_ITER(pkm, fe) {
    if (fe->fe_hostname == NULL) {
      if (adding == NULL) adding = fe;
    }
    else if ((ai != NULL) &&
             (fe->ai.ai_addr != NULL) &&
             (ai->ai_addrlen > 0) &&
             (0 == addrcmp(fe->ai.ai_addr, ai->ai_addr)))
    {
      fe->last_configured = pk_time();
      return NULL;
    }
  }
  if (adding == NULL) return pk_err_null(ERR_NO_MORE_FRONTENDS);

  adding->conn.status = (conn_status_flags | CONN_STATUS_ALLOCATED);
  copy_addrinfo_data(&(adding->ai), ai);
  adding->fe_port = port;
  adding->fe_hostname = strdup(hostname);
  adding->last_ddnsup = 0;
  adding->error_count = 0;
  adding->request_count = 0;
  adding->priority = 0;
  adding->last_configured = pk_time();

  return adding;
}

static unsigned int pkm_sid_shift(char *sid)
{
  return murmur3_32(sid, strlen(sid));
}

struct pk_backend_conn* pkm_alloc_be_conn(struct pk_manager* pkm,
                                          struct pk_tunnel* fe, char *sid)
{
  int i, evicting;
  time_t max_age;
  unsigned int shift;
  struct pk_backend_conn* pkb;
  struct pk_backend_conn* pkb_oldest;

  PK_TRACE_FUNCTION;

  /* FIXME: This becomes a bottleneck when we start to hit our connection
   *        limits; it goes suddenly from O(1) to O(n). We might want to
   *        replace this with a data structure degrades a bit more
   *        gracefully.
   */

  max_age = pk_time();
  pkb_oldest = NULL;
  shift = pkm_sid_shift(sid);
  for (i = 0; i < pkm->be_conn_max; i++) {
    pkb = (pkm->be_conns + ((i + shift) % pkm->be_conn_max));
    if (!(pkb->conn.status & CONN_STATUS_ALLOCATED)) {
      pkc_reset_conn(&(pkb->conn), CONN_STATUS_ALLOCATED);
      pkb->tunnel = fe;
      /* Note: Do not merge this into the pkc_reset_conn call, as that
       *       could suppress errors. We expect whatever allocated this
       *       conn to reset the changing flag whan it's done working. */
      pkb->conn.status |= CONN_STATUS_CHANGING;
      strncpyz(pkb->sid, sid, BE_MAX_SID_SIZE);
      return pkb;
    }
    if ((pkb->conn.activity <= max_age) &&
        !(pkb->conn.status & (CONN_STATUS_CHANGING|CONN_STATUS_LISTENING))) {
      max_age = pkb->conn.activity;
      pkb_oldest = pkb;
    }
  }

  /* If we get this far, we found no empty slots. Let's complain to the
   * log and, if so configured, kick out the oldest idle connection. */
  if (NULL != (pkb = pkb_oldest)) {
    max_age = pk_time(0) - pkb->conn.activity;
    evicting = (pk_state.conn_eviction_idle_s &&
               (pk_state.conn_eviction_idle_s < max_age));

    /* Eviction is an error, as it means we're unable to do our job and
     * may have been configured with too few connection slots. */
    pk_log(evicting ? PK_LOG_MANAGER_ERROR : PK_LOG_MANAGER_DEBUG,
           "Idlest conn: %s (idle %ds, evicting=%d)",
           pkb->sid, max_age, evicting);
    pk_dump_be_conn("be", pkb);

    if (evicting) {
      pkb->conn.status |= (CONN_STATUS_CLS_WRITE|CONN_STATUS_CLS_READ);
      pkm_update_io(pkb->tunnel, pkb, 0);
      pkc_reset_conn(&(pkb->conn), CONN_STATUS_ALLOCATED);
      pkb->tunnel = fe;
      strncpyz(pkb->sid, sid, BE_MAX_SID_SIZE);
      return pkb;
    }
  }

  PK_CHECK_MEMORY_CANARIES;
  return NULL;
}

void pkm_free_be_conn(struct pk_backend_conn* pkb)
{
  pkb->conn.status = CONN_STATUS_UNKNOWN;
}

struct pk_backend_conn* pkm_find_be_conn(struct pk_manager* pkm,
                                         struct pk_tunnel* fe, char* sid)
{
  int i;
  unsigned int shift;
  struct pk_backend_conn* pkb;

  PK_TRACE_FUNCTION;

  shift = pkm_sid_shift(sid) % pkm->be_conn_max;
  for (i = 0; i < pkm->be_conn_max; i++) {
    pkb = (pkm->be_conns + ((i + shift) % pkm->be_conn_max));
    if ((pkb->conn.status & CONN_STATUS_ALLOCATED) &&
        (pkb->tunnel == fe) &&
        (0 == strncmp(pkb->sid, sid, BE_MAX_SID_SIZE))) {
      return pkb;
    }
  }
  return NULL;
}


/*** High level API stuff ****************************************************/

struct pk_manager* pkm_manager_init(struct ev_loop* loop,
                                    int buffer_size, char* buffer,
                                    int kites, int tunnels, int conns,
                                    const char* dynamic_dns_url, SSL_CTX* ctx)
{
  struct pk_manager* pkm;
  int i, malloced;
  unsigned int parse_buffer_bytes;

  PK_TRACE_FUNCTION;
  PK_INIT_MEMORY_CANARIES;

  /* Make sure we have some good random junk to work with, and re-seed
   * rand() if allowed. */
  better_srand(PK_RANDOM_DEFAULT);

#ifdef HAVE_OPENSSL
  pk_ssl_thread_setup();
  pk_log(PK_LOG_TUNNEL_DATA, "SSL_ERROR_ZERO_RETURN = %d", SSL_ERROR_ZERO_RETURN);
  pk_log(PK_LOG_TUNNEL_DATA, "SSL_ERROR_WANT_WRITE = %d", SSL_ERROR_WANT_WRITE);
  pk_log(PK_LOG_TUNNEL_DATA, "SSL_ERROR_WANT_READ = %d", SSL_ERROR_WANT_READ);
  pk_log(PK_LOG_TUNNEL_DATA, "SSL_ERROR_WANT_CONNECT = %d", SSL_ERROR_WANT_CONNECT);
  pk_log(PK_LOG_TUNNEL_DATA, "SSL_ERROR_WANT_ACCEPT = %d", SSL_ERROR_WANT_ACCEPT);
  pk_log(PK_LOG_TUNNEL_DATA, "SSL_ERROR_WANT_X509_LOOKUP = %d", SSL_ERROR_WANT_X509_LOOKUP);
  pk_log(PK_LOG_TUNNEL_DATA, "SSL_ERROR_SYSCALL = %d", SSL_ERROR_SYSCALL);
  pk_log(PK_LOG_TUNNEL_DATA, "SSL_ERROR_SSL = %d", SSL_ERROR_SSL);
#endif

  if (kites < MIN_KITE_ALLOC) kites = MIN_KITE_ALLOC;
  if (tunnels < MIN_FE_ALLOC) tunnels = MIN_FE_ALLOC;
  if (conns < MIN_CONN_ALLOC) conns = MIN_CONN_ALLOC;

  if (buffer == NULL) {
    buffer_size = PK_MANAGER_BUFSIZE(kites, tunnels, conns, PARSER_BYTES_AVG);
    buffer = malloc(buffer_size);
    malloced = 1;
  }
  else {
    i = PK_MANAGER_BUFSIZE(kites, tunnels, conns, PARSER_BYTES_MIN);
    if (buffer_size < i) {
      pk_log(PK_LOG_MANAGER_ERROR,
             "pkm_manager_init: Buffer (%d bytes) too small, need %d.",
             buffer_size, i);
      return pk_err_null(ERR_TOOBIG_PARSERS);
    }
    malloced = 0;
  }
  if (buffer == NULL) {
    pk_log(PK_LOG_MANAGER_ERROR, "pkm_manager_init: No buffer! Malloc failed?");
    return NULL;
  }

  memset(buffer, 0, buffer_size);

  pkm = (struct pk_manager*) buffer;
  pkm->status = PK_STATUS_STARTUP;
  pkm->buffer_bytes_free = buffer_size;
  pkm->was_malloced = malloced;
  if (loop == NULL) {
      loop = ev_loop_new(0);
      pkm->ev_loop_malloced = 1;
  }
  else {
      pkm->ev_loop_malloced = 0;
  }
  pkm->loop = loop;
  pke_init_events(&(pkm->events), 3);  /* FIXME: Magic number */

  PK_ADD_MEMORY_CANARY(pkm);

  pkm->buffer = buffer + sizeof(struct pk_manager);
  pkm->buffer_bytes_free -= sizeof(struct pk_manager);
  pkm->buffer_base = pkm->buffer;
  if (pkm->buffer_bytes_free < 0) return pk_err_null(ERR_TOOBIG_MANAGER);

  /* Allocate space for the kites */
  pkm->buffer_bytes_free -= sizeof(struct pk_pagekite) * kites;
  if (pkm->buffer_bytes_free < 0) return pk_err_null(ERR_TOOBIG_KITES);
  pkm->kites = (struct pk_pagekite *) pkm->buffer;
  pkm->kite_max = kites;
  pkm->buffer += sizeof(struct pk_pagekite) * kites;

  /* Allocate space for the tunnels */
  pkm->buffer_bytes_free -= (sizeof(struct pk_tunnel) * tunnels);
  pkm->buffer_bytes_free -= (sizeof(struct pk_kite_request) * kites * tunnels);
  if (pkm->buffer_bytes_free < 0) return pk_err_null(ERR_TOOBIG_FRONTENDS);
  pkm->tunnels = (struct pk_tunnel *) pkm->buffer;
  pkm->tunnel_max = tunnels;
  pkm->buffer += sizeof(struct pk_tunnel) * tunnels;
  PK_TUNNEL_ITER(pkm, fe) {
    fe->fe_hostname = NULL;
    fe->ai.ai_addr = NULL;
    fe->ai.ai_canonname = NULL;
    fe->requests = (struct pk_kite_request*) pkm->buffer;
#ifdef HAVE_OPENSSL
    fe->conn.ssl = NULL;
#endif
    pkm->buffer += (sizeof(struct pk_kite_request) * kites);
  }

  /* Allocate space for the backend connections */
  pkm->buffer_bytes_free -= sizeof(struct pk_backend_conn) * conns;
  if (pkm->buffer_bytes_free < 0) return pk_err_null(ERR_TOOBIG_BE_CONNS);
  pkm->be_conns = (struct pk_backend_conn *) pkm->buffer;
  pkm->be_conn_max = conns;
  for (i = 0; i < conns; i++) {
    (pkm->be_conns+i)->conn.sockfd = -1;
#ifdef HAVE_OPENSSL
    (pkm->be_conns+i)->conn.ssl = NULL;
#endif
    (pkm->be_conns+i)->conn.status = 0;
    pkc_reset_conn(&(pkm->be_conns+i)->conn, 0);
  }
  pkm->buffer += sizeof(struct pk_backend_conn) * conns;

  /* Allocate space for the blocking job queue */
  pkm->buffer_bytes_free -= sizeof(struct pk_job) * (conns+tunnels);
  if (pkm->buffer_bytes_free < 0) return pk_err_null(ERR_TOOBIG_BE_CONNS);
  pkm->blocking_jobs.pile = (struct pk_job *) pkm->buffer;
  pkm->blocking_jobs.max = (conns+tunnels);
  for (i = 0; i < (conns+tunnels); i++) {
    (pkm->blocking_jobs.pile+i)->job = PK_NO_JOB;
  }
  pkm->buffer += sizeof(struct pk_job) * (conns+tunnels);

  /* Whatever is left, we divide evenly between the protocol parsers... */
  parse_buffer_bytes = (pkm->buffer_bytes_free-1) / tunnels;
  /* ... within reason. */
  if (parse_buffer_bytes > PARSER_BYTES_MAX)
    parse_buffer_bytes = PARSER_BYTES_MAX;
  if (parse_buffer_bytes < PARSER_BYTES_MIN)
    return pk_err_null(ERR_TOOBIG_PARSERS);

  /* Initialize the tunnel structs... */
  for (i = 0; i < tunnels; i++) {
    (pkm->tunnels+i)->manager = pkm;
    (pkm->tunnels+i)->conn.sockfd = -1;
#ifdef HAVE_OPENSSL
    (pkm->tunnels+i)->conn.ssl = NULL;
#endif
    (pkm->tunnels+i)->parser = pk_parser_init(parse_buffer_bytes,
                                                (char *) pkm->buffer,
                                               (pkChunkCallback*) &pkm_chunk_cb,
                                              (pkm->tunnels+i));
    pkm->buffer += parse_buffer_bytes;
    pkm->buffer_bytes_free -= parse_buffer_bytes;
  }

  pkm->fancy_pagekite_net_rejection_url = PK_REJECT_FANCY_URL;
  pkm->enable_watchdog = 0;
  pkm->want_spare_frontends = 0;
  pkm->housekeeping_interval_min = PK_HOUSEKEEPING_INTERVAL_MIN;
  pkm->housekeeping_interval_max = PK_HOUSEKEEPING_INTERVAL_MAX_DEF;
  pkm->check_world_interval = PK_CHECK_WORLD_INTERVAL;
  pkm->interval_fudge_factor = 2 * (rand() % PK_HOUSEKEEPING_INTERVAL_MIN);

  pkm->last_world_update = (time_t) 0;
  pkm->last_dns_update = (time_t) 0;
  pkm->dynamic_dns_url = dynamic_dns_url ? strdup(dynamic_dns_url) : NULL;
  pkm->ssl_ctx = ctx;
  PKS_STATE(pk_state.have_ssl = (ctx != NULL);
            pk_state.force_update = 1)

  /* Set up our event-loop callbacks */
  ev_timer_init(&(pkm->timer), pkm_timer_cb, 0, 0);
  pkm->timer.data = (void *) pkm;
  pkm_reset_timer(pkm);
  pkm->enable_timer = 1;

  /* Let external threads shut us down */
  ev_async_init(&(pkm->quit), pkm_quit_cb);
  ev_async_start(loop, &(pkm->quit));

  /* Let external threads control our "periodic housekeeping" */
  ev_async_init(&(pkm->tick), pkm_tick_cb);
  pkm->tick.data = (void *) pkm;
  ev_async_start(loop, &(pkm->tick));

  /* Prepare blocking thread structures. */
  pthread_mutex_init(&(pkm->config_lock), NULL);
  pthread_mutex_init(&(pkm->intr_lock), NULL);
  pthread_mutex_init(&(pkm->loop_lock), NULL);
  pthread_mutex_init(&(pkm->blocking_jobs.mutex), NULL);
  pthread_cond_init(&(pkm->blocking_jobs.cond), NULL);
  pkm->blocking_jobs.count = 0;

  /* Let external threads interrupt the loop */
  ev_async_init(&(pkm->interrupt), pkm_interrupt_cb);
  pkm->interrupt.data = (void *) pkm;
  ev_async_start(loop, &(pkm->interrupt));

  /* SIGPIPE is boring */
#ifndef _MSC_VER
  signal(SIGPIPE, SIG_IGN);
#endif

  pk_log(PK_LOG_MANAGER_INFO,
         "Initialized %s manager v%s/%s (using %d bytes)",
         pk_state.app_id_long, PK_VERSION, pk_state.app_id_short, buffer_size);
  return pkm;
}

void pkm_manager_free(struct pk_manager* pkm)
{
  pk_ssl_thread_cleanup();

  if (pkm->ev_loop_malloced) {
    ev_loop_destroy(pkm->loop);
  }

  if (pkm->dynamic_dns_url != NULL) {
    free(pkm->dynamic_dns_url);
  }

  PK_TUNNEL_ITER(pkm, fe) {
    if (fe->fe_uuid != NULL) free(fe->fe_uuid);
    if (fe->fe_hostname != NULL) free(fe->fe_hostname);
    free_addrinfo_data(&fe->ai);
    fe->fe_uuid = fe->fe_hostname = NULL;
  }

  if (pkm->was_malloced) {
    free(pkm);
  }
}

void* pkm_run(void *void_pkm)
{
  struct pk_manager* pkm = (struct pk_manager*) void_pkm;

  if (pkm->enable_watchdog) pkw_start_watchdog(pkm);

  pkb_start_blockers(pkm, 2);

  if (PK_HOOK(PK_HOOK_START_EV_LOOP, 0, pkm, NULL)) {
    pthread_mutex_lock(&(pkm->loop_lock));
    ev_loop(pkm->loop, 0);
    pthread_mutex_unlock(&(pkm->loop_lock));
  }

  pke_cancel_all_events(&(pkm->events));
  pkb_stop_blockers(pkm);
  if (pkm->enable_watchdog) pkw_stop_watchdog(pkm);
  pk_log(PK_LOG_MANAGER_DEBUG, "Event loop and workers stopped.");

  PK_HOOK(PK_HOOK_STOPPED, 0, pkm, NULL);
  pke_post_event(&(pkm->events), PK_EV_SHUTDOWN, 0, NULL);

  pkm_reset_manager(pkm);

  return void_pkm;
}

int pkm_run_in_thread(struct pk_manager* pkm)
{
  pk_log(PK_LOG_MANAGER_INFO, "Starting manager in new thread");
  return pthread_create(&(pkm->main_thread), NULL, pkm_run, (void *) pkm);
}

int pkm_wait_thread(struct pk_manager* pkm)
{
  return pthread_join(pkm->main_thread, NULL);
}

int pkm_stop_thread(struct pk_manager* pkm)
{
  pk_log(PK_LOG_MANAGER_DEBUG, "Stopping manager...");
  pkm_quit(pkm);
  return pthread_join(pkm->main_thread, NULL);
}

int pkmanager_test(void)
{
#if PK_TESTS
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
  fprintf(stderr, "pkm_manager_init tests passed (1/3)\n");

  /* Create a real one */
  reset_memory_canaries();
  m = pkm_manager_init(N, PK_MANAGER_MINSIZE, buffer, -1, -1, -1, N, N);
  if (m == NULL) pk_perror("pkmanager.c");
  assert(NULL != m);
  fprintf(stderr, "pkm_manager_init tests passed (2/3)\n");

  /* Create a real one from heap */
  reset_memory_canaries();
  m = pkm_manager_init(NULL, 0, NULL, -1, -1, -1, NULL, NULL);
  assert(NULL != m);

  /* Ensure the right defaults are used. */
  assert(m->tunnel_max == MIN_FE_ALLOC);
  assert(m->kite_max == MIN_KITE_ALLOC);
  assert(m->be_conn_max == MIN_CONN_ALLOC);
  fprintf(stderr, "pkm_manager_init tests passed (3/3)\n");

  /* Ensure memory regions don't overlap */
  memset(m->be_conns,  3, sizeof(struct pk_backend_conn) * m->be_conn_max);
  memset(m->tunnels,   2, sizeof(struct pk_tunnel)       * m->tunnel_max);
  memset(m->kites,     1, sizeof(struct pk_pagekite)     * m->kite_max);
  assert(0 == *((char*) m->buffer));
  assert(1 == *((char*) m->kites));
  assert(2 == *((char*) m->tunnels));
  assert(3 == *((char*) m->be_conns));
  fprintf(stderr, "memory regions do not overlap\n");
  reset_memory_canaries();

  /* Recreate, because those memsets broke things. */
  m = pkm_manager_init(NULL, 0, NULL, -1, -1, -1, NULL, NULL);
  assert(NULL != m);

  /* Test pk_add_job and pk_get_job */
  assert(0 == m->blocking_jobs.count);
  assert(0 < pkb_add_job(&(m->blocking_jobs), PK_QUIT, 0, NULL));
  assert(1 == m->blocking_jobs.count);
  assert(0 < pkb_get_job(&(m->blocking_jobs), &j));
  assert(0 == m->blocking_jobs.count);
  assert(j.job == PK_QUIT);
  fprintf(stderr, "pk_add_job and pk_get_job tests passed\n");

  /* Test pk_add_frontend_ai */
  memset(&ai, 0, sizeof(struct addrinfo));
  for (i = 0; i < MIN_FE_ALLOC; i++)
    assert(NULL != pkm_add_frontend_ai(m, &ai, "woot", 123, 1));
  assert(NULL == pkm_add_frontend_ai(m, &ai, "woot", 123, 1));
  assert(ERR_NO_MORE_FRONTENDS == pk_error);
  fprintf(stderr, "pk_add_frontend_ai tests passed\n");

  /* Test pk_add_kite */
  for (i = 0; i < MIN_KITE_ALLOC; i++)
    assert(NULL != pkm_add_kite(m, "http", "foo", 80, "sec", "localhost", 80));
  assert(NULL == pkm_add_kite(m, "http", "foo", 80, "sec", "localhost", 80));
  assert(ERR_NO_MORE_KITES == pk_error);
  assert(NULL != pkm_find_kite(m, "http", "foo", 80));
  assert(NULL == pkm_find_kite(m, "http", "bar", 80));
  fprintf(stderr, "pk_add_kite tests passed\n");

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
  fprintf(stderr, "pk_*_be_conn tests passed\n");

  /* Cleanup */
  pkm_manager_free(m);
#endif
  return 1;
}
