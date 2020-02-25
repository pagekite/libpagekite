/******************************************************************************
pklogging.c - Logging.

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

#ifdef ANDROID
#include <android/log.h>
#endif

#include "pkutils.h"
#include "pkhooks.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"


FILE* PK_DISABLE_LOGGING = (FILE*) "Disabled";
static unsigned int logged_lines = 0;
static unsigned int logged_errors = 0;

int pk_log(int level, const char* fmt, ...)
{
  va_list args;
  char output[4000];
  int r, len;
  FILE* log_file;

  if (level & pk_state.log_mask) {
#ifdef _MSC_VER
    len = sprintf(output, "ts=%x; ll=%x; msg=",
                          (int) time(0), logged_lines++);
#else
    struct timeval t;
    char tsbuf[30];
# ifdef HAVE_DS_LOG_FORMAT
    gettimeofday(&t, NULL);
    strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%d %H:%M:%S", localtime(&t.tv_sec));
    len = snprintf(output, 4000, "[%s.%03d][%x] ",
                           tsbuf, (int)t.tv_usec / 1000, (int) pthread_self());
# else
    if (log_file != NULL) {
      gettimeofday(&t, NULL);
      strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%d %H:%M:%S", localtime(&t.tv_sec));
      len = sprintf(output, "t=%s.%03d; ts=%x; tid=%x; ll=%x; msg=",
                            tsbuf, (int)t.tv_usec / 1000,
                            (int) time(0), (int) pthread_self(),
                            logged_lines++);
    }
    else {
      /* For syslog, we omit the times, syslog handles that. */
      len = sprintf(output, "tid=%x; ll=%x; msg=",
                            (int) pthread_self(), logged_lines++);
    }
# endif
#endif
    va_start(args, fmt);
    len += (r = vsnprintf(output + len, 4000 - len, fmt, args));
    va_end(args);

    if ((r > 0) && PK_HOOK(PK_HOOK_LOG, len, output, NULL)) {
      if (!(level & PK_LOG_TRACE)) pke_post_event(NULL, PK_EV_LOGGING, len, output);
      pks_logcopy(output, len);
      log_file = pk_state.log_file; /* Avoid race conditions if it changes. */
      if (log_file != NULL) {
#ifdef ANDROID
#warning Default logging uses __android_log_print instead of stderr.
        if (log_file == stderr) {
          __android_log_print(ANDROID_LOG_INFO,
                              "libpagekite", "%.4000s\n", output);
        } else
#endif
        if (log_file != PK_DISABLE_LOGGING) {
          fprintf(log_file, "%.4000s\n", output);
          fflush(log_file);
        }
#ifdef HAVE_SYSLOG_H
      } else {
        int priority = (level & PK_LOG_ERRORS) ? LOG_ERR : LOG_NOTICE;
        syslog(priority, "%.4000s\n", output);
#endif
      }
    }
  }
  else {
    r = 0;
  }

  if (pk_state.bail_on_errors) {
    if (level & PK_LOG_ERRORS) {
      logged_errors += 10;
      if (logged_errors > 10*pk_state.bail_on_errors)
        exit(100);
      if (logged_errors > 9*pk_state.bail_on_errors)
        pk_state.log_mask = PK_LOG_ALL;
    }
    else if (level & PK_LOG_NORMAL) {
      if (logged_errors) logged_errors -= 1;
    }
  }

  return r;
}

void pk_log_raw_data(int level, char* prefix, int fd, void* data, size_t bytes) {
  char buffer[160];
  int printed = 0;
  char* p = (char *) data;

  while (printed < bytes) {
    printed += printable_binary(buffer, 160, p + printed, bytes - printed);
    pk_log(level, "%s/%d(%d/%d): %s", prefix, fd, printed, bytes, buffer);
  }
}

