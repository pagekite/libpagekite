/******************************************************************************
pagekite.c - High level library interface

*******************************************************************************

This file is Copyright 2012-2013, The Beanstalks Project ehf.

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

#include "pagekite.h"

#include "common.h"
#include "utils.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"

#define PK_DEFAULT_FLAGS (PK_WITH_SSL | PK_WITH_IPV4 | PK_WITH_IPV6)


static struct pk_manager* PK_MANAGER(pagekite_mgr pkm) {
  return (struct pk_manager*) pkm;
}


pagekite_mgr pagekite_init(
  const char* app_id,
  int max_kites,
  int max_frontends,
  int max_conns,
  const char* dyndns_url,
  int flags,
  int verbosity)
{
  SSL_CTX* ssl_ctx;
  struct pk_manager *pkm_;

  if (flags == PK_WITH_DEFAULTS) flags = PK_DEFAULT_FLAGS;

#ifdef _MSC_VER
  WSADATA wsa_data;
  int r;
  if (0 != (r = WSAStartup(MAKEWORD(2, 2), &wsa_data))) {
    pk_log(PK_LOG_ERROR, "libpagekite: Error during WSAStartup: %d\n", r);
    return pk_err_null(ERR_WSA_STARTUP);
  }
#endif

  pks_global_init(PK_LOG_NORMAL);
  if (verbosity < 0x100) {
    pk_state.log_mask = ((verbosity < 0) ? PK_LOG_ERRORS :
                        ((verbosity < 1) ? PK_LOG_NORMAL :
                        ((verbosity < 2) ? PK_LOG_DEBUG : PK_LOG_ALL)));
  }
  else {
    pk_state.log_mask = verbosity;
  }

  if (flags & PK_WITH_SSL) {
    PKS_SSL_INIT(ssl_ctx);
  }
  else {
    ssl_ctx = NULL;
  }

  /* Note: This will leak memory if used. */
  if (app_id != NULL) pk_state.app_id_long = strdup(app_id);

  if (NULL == (pkm_ = pkm_manager_init(NULL, 0, NULL,
                                       max_kites,
                                       max_frontends,
                                       max_conns,
                                       dyndns_url,
                                       ssl_ctx))) {
    return NULL;
  }

  if (flags & PK_WITH_SERVICE_FRONTENDS) {
    if (0 > pagekite_add_service_frontends((pagekite_mgr) pkm_, flags)) {
      pkm_manager_free(pkm_);
      return NULL;
    }
  }

  pkm_set_timer_enabled(pkm_, 1);
  pkm_tick(pkm_);

  return (pagekite_mgr) pkm_;
}

pagekite_mgr pagekite_init_pagekitenet(
  const char* app_id,
  int max_kites,
  int max_conns,
  int flags,
  int verbosity)
{
  pagekite_mgr pkm;

  if (flags == PK_WITH_DEFAULTS) flags = PK_DEFAULT_FLAGS;

  if (NULL == (pkm = pagekite_init(app_id,
                                   max_kites,
                                   PAGEKITE_NET_FE_MAX,
                                   max_conns,
                                   PAGEKITE_NET_DDNS,
                                   flags, verbosity)))
  {
    return NULL;
  }

  /* If flags say anything about service frontends, do nothing: either
     it was already done, or we're not supposed to do anything. */
  if (!(flags & (PK_WITHOUT_SERVICE_FRONTENDS|PK_WITH_SERVICE_FRONTENDS))) {
    if (0 > pagekite_add_service_frontends(pkm, flags)) {
      pkm_manager_free(PK_MANAGER(pkm));
      return NULL;
    }
  }

  return pkm;
}

int pagekite_add_service_frontends(pagekite_mgr pkm, int flags) {
  int fes_v4 = 0;
#ifdef HAVE_IPV6
  int fes_v6 = 0;
#endif

  if (flags == PK_WITH_DEFAULTS) flags = PK_DEFAULT_FLAGS;

  if (((flags & PK_WITH_IPV4) &&
       (0 > (fes_v4 = pkm_add_frontend(PK_MANAGER(pkm),
                                       PAGEKITE_NET_V4FRONTENDS,
                                       FE_STATUS_AUTO))))
#ifdef HAVE_IPV6
         ||
       ((flags & PK_WITH_IPV4) &&
        (0 > (fes_v6 = pkm_add_frontend(PK_MANAGER(pkm),
                                        PAGEKITE_NET_V6FRONTENDS,
                                        FE_STATUS_AUTO))))
#endif
  ) {
    return -1;
  }

#ifdef HAVE_IPV6
  int fes = fes_v4 + fes_v6;
#else
  int fes = fes_v4;
#endif
  if (0 == fes) {
    pk_set_error(ERR_NO_FRONTENDS);
    return -1;
  }
  return fes;
}