int pk_log_chunk(struct pk_tunnel* fe, struct pk_chunk* chnk) {
  int i;
  int r = 0;
  char fe_ip[1024];
  if (fe != NULL && fe->ai.ai_addr != NULL) {
    in_addr_to_str(fe->ai.ai_addr, fe_ip, 1024);
  }
  else {
    strcpy(fe_ip, "(unknown)");
  }
  if (chnk->ping) {
    r += pk_log(PK_LOG_TUNNEL_HEADERS, "PING from %s", fe_ip);
  }
  else if (chnk->sid) {
    if (chnk->noop) {
      r += pk_log(PK_LOG_TUNNEL_DATA,
                  "[sid=%s] NOOP: (eof:%d skb:%d spd:%d) from %s",
                  chnk->sid, chnk->eof,
                  chnk->remote_sent_kb, chnk->throttle_spd, fe_ip);
    }
    else if (chnk->eof) {
      r += pk_log(PK_LOG_TUNNEL_DATA, "[sid=%s] EOF: %s via %s",
                                      chnk->sid, chnk->eof, fe_ip);
    }
    else {
      if (chnk->request_host) {
        r += pk_log(PK_LOG_TUNNEL_CONNS,
                    "[%s]:%d requested %s://%s:%d%s [sid=%s] via %s",
                    chnk->remote_ip, chnk->remote_port,
                    chnk->request_proto, chnk->request_host, chnk->request_port,
                    chnk->remote_tls ? " (TLS to relay)" : "", chnk->sid, fe_ip);
      }
      r += pk_log(PK_LOG_TUNNEL_DATA, "[sid=%s] DATA: %d bytes via %s",
                                      chnk->sid, chnk->length, fe_ip);
    }
  }
  else if (chnk->noop) {
    r += pk_log(PK_LOG_TUNNEL_HEADERS, "Received NOOP from %s", fe_ip);
  }
  else {
    r += pk_log(PK_LOG_TUNNEL_HEADERS, "Weird: Non-ping chnk with no SID from %s",
                                       fe_ip);
  }
  for (i = 0; i < chnk->header_count; i++) {
    r += pk_log(PK_LOG_TUNNEL_HEADERS, "Header: %s", chnk->headers[i]);
  }
  return r;
}


void pk_dump_parser(char* prefix, struct pk_parser* p)
{
  int i;

  pk_log(PK_LOG_MANAGER_DEBUG, "%s/buffer_bytes_left: %d", prefix, p->buffer_bytes_left);
  if (NULL == p->chunk) return;

  for (i = 0; i < p->chunk->header_count; i++) {
    pk_log(PK_LOG_MANAGER_DEBUG, "%s/chunk/header_%d: %s", prefix, i, p->chunk->headers[i]);
  }
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/chunk/length: %d", prefix, p->chunk->length);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/chunk/length: %d", prefix, p->chunk->length);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/chunk/frame/length: %d", prefix, p->chunk->frame.length);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/chunk/frame/hdr_length: %d", prefix, p->chunk->frame.hdr_length);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/chunk/frame/raw_length: %d", prefix, p->chunk->frame.raw_length);
}

void pk_dump_conn(char* prefix, struct pk_conn* conn)
{
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/status: %8.8x", prefix, conn->status);
  if (conn->sockfd < 0) return;

  pk_log(PK_LOG_MANAGER_DEBUG, "%s/sockfd: %d", prefix, conn->sockfd);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/activity: %x (%ds ago)", prefix,
                               conn->activity,
                               pk_time(0) - conn->activity);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/read_bytes: %d", prefix, conn->read_bytes);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/read_kb: %d", prefix, conn->read_kb);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/sent_kb: %d", prefix, conn->sent_kb);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/send_window_kb: %d", prefix, conn->send_window_kb);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/wrote_bytes: %d", prefix, conn->wrote_bytes);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/reported_kb: %d", prefix, conn->reported_kb);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/in_buffer_pos: %d", prefix, conn->in_buffer_pos);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/out_buffer_pos: %d", prefix, conn->out_buffer_pos);
}