int pagekite_free(pagekite_mgr pkm) {
  if (pkm == NULL) return -1;
  pkm_manager_free(PK_MANAGER(pkm));
#ifdef _MSC_VER
  Sleep(100); /* Give logger time to get the rest of the log for debugging */
  WSACleanup();
#endif
  return 0;
}

int pagekite_set_log_mask(pagekite_mgr pkm, int mask)
{
  (void) pkm;
  pk_state.log_mask = mask;
  return 0;
}

int pagekite_enable_watchdog(pagekite_mgr pkm, int enable)
{
  if (pkm == NULL) return -1;
  PK_MANAGER(pkm)->enable_watchdog = (enable > 0);
  return 0;
}

int pagekite_enable_fake_ping(pagekite_mgr pkm, int enable)
{
  (void) pkm;
  pk_state.fake_ping = enable;
  return 0;
}

int pagekite_set_bail_on_errors(pagekite_mgr pkm, int errors)
{
  (void) pkm;
  pk_state.bail_on_errors = errors;
  return 0;
}

int pagekite_set_conn_eviction_idle_s(pagekite_mgr pkm, int seconds)
{
  (void) pkm;
  pk_state.conn_eviction_idle_s = seconds;
  return 0;
}

int pagekite_want_spare_frontends(pagekite_mgr pkm, int spares)
{
  if (pkm == NULL) return -1;
  PK_MANAGER(pkm)->want_spare_frontends = spares;
  return 0;
}


int pagekite_add_kite(pagekite_mgr pkm,
  const char* proto,
  const char* kitename,
  int pport, 
  const char* secret,
  const char* backend,
  int lport) 
{
  if (pkm == NULL) return -1;
  return (NULL != pkm_add_kite(PK_MANAGER(pkm),
                               proto, kitename, pport, secret, backend, lport)
          ) ? 0 : -1;
}

int pagekite_add_frontend(pagekite_mgr pkm,
  const char* domain,
  int port)
{
  if (pkm == NULL) return -1;
  return pkm_add_frontend(PK_MANAGER(pkm), domain, port, FE_STATUS_AUTO);
}

int pagekite_tick(pagekite_mgr pkm)
{
  if (pkm == NULL) return -1;
  pkm_tick(PK_MANAGER(pkm));
  return 0;
}

int pagekite_poll(pagekite_mgr pkm, int timeout) {
  if (pkm == NULL) return -1;
  pthread_mutex_lock(&(pk_state.lock));
  /* FIXME: Obey the timeout */
  pthread_cond_wait(&(pk_state.cond), &(pk_state.lock));
  pthread_mutex_unlock(&(pk_state.lock));
  return 0;
}

int pagekite_start(pagekite_mgr pkm) {
  if (pkm == NULL) return -1;
  return pkm_run_in_thread(PK_MANAGER(pkm));
}

int pagekite_wait(pagekite_mgr pkm) {
  if (pkm == NULL) return -1;
  return pkm_wait_thread(PK_MANAGER(pkm));
}

int pagekite_stop(pagekite_mgr pkm) {
  if (pkm == NULL) return -1;
  if (0 > pkm_stop_thread(PK_MANAGER(pkm))) return -1;
  return 0;
}

int pagekite_get_status(pagekite_mgr pkm) {
  if (pkm == NULL) return -1;
  return PK_MANAGER(pkm)->status;
}

void pagekite_perror(pagekite_mgr pkm, const char* prefix) {
  (void) pkm;
  pk_perror(prefix);
}

char* pagekite_get_log(pagekite_mgr pkm) {
  static char buffer[PKS_LOG_DATA_MAX+1];
  if (pkm == NULL) {
    strcpy(buffer, "Not running.");
  }
  else {
    pks_copylog(buffer);
  }
  buffer[PKS_LOG_DATA_MAX] = '\0';
  return buffer;
}