void pk_dump_tunnel(char* prefix, struct pk_tunnel* fe)
{
  char tmp[1024];
  if (NULL == fe->ai.ai_addr) return;

  pk_log(PK_LOG_MANAGER_DEBUG, "%s/fe_hostname: %s", prefix, fe->fe_hostname);
  pk_log(PK_LOG_MANAGER_DEBUG, "%s/fe_port: %d", prefix, fe->fe_port);

  if (0 <= fe->conn.sockfd) {
    pk_log(PK_LOG_MANAGER_DEBUG, "%s/fe_session: %s", prefix, fe->fe_session);
    pk_log(PK_LOG_MANAGER_DEBUG, "%s/request_count: %d", prefix, fe->request_count);
    in_addr_to_str(fe->ai.ai_addr, tmp, 1024);
    pk_log(PK_LOG_MANAGER_DEBUG, "%s/fe_ai: %s", prefix, tmp);
    sprintf(tmp, "%s/conn", prefix);
    pk_dump_conn(tmp, &(fe->conn));
    sprintf(tmp, "%s/parser", prefix);
    pk_dump_parser(tmp, fe->parser);
  }
}

void pk_dump_be_conn(char* prefix, struct pk_backend_conn* bec)
{
  char tmp[1024];
  if (!bec) return;

  #define LL PK_LOG_MANAGER_DEBUG
  if (bec->tunnel)
    pk_log(LL, "%s/fe: %s", prefix, bec->tunnel->fe_hostname);

  if (bec->kite)
    pk_log(LL, "%s/kite: %d <- %s://%s", prefix, bec->kite->local_port,
                                                 bec->kite->protocol,
                                                 bec->kite->public_domain);

  sprintf(tmp, "%s/conn", prefix);
  pk_dump_conn(tmp, &(bec->conn));
}

void pk_dump_state(struct pk_manager* pkm)
{
  int i;
  char prefix[1024];
  struct pk_tunnel* fe;
  struct pk_backend_conn* bec;

  #define LL PK_LOG_MANAGER_DEBUG
  pk_log(LL, "pk_global_state/app_id_short: %s", pk_state.app_id_short);
  pk_log(LL, "pk_global_state/app_id_long: %s", pk_state.app_id_long);
  pk_log(LL, "pk_global_state/use_ipv4: %d", pk_state.use_ipv4);
#ifdef HAVE_IPV6
  pk_log(LL, "pk_global_state/use_ipv6: %d", pk_state.use_ipv6);
#endif
  pk_log(LL, "pk_global_state/have_ssl: %d", pk_state.have_ssl);
  pk_log(LL, "pk_global_state/live_streams: %d", pk_state.live_streams);
  pk_log(LL, "pk_global_state/live_tunnels: %d", pk_state.live_tunnels);
  pk_log(LL, "pk_manager/status: %d", pkm->status);
  pk_log(LL, "pk_manager/buffer_bytes_free: %d", pkm->buffer_bytes_free);
  pk_log(LL, "pk_manager/kite_max: %d", pkm->kite_max);
  pk_log(LL, "pk_manager/tunnel_max: %d", pkm->tunnel_max);
  pk_log(LL, "pk_manager/be_conn_max: %d", pkm->be_conn_max);
  pk_log(LL, "pk_manager/last_world_update: %x", pkm->last_world_update);
  pk_log(LL, "pk_manager/next_tick: %d", pkm->next_tick);
  pk_log(LL, "pk_manager/enable_timer: %d", 0 < pkm->enable_timer);
  pk_log(LL, "pk_manager/fancy_pagekite_net_rejection_url: %s", pkm->fancy_pagekite_net_rejection_url);
  pk_log(LL, "pk_manager/want_spare_frontends: %d", pkm->want_spare_frontends);
  pk_log(LL, "pk_manager/dynamic_dns_url: %s", pkm->dynamic_dns_url);

  for (i = 0, fe = pkm->tunnels; i < pkm->tunnel_max; i++, fe++) {
    sprintf(prefix, "fe_%d", i);
    pk_dump_tunnel(prefix, fe);
  }
  for (i = 0, bec = pkm->be_conns; i < pkm->be_conn_max; i++, bec++) {
    sprintf(prefix, "beconn_%d", i);
    pk_dump_be_conn(prefix, bec);
  }
}

